#include "storage/StorageSchemaVersion.hpp"

#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

namespace {

constexpr const char *STORAGE_SCHEMA_FILE_VERSION =
    "NODO_STORAGE_SCHEMA_VERSION_V1";

bool isSafeScalar(const std::string &value) {
  if (value.empty() || value.size() > 160) {
    return false;
  }

  for (const char character : value) {
    const bool allowed = (character >= 'a' && character <= 'z') ||
                         (character >= 'A' && character <= 'Z') ||
                         (character >= '0' && character <= '9') ||
                         character == '_' || character == '-' ||
                         character == '.';

    if (!allowed) {
      return false;
    }
  }

  return true;
}

std::uint64_t parseU64Strict(const std::map<std::string, std::string> &fields,
                             const std::string &key) {
  const auto found = fields.find(key);

  if (found == fields.end()) {
    throw std::invalid_argument("Storage schema field missing: " + key);
  }

  if (found->second.empty()) {
    throw std::invalid_argument("Storage schema numeric field is empty: " +
                                key);
  }

  for (const char current : found->second) {
    if (current < '0' || current > '9') {
      throw std::invalid_argument(
          "Storage schema numeric field is malformed: " + key);
    }
  }

  std::size_t parsedSize = 0;
  const std::uint64_t parsed =
      static_cast<std::uint64_t>(std::stoull(found->second, &parsedSize));

  if (parsedSize != found->second.size()) {
    throw std::invalid_argument("Storage schema numeric field is malformed: " +
                                key);
  }

  return parsed;
}

std::string parseStringStrict(const std::map<std::string, std::string> &fields,
                              const std::string &key) {
  const auto found = fields.find(key);

  if (found == fields.end()) {
    throw std::invalid_argument("Storage schema field missing: " + key);
  }

  if (!isSafeScalar(found->second)) {
    throw std::invalid_argument("Storage schema scalar field is unsafe: " +
                                key);
  }

  return found->second;
}

} // namespace

std::string
storageSchemaValidationStatusToString(StorageSchemaValidationStatus status) {
  switch (status) {
  case StorageSchemaValidationStatus::ACCEPTED:
    return "ACCEPTED";
  case StorageSchemaValidationStatus::MISSING_VERSION_FILE:
    return "MISSING_VERSION_FILE";
  case StorageSchemaValidationStatus::UNSUPPORTED_SCHEMA:
    return "UNSUPPORTED_SCHEMA";
  case StorageSchemaValidationStatus::UNSUPPORTED_VERSION:
    return "UNSUPPORTED_VERSION";
  case StorageSchemaValidationStatus::DOWNGRADE_REJECTED:
    return "DOWNGRADE_REJECTED";
  case StorageSchemaValidationStatus::NON_CANONICAL:
    return "NON_CANONICAL";
  case StorageSchemaValidationStatus::IO_ERROR:
    return "IO_ERROR";
  default:
    return "IO_ERROR";
  }
}

StorageSchemaVersion::StorageSchemaVersion()
    : m_schemaId(""), m_version(0), m_minimumCompatibleVersion(0) {}

StorageSchemaVersion::StorageSchemaVersion(
    std::string schemaId, std::uint64_t version,
    std::uint64_t minimumCompatibleVersion)
    : m_schemaId(std::move(schemaId)), m_version(version),
      m_minimumCompatibleVersion(minimumCompatibleVersion) {}

const std::string &StorageSchemaVersion::schemaId() const { return m_schemaId; }

std::uint64_t StorageSchemaVersion::version() const { return m_version; }

std::uint64_t StorageSchemaVersion::minimumCompatibleVersion() const {
  return m_minimumCompatibleVersion;
}

bool StorageSchemaVersion::isStructurallyValid() const {
  return isSafeScalar(m_schemaId) && m_version > 0 &&
         m_minimumCompatibleVersion > 0 &&
         m_minimumCompatibleVersion <= m_version;
}

