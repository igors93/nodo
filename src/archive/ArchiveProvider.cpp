#include "archive/ArchiveProvider.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "archive/ArchiveSignature.hpp"
#include "crypto/AddressDerivation.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "utils/SafeScalar.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::archive {

namespace {

constexpr const char *kSignaturePurpose = "ARCHIVE/REGISTRATION";

bool isSafeId(const std::string &value) {
  return utils::isSafeIdentifier(value, 128, "_-.:");
}

} // namespace

ArchiveProviderRegistration::ArchiveProviderRegistration()
    : m_fields(), m_signature() {}

ArchiveProviderRegistration::ArchiveProviderRegistration(
    ArchiveProviderRegistrationFields fields, crypto::Signature signature)
    : m_fields(std::move(fields)), m_signature(std::move(signature)) {}

ArchiveProviderRegistration
ArchiveProviderRegistration::sign(ArchiveProviderRegistrationFields fields,
                                  const crypto::KeyPair &key,
                                  std::int64_t signedAt) {
  fields.publicKeyHex = key.publicKey().keyMaterial();
  fields.providerId = key.address().value();
  ArchiveProviderRegistration unsignedRegistration(std::move(fields),
                                                   crypto::Signature());
  const crypto::Signature signature = ArchiveSignature::sign(
      unsignedRegistration.encodeBody(), kSignaturePurpose, key, signedAt);
  return ArchiveProviderRegistration(unsignedRegistration.fields(), signature);
}

const ArchiveProviderRegistrationFields &
ArchiveProviderRegistration::fields() const {
  return m_fields;
}

const crypto::Signature &ArchiveProviderRegistration::signature() const {
  return m_signature;
}

bool ArchiveProviderRegistration::isStructurallyValid() const {
  const ArchiveProviderRegistrationFields &f = m_fields;
  if (!utils::isSafeIdentifier(f.chainId, 128, "_-.") ||
      !isSafeId(f.providerId) || !isSafeId(f.operatorId) ||
      f.publicKeyHex.size() != 64 ||
      f.publicKeyHex.find_first_not_of("0123456789abcdef") != std::string::npos ||
      f.bondRawUnits == 0 ||
      f.bondRawUnits >
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
      f.declaredCapacityBytes == 0 || f.registeredHeight == 0) {
    return false;
  }
  try {
    const crypto::PublicKey publicKey(crypto::CryptoAlgorithm::CLASSIC_ED25519,
                                      f.publicKeyHex);
    return crypto::AddressDerivation::deriveFromPublicKey(publicKey).value() ==
           f.providerId;
  } catch (const std::exception &) {
    return false;
  }
}

bool ArchiveProviderRegistration::verifySignature() const {
  return isStructurallyValid() &&
         ArchiveSignature::verify(encodeBody(), kSignaturePurpose, m_signature,
                                  m_fields.publicKeyHex);
}

std::uint64_t ArchiveProviderRegistration::maxSlots(
    const config::HistoryParameters &parameters) const {
  return parameters.isValid()
             ? m_fields.bondRawUnits / parameters.archiveMinBondPerSlotRawUnits()
             : 0;
}

std::vector<unsigned char> ArchiveProviderRegistration::encodeBody() const {
  const ArchiveProviderRegistrationFields &f = m_fields;
  serialization::CanonicalWriter writer;
  writer.writeString(SCHEMA);
  writer.writeString(f.chainId);
  writer.writeString(f.providerId);
  writer.writeString(f.publicKeyHex);
  writer.writeString(f.operatorId);
  writer.writeUInt64(f.bondRawUnits);
  writer.writeUInt64(f.declaredCapacityBytes);
  writer.writeUInt64(f.registeredHeight);
  return writer.bytes();
}

std::string ArchiveProviderRegistration::registrationId() const {
  return hashHex("ARCHIVE/REGISTRATION-ID", encodeBody());
}

std::vector<unsigned char> ArchiveProviderRegistration::encode() const {
  serialization::CanonicalWriter writer;
  writer.writeBytes(encodeBody());
  ArchiveSignature::write(writer, m_signature);
  return writer.bytes();
}

ArchiveProviderRegistration
ArchiveProviderRegistration::decode(const std::vector<unsigned char> &bytes) {
  if (bytes.empty() || bytes.size() > kMaxEncodedBytes) {
    throw std::invalid_argument("Provider registration size is out of range.");
  }
  serialization::CanonicalReader outer(bytes, kMaxEncodedBytes);
  const std::vector<unsigned char> body = outer.readBytes();
  const crypto::Signature signature = ArchiveSignature::read(outer);
  outer.requireFullyConsumed();

  serialization::CanonicalReader reader(body, 256);
  if (reader.readString() != SCHEMA) {
    throw std::invalid_argument("Unknown provider registration schema.");
  }
  ArchiveProviderRegistrationFields fields;
  fields.chainId = reader.readString();
  fields.providerId = reader.readString();
  fields.publicKeyHex = reader.readString();
  fields.operatorId = reader.readString();
  fields.bondRawUnits = reader.readUInt64();
  fields.declaredCapacityBytes = reader.readUInt64();
  fields.registeredHeight = reader.readUInt64();
  reader.requireFullyConsumed();
  ArchiveProviderRegistration registration(std::move(fields), signature);
  if (!registration.isStructurallyValid() || registration.encode() != bytes) {
    throw std::invalid_argument("Provider registration is malformed.");
  }
  return registration;
}

