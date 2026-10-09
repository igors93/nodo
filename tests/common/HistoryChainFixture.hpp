#ifndef NODO_TESTS_COMMON_HISTORY_CHAIN_FIXTURE_HPP
#define NODO_TESTS_COMMON_HISTORY_CHAIN_FIXTURE_HPP

#include "TestFramework.hpp"
#include "app/CommandLineInterface.hpp"
#include "config/GenesisRegistry.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace nodo::test {

// A real localnet data directory driven through the CLI, so blocks, QCs,
// checkpoints and snapshots come from the production finalization path.
class HistoryChainFixture {
public:
  static constexpr std::int64_t kBaseTimestamp = 1900000000;

  explicit HistoryChainFixture(const std::string &name)
      : m_path(std::filesystem::temp_directory_path() /
               ("nodo-history-" + name)) {
    std::error_code ec;
    std::filesystem::remove_all(m_path, ec);
    run({"init", "--data-dir", m_path.string(), "--peer-id", peerId(),
         "--endpoint", "127.0.0.1:9911", "--timestamp",
         std::to_string(kBaseTimestamp)},
        "init");
    run({"keys", "create", "--data-dir", m_path.string(), "--timestamp",
         std::to_string(kBaseTimestamp + 1)},
        "keys create");
  }

  ~HistoryChainFixture() {
    std::error_code ec;
    std::filesystem::remove_all(m_path, ec);
  }

  HistoryChainFixture(const HistoryChainFixture &) = delete;
  HistoryChainFixture &operator=(const HistoryChainFixture &) = delete;

  static std::string peerId() { return "history-fixture-peer"; }

  static std::int64_t timestampFor(std::uint64_t height) {
    return kBaseTimestamp + 100 + static_cast<std::int64_t>(height) * 10;
  }

  void produce(std::uint64_t count) {
    for (std::uint64_t index = 0; index < count; ++index) {
      const std::uint64_t next = height() + 1;
      // Localnet refuses empty blocks: finalize one transfer per block.
      run({"tx", "submit", "--data-dir", m_path.string(), "--timestamp",
           std::to_string(timestampFor(next) - 5)},
          "tx submit " + std::to_string(next));
      run({"block", "produce", "--data-dir", m_path.string(), "--peer-id",
           peerId(), "--endpoint", "127.0.0.1:9911", "--timestamp",
           std::to_string(timestampFor(next))},
          "block produce " + std::to_string(next));
    }
  }

  app::CommandLineResult cli(std::vector<std::string> args) const {
    return app::CommandLineInterface::execute(args);
  }

  void run(const std::vector<std::string> &args,
           const std::string &what) const {
    const app::CommandLineResult result =
        app::CommandLineInterface::execute(args);
    require(result.success(), what + " failed: " + result.message());
  }

  const std::filesystem::path &path() const { return m_path; }

  node::NodeDataDirectoryConfig directory() const {
    return node::NodeDataDirectoryConfig(m_path);
  }

  static config::GenesisConfig genesis() {
    return config::GenesisRegistry::get("localnet").genesis();
  }

  std::uint64_t height() const {
    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directory());
    require(manifest.loaded(), "fixture manifest must load");
    return manifest.manifest().latestBlockHeight();
  }

  node::FinalizedBlockArtifact artifact(std::uint64_t height) const {
    return node::FinalizedBlockArtifactCodec::readBlockArtifactFile(
        node::FinalizedBlockStore::blockFilePath(directory(), height));
  }

private:
  std::filesystem::path m_path;
};

} // namespace nodo::test

#endif