bool StorageSchemaVersion::isSupportedNodeDataDirectoryVersion() const {
  return isStructurallyValid() && m_schemaId == nodeDataDirectorySchemaId() &&
         m_version >= minimumSupportedNodeDataDirectoryVersion() &&
         m_version <= currentNodeDataDirectoryVersion() &&
         m_minimumCompatibleVersion <= currentNodeDataDirectoryVersion();
}

std::string StorageSchemaVersion::serialize() const {
  std::ostringstream oss;

  oss << "StorageSchemaVersion{"
      << "schemaId=" << m_schemaId << ";version=" << m_version
      << ";minimumCompatibleVersion=" << m_minimumCompatibleVersion << "}";

  return oss.str();
}

std::string StorageSchemaVersion::toFileContents() const {
  if (!isStructurallyValid()) {
    throw std::invalid_argument(
        "Invalid storage schema version cannot be serialized.");
  }

  return serialization::KeyValueFileCodec::serialize(
      STORAGE_SCHEMA_FILE_VERSION,
      {{"schemaId", m_schemaId},
       {"version", std::to_string(m_version)},
       {"minimumCompatibleVersion",
        std::to_string(m_minimumCompatibleVersion)}});
}

std::string StorageSchemaVersion::schemaFileName() {
  return "storage_schema.nodo";
}

std::string StorageSchemaVersion::nodeDataDirectorySchemaId() {
  return "NODO_NODE_DATA_DIRECTORY";
}

std::uint64_t StorageSchemaVersion::currentNodeDataDirectoryVersion() {
  return 1;
}

std::uint64_t StorageSchemaVersion::minimumSupportedNodeDataDirectoryVersion() {
  return 1;
}

StorageSchemaVersion StorageSchemaVersion::currentNodeDataDirectorySchema() {
  return StorageSchemaVersion(nodeDataDirectorySchemaId(),
                              currentNodeDataDirectoryVersion(),
                              minimumSupportedNodeDataDirectoryVersion());
}

StorageSchemaVersion
StorageSchemaVersion::fromFileContents(const std::string &contents) {
  const serialization::KeyValueFileDocument document =
      serialization::KeyValueFileCodec::parse(contents,
                                              STORAGE_SCHEMA_FILE_VERSION);

  document.requireOnlyFields(
      {"schemaId", "version", "minimumCompatibleVersion"});

  const std::map<std::string, std::string> fields = document.fields();

  StorageSchemaVersion schema(
      parseStringStrict(fields, "schemaId"), parseU64Strict(fields, "version"),
      parseU64Strict(fields, "minimumCompatibleVersion"));

  if (!schema.isStructurallyValid()) {
    throw std::invalid_argument("Parsed storage schema version is invalid.");
  }

  if (schema.toFileContents() != contents) {
    throw std::invalid_argument(
        "Storage schema version file is not canonical.");
  }

  return schema;
}

void StorageSchemaVersion::writeCurrentNodeDataDirectoryVersionFile(
    const std::filesystem::path &rootPath) {
  if (rootPath.empty()) {
    throw std::invalid_argument("Storage schema root path cannot be empty.");
  }

  std::filesystem::create_directories(rootPath);

  AtomicFile::writeTextFile(StorageSchemaVersionFile::pathForRoot(rootPath),
                            currentNodeDataDirectorySchema().toFileContents());
}

StorageSchemaValidationResult::StorageSchemaValidationResult()
    : m_status(StorageSchemaValidationStatus::IO_ERROR),
      m_reason("Uninitialized storage schema validation result."),
      m_schema(std::nullopt) {}

StorageSchemaValidationResult
StorageSchemaValidationResult::accepted(StorageSchemaVersion schema) {
  StorageSchemaValidationResult result;
  result.m_status = StorageSchemaValidationStatus::ACCEPTED;
  result.m_reason = "";
  result.m_schema = std::move(schema);
  return result;
}

