#include "storage/AtomicFile.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path tempRoot() {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("nodo_atomic_security_" + std::to_string(nonce));
}

void testNewFileCreated() {
  const auto root = tempRoot();
  std::filesystem::create_directories(root);

  const auto file = root / "target.nodo";
  nodo::storage::AtomicFile::writeTextFile(file, "hello");

  assert(std::filesystem::exists(file));
  assert(nodo::storage::AtomicFile::readTextFile(file) == "hello");

  std::filesystem::remove_all(root);
}

void testOverwriteReplacesContent() {
  const auto root = tempRoot();
  std::filesystem::create_directories(root);

  const auto file = root / "target.nodo";
  nodo::storage::AtomicFile::writeTextFile(file, "first");
  nodo::storage::AtomicFile::writeTextFile(file, "second");

  assert(nodo::storage::AtomicFile::readTextFile(file) == "second");

  std::filesystem::remove_all(root);
}

void testNoTemporaryFilesLeftAfterSuccess() {
  const auto root = tempRoot();
  std::filesystem::create_directories(root);

  const auto file = root / "target.nodo";
  nodo::storage::AtomicFile::writeTextFile(file, "data");

  const auto leftovers =
      nodo::storage::AtomicFile::listTemporaryWriteFiles(root);

  assert(leftovers.empty() &&
         "No temp files must remain after successful write");

  std::filesystem::remove_all(root);
}

void testTemporaryFileIdentificationByMarker() {
  const auto root = tempRoot();
  std::filesystem::create_directories(root);

  const auto syntheticTmp = root / "manifest.nodo.tmp.12345";
  nodo::storage::AtomicFile::writeTextFile(syntheticTmp, "orphan");

  assert(nodo::storage::AtomicFile::isTemporaryWriteFile(syntheticTmp));

  const auto realFile = root / "manifest.nodo";
  nodo::storage::AtomicFile::writeTextFile(realFile, "real");

  assert(!nodo::storage::AtomicFile::isTemporaryWriteFile(realFile));

  std::filesystem::remove_all(root);
}

void testRemoveTemporaryWriteFiles() {
  const auto root = tempRoot();
  std::filesystem::create_directories(root);

  const auto tmp1 = root / "a.nodo.tmp.111";
  const auto tmp2 = root / "b.nodo.tmp.222";
  const auto real = root / "c.nodo";

  nodo::storage::AtomicFile::writeTextFile(tmp1, "t1");
  nodo::storage::AtomicFile::writeTextFile(tmp2, "t2");
  nodo::storage::AtomicFile::writeTextFile(real, "real");

  const std::size_t removed =
      nodo::storage::AtomicFile::removeTemporaryWriteFiles(root);

  assert(removed == 2);
  assert(!std::filesystem::exists(tmp1));
  assert(!std::filesystem::exists(tmp2));
  assert(std::filesystem::exists(real));

  std::filesystem::remove_all(root);
}

} // namespace

int main() {
  testNewFileCreated();
  testOverwriteReplacesContent();
  testNoTemporaryFilesLeftAfterSuccess();
  testTemporaryFileIdentificationByMarker();
  testRemoveTemporaryWriteFiles();
  return 0;
}
