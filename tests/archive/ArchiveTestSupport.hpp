#ifndef NODO_TESTS_ARCHIVE_ARCHIVE_TEST_SUPPORT_HPP
#define NODO_TESTS_ARCHIVE_ARCHIVE_TEST_SUPPORT_HPP

#include "../common/TestFramework.hpp"

#include "archive/ArchivalChallenge.hpp"
#include "archive/ArchivalProof.hpp"
#include "archive/ArchiveEncoding.hpp"
#include "archive/ArchiveProvider.hpp"
#include "archive/ArchiveSegment.hpp"
#include "config/HistoryParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace nodo::test {

// Deterministic synthetic finalized history: segment code never parses
// blocks, it commits their canonical bytes and hashes.
class SyntheticHistory {
public:
  static constexpr const char *kChainId = "nodo-localnet-1";
  static constexpr std::int64_t kSignedAt = 1900000000;

  explicit SyntheticHistory(std::uint64_t blocks)
      : m_parameters(config::HistoryParameters::developmentLocal()) {
    for (std::uint64_t height = 0; height <= blocks; ++height) {
      std::string bytes = "Block{height=" + std::to_string(height) + ";payload=";
      bytes.append(100 + (height * 97) % 900, static_cast<char>('a' + height % 26));
      bytes += "}";
      m_blocks[height] =
          archive::ArchivedBlock{archive::hashHex("TEST/BLOCK", bytes), bytes};
    }
  }

  const config::HistoryParameters &parameters() const { return m_parameters; }

  archive::ArchivedBlockSource source(std::set<std::uint64_t> missing = {}) const {
    return [this, missing](std::uint64_t height)
               -> std::optional<archive::ArchivedBlock> {
      const auto found = m_blocks.find(height);
      if (found == m_blocks.end() || missing.count(height) != 0) {
        return std::nullopt;
      }
      return found->second;
    };
  }

  archive::ArchiveSegmentIndex segment(std::uint64_t index) const {
    return archive::ArchiveSegmentBuilder::build(m_parameters, kChainId, index,
                                                 source());
  }

  std::vector<archive::ArchiveSegmentCommitment> commitments(std::uint64_t count) const {
    std::vector<archive::ArchiveSegmentCommitment> result;
    for (std::uint64_t index = 0; index < count; ++index) {
      result.push_back(segment(index).commitment());
    }
    return result;
  }

  const std::string &blockHash(std::uint64_t height) const {
    return m_blocks.at(height).blockHash;
  }

  static crypto::KeyPair key(const std::string &name) {
    return crypto::KeyPair::createDeterministicEd25519KeyPair("archive-" + name);
  }

  // Registers and activates a provider; returns its id.
  std::string addProvider(archive::ArchiveProviderRegistry &registry,
                          const std::string &name, const std::string &operatorId,
                          std::uint64_t slots = 100,
                          std::uint64_t capacityBytes = 1ULL << 40) const {
    archive::ArchiveProviderRegistrationFields fields;
    fields.chainId = kChainId;
    fields.operatorId = operatorId;
    fields.bondRawUnits = slots * m_parameters.archiveMinBondPerSlotRawUnits();
    fields.declaredCapacityBytes = capacityBytes;
    fields.registeredHeight = 1;
    const auto registration =
        archive::ArchiveProviderRegistration::sign(fields, key(name), kSignedAt);
    require(registry.registerProvider(registration, m_parameters) ==
                archive::ArchiveRegistrationStatus::REGISTERED,
            "provider " + name + " must register");
    const std::uint64_t activation =
        1 + m_parameters.archiveProviderActivationDelayBlocks();
    registry.activateAt(activation, blockHash(activation));
    return registration.fields().providerId;
  }

  // Round whose seed block comes right after the segment is sealed.
  std::uint64_t firstRoundFor(std::uint64_t segmentIndex) const {
    return m_parameters.segmentLastHeight(segmentIndex) /
           m_parameters.archiveChallengeIntervalBlocks();
  }

  archive::ArchivalChallenge challenge(const std::string &providerId,
                                       std::uint64_t segmentIndex,
                                       std::uint64_t round) const {
    return archive::ArchivalChallenge::derive(
        m_parameters, providerId, segment(segmentIndex).commitment(), round,
        blockHash(m_parameters.challengeSeedHeight(round)));
  }

private:
  config::HistoryParameters m_parameters;
  std::map<std::uint64_t, archive::ArchivedBlock> m_blocks;
};

} // namespace nodo::test

#endif
