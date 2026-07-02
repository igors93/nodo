#include "storage/StorageRecovery.hpp"

#include "storage/AtomicFile.hpp"

#include <sstream>
#include <utility>

namespace nodo::storage {

StorageRecoveryResult::StorageRecoveryResult()
    : m_quarantinedFileCount(0), m_quarantinedFiles() {}

StorageRecoveryResult::StorageRecoveryResult(
    std::size_t quarantinedFileCount, std::vector<std::string> quarantinedFiles)
    : m_quarantinedFileCount(quarantinedFileCount),
      m_quarantinedFiles(std::move(quarantinedFiles)) {}

std::size_t StorageRecoveryResult::quarantinedFileCount() const {
  return m_quarantinedFileCount;
}

const std::vector<std::string> &
StorageRecoveryResult::quarantinedFiles() const {
  return m_quarantinedFiles;
}

bool StorageRecoveryResult::recovered() const {
  return m_quarantinedFileCount > 0;
}

std::string StorageRecoveryResult::serialize() const {
  std::ostringstream output;

  output << "StorageRecoveryResult{"
         << "quarantinedFileCount=" << m_quarantinedFileCount << ";files=[";

  for (std::size_t index = 0; index < m_quarantinedFiles.size(); ++index) {
    if (index != 0) {
      output << ",";
    }

    output << m_quarantinedFiles[index];
  }

  output << "]}";

  return output.str();
}

StorageRecoveryResult StorageRecovery::quarantineTemporaryWrites(
    const std::filesystem::path &rootDirectory) {
  std::vector<std::string> quarantinedFiles;

  if (rootDirectory.empty() || !std::filesystem::exists(rootDirectory)) {
    return StorageRecoveryResult();
  }

  const std::filesystem::path quarantine = quarantineDirectory(rootDirectory);

  std::filesystem::create_directories(quarantine);

  std::vector<std::filesystem::path> candidates;
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(rootDirectory)) {
    if (!entry.is_regular_file() ||
        !AtomicFile::isTemporaryWriteFile(entry.path())) {
      continue;
    }

    if (entry.path().string().find(quarantine.string()) == 0) {
      continue;
    }

    candidates.push_back(entry.path());
  }

  for (const auto &sourcePath : candidates) {
    std::filesystem::path target = quarantine / sourcePath.filename();

    std::size_t suffix = 0;
    while (std::filesystem::exists(target)) {
      ++suffix;
      target = quarantine /
               (sourcePath.filename().string() + "." + std::to_string(suffix));
    }

    std::error_code renameError;
    std::filesystem::rename(sourcePath, target, renameError);

    if (!renameError) {
      quarantinedFiles.push_back(target.string());
    }
  }

  return StorageRecoveryResult(quarantinedFiles.size(), quarantinedFiles);
}

std::filesystem::path StorageRecovery::quarantineDirectory(
    const std::filesystem::path &rootDirectory) {
  return rootDirectory / ".nodo-recovery-quarantine";
}

} // namespace nodo::storage
