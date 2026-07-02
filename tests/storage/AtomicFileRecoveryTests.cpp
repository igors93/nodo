#include "storage/AtomicFile.hpp"
#include "storage/StorageRecovery.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();

  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo_atomic_file_test_" + std::to_string(nonce));

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  const std::filesystem::path file = root / "manifest.nodo";

  nodo::storage::AtomicFile::writeTextFile(file, "version=1\n");

  assert(nodo::storage::AtomicFile::readTextFile(file) == "version=1\n");

  nodo::storage::AtomicFile::writeTextFile(file, "version=2\n");

  assert(nodo::storage::AtomicFile::readTextFile(file) == "version=2\n");

  const std::filesystem::path stale = root / "manifest.nodo.tmp.synthetic";

  nodo::storage::AtomicFile::writeTextFile(stale, "orphan\n");

  assert(nodo::storage::AtomicFile::isTemporaryWriteFile(stale));

  const nodo::storage::StorageRecoveryResult recovery =
      nodo::storage::StorageRecovery::quarantineTemporaryWrites(root);

  assert(recovery.quarantinedFileCount() == 1);
  assert(!std::filesystem::exists(stale));
  assert(std::filesystem::exists(
      nodo::storage::StorageRecovery::quarantineDirectory(root)));

  std::filesystem::remove_all(root);
  return 0;
}