std::string archiveProviderStatusToString(ArchiveProviderStatus status) {
  switch (status) {
  case ArchiveProviderStatus::PENDING:
    return "PENDING";
  case ArchiveProviderStatus::ACTIVE:
    return "ACTIVE";
  case ArchiveProviderStatus::REMOVED:
    return "REMOVED";
  }
  return "REMOVED";
}

std::string
archiveRegistrationStatusToString(ArchiveRegistrationStatus status) {
  switch (status) {
  case ArchiveRegistrationStatus::REGISTERED:
    return "REGISTERED";
  case ArchiveRegistrationStatus::MALFORMED:
    return "MALFORMED";
  case ArchiveRegistrationStatus::BAD_SIGNATURE:
    return "BAD_SIGNATURE";
  case ArchiveRegistrationStatus::WRONG_CHAIN:
    return "WRONG_CHAIN";
  case ArchiveRegistrationStatus::INSUFFICIENT_BOND:
    return "INSUFFICIENT_BOND";
  case ArchiveRegistrationStatus::DUPLICATE_PROVIDER:
    return "DUPLICATE_PROVIDER";
  }
  return "MALFORMED";
}

ArchiveProviderRegistry::ArchiveProviderRegistry(std::string chainId)
    : m_chainId(std::move(chainId)), m_records() {}

ArchiveRegistrationStatus ArchiveProviderRegistry::registerProvider(
    const ArchiveProviderRegistration &registration,
    const config::HistoryParameters &parameters) {
  if (!registration.isStructurallyValid() || !parameters.isValid()) {
    return ArchiveRegistrationStatus::MALFORMED;
  }
  if (!registration.verifySignature()) {
    return ArchiveRegistrationStatus::BAD_SIGNATURE;
  }
  if (registration.fields().chainId != m_chainId) {
    return ArchiveRegistrationStatus::WRONG_CHAIN;
  }
  if (registration.maxSlots(parameters) == 0) {
    return ArchiveRegistrationStatus::INSUFFICIENT_BOND;
  }
  // A key registers once; a removed provider cannot reset its record.
  if (m_records.count(registration.fields().providerId) != 0) {
    return ArchiveRegistrationStatus::DUPLICATE_PROVIDER;
  }
  const std::uint64_t height = registration.fields().registeredHeight;
  const std::uint64_t delay = parameters.archiveProviderActivationDelayBlocks();
  if (height > std::numeric_limits<std::uint64_t>::max() - delay) {
    return ArchiveRegistrationStatus::MALFORMED;
  }
  ArchiveProviderRecord record;
  record.registration = registration;
  record.status = ArchiveProviderStatus::PENDING;
  record.activationHeight = height + delay;
  m_records.emplace(registration.fields().providerId, std::move(record));
  return ArchiveRegistrationStatus::REGISTERED;
}

std::size_t ArchiveProviderRegistry::activateAt(std::uint64_t height,
                                                const std::string &blockHash) {
  if (!isDigestHex(blockHash)) {
    throw std::invalid_argument("Activation needs a finalized block hash.");
  }
  std::size_t activated = 0;
  for (auto &[providerId, record] : m_records) {
    if (record.status != ArchiveProviderStatus::PENDING ||
        record.activationHeight != height) {
      continue;
    }
    serialization::CanonicalWriter writer;
    writer.writeString(providerId);
    writer.writeString(blockHash);
    record.assignmentKey = hashHex("ARCHIVE/PROVIDER-KEY", writer.bytes());
    record.status = ArchiveProviderStatus::ACTIVE;
    ++activated;
  }
  return activated;
}

bool ArchiveProviderRegistry::remove(const std::string &providerId,
                                     std::uint64_t height,
                                     const std::string &reason) {
  ArchiveProviderRecord *record = find(providerId);
  if (record == nullptr || record->status == ArchiveProviderStatus::REMOVED) {
    return false;
  }
  record->status = ArchiveProviderStatus::REMOVED;
  record->removedAtHeight = height;
  record->removalReason = reason;
  return true;
}

ArchiveProviderRecord *ArchiveProviderRegistry::find(const std::string &providerId) {
  const auto found = m_records.find(providerId);
  return found == m_records.end() ? nullptr : &found->second;
}

const ArchiveProviderRecord *
ArchiveProviderRegistry::find(const std::string &providerId) const {
  const auto found = m_records.find(providerId);
  return found == m_records.end() ? nullptr : &found->second;
}

std::vector<const ArchiveProviderRecord *>
ArchiveProviderRegistry::activeProviders() const {
  std::vector<const ArchiveProviderRecord *> active;
  for (const auto &[providerId, record] : m_records) {
    (void)providerId;
    if (record.status == ArchiveProviderStatus::ACTIVE) {
      active.push_back(&record);
    }
  }
  return active;
}

std::size_t ArchiveProviderRegistry::size() const { return m_records.size(); }

const std::string &ArchiveProviderRegistry::chainId() const { return m_chainId; }

std::string ArchiveProviderRegistry::digest() const {
  serialization::CanonicalWriter writer;
  writer.writeString(m_chainId);
  writer.writeUInt32(static_cast<std::uint32_t>(m_records.size()));
  for (const auto &[providerId, record] : m_records) {
    writer.writeString(providerId);
    writer.writeString(record.registration.registrationId());
    writer.writeString(archiveProviderStatusToString(record.status));
    writer.writeUInt64(record.activationHeight);
    writer.writeString(record.assignmentKey);
    writer.writeUInt32(record.successfulEpochStreak);
    writer.writeUInt32(record.consecutiveFailedEpochs);
    writer.writeUInt64(record.removedAtHeight);
    writer.writeString(record.removalReason);
  }
  return hashHex("ARCHIVE/REGISTRY", writer.bytes());
}

} // namespace nodo::archive