StorageSchemaValidationResult
StorageSchemaValidationResult::rejected(StorageSchemaValidationStatus status,
                                        std::string reason) {
  StorageSchemaValidationResult result;
  result.m_status = status;
  result.m_reason = std::move(reason);
  result.m_schema = std::nullopt;
  return result;
}

StorageSchemaValidationStatus StorageSchemaValidationResult::status() const {
  return m_status;
}

const std::string &StorageSchemaValidationResult::reason() const {
  return m_reason;
}

const std::optional<StorageSchemaVersion> &
StorageSchemaValidationResult::schema() const {
  return m_schema;
}

bool StorageSchemaValidationResult::accepted() const {
  return m_status == StorageSchemaValidationStatus::ACCEPTED &&
         m_schema.has_value() &&
         m_schema->isSupportedNodeDataDirectoryVersion();
}

std::string StorageSchemaValidationResult::serialize() const {
  std::ostringstream oss;

  oss << "StorageSchemaValidationResult{"
      << "status=" << storageSchemaValidationStatusToString(m_status)
      << ";reason=" << m_reason
      << ";schema=" << (m_schema.has_value() ? m_schema->serialize() : "NONE")
      << "}";

  return oss.str();
}

std::filesystem::path
StorageSchemaVersionFile::pathForRoot(const std::filesystem::path &rootPath) {
  if (rootPath.empty()) {
    throw std::invalid_argument("Storage schema root path cannot be empty.");
  }

  return rootPath / StorageSchemaVersion::schemaFileName();
}

StorageSchemaValidationResult
StorageSchemaVersionFile::validateNodeDataDirectoryRoot(
    const std::filesystem::path &rootPath) {
  if (rootPath.empty()) {
    return StorageSchemaValidationResult::rejected(
        StorageSchemaValidationStatus::IO_ERROR,
        "Storage schema root path is empty.");
  }

  const std::filesystem::path schemaPath = pathForRoot(rootPath);

  if (!std::filesystem::exists(schemaPath)) {
    return StorageSchemaValidationResult::rejected(
        StorageSchemaValidationStatus::MISSING_VERSION_FILE,
        "Storage schema version file is missing: " + schemaPath.string());
  }

  try {
    const StorageSchemaVersion schema = StorageSchemaVersion::fromFileContents(
        AtomicFile::readTextFile(schemaPath));

    if (schema.schemaId() !=
        StorageSchemaVersion::nodeDataDirectorySchemaId()) {
      return StorageSchemaValidationResult::rejected(
          StorageSchemaValidationStatus::UNSUPPORTED_SCHEMA,
          "Unsupported storage schema id: " + schema.schemaId());
    }

    if (schema.version() >
            StorageSchemaVersion::currentNodeDataDirectoryVersion() ||
        schema.minimumCompatibleVersion() >
            StorageSchemaVersion::currentNodeDataDirectoryVersion()) {
      return StorageSchemaValidationResult::rejected(
          StorageSchemaValidationStatus::UNSUPPORTED_VERSION,
          "Storage schema version is newer than this runtime supports.");
    }

    if (schema.version() <
        StorageSchemaVersion::minimumSupportedNodeDataDirectoryVersion()) {
      return StorageSchemaValidationResult::rejected(
          StorageSchemaValidationStatus::DOWNGRADE_REJECTED,
          "Storage schema version is older than this runtime can safely load.");
    }

    if (!schema.isSupportedNodeDataDirectoryVersion()) {
      return StorageSchemaValidationResult::rejected(
          StorageSchemaValidationStatus::UNSUPPORTED_VERSION,
          "Storage schema version is unsupported.");
    }

    return StorageSchemaValidationResult::accepted(schema);
  } catch (const std::invalid_argument &error) {
    return StorageSchemaValidationResult::rejected(
        StorageSchemaValidationStatus::NON_CANONICAL, error.what());
  } catch (const std::exception &error) {
    return StorageSchemaValidationResult::rejected(
        StorageSchemaValidationStatus::IO_ERROR, error.what());
  }
}

} // namespace nodo::storage
