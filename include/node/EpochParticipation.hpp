#ifndef NODO_NODE_EPOCH_PARTICIPATION_HPP
#define NODO_NODE_EPOCH_PARTICIPATION_HPP

#include "consensus/BlockFinalizer.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * Canonical, compact evidence for one completed validator epoch.
 *
 * The snapshot is derived only from finalized quorum certificates and the
 * immutable validator set recorded for each height.  It intentionally stores
 * aggregate work per validator instead of copying 43,200 certificates into the
 * settlement block.
 */
class EpochParticipationSnapshot {
public:
  EpochParticipationSnapshot();

  EpochParticipationSnapshot(
      std::uint64_t epoch, std::uint64_t startBlock, std::uint64_t endBlock,
      std::uint64_t targetWorkWeight,
      std::vector<economics::ValidationWorkRecord> workRecords,
      std::vector<economics::ValidatorScoreRecord> scoreRecords);

  std::uint64_t epoch() const;
  std::uint64_t startBlock() const;
  std::uint64_t endBlock() const;
  std::uint64_t targetWorkWeight() const;
  std::uint64_t acceptedWorkWeight() const;
  const std::vector<economics::ValidationWorkRecord> &workRecords() const;
  const std::vector<economics::ValidatorScoreRecord> &scoreRecords() const;

  std::vector<core::LedgerRecord> ledgerRecords(std::int64_t timestamp) const;
  bool isValid() const;

private:
  std::uint64_t m_epoch;
  std::uint64_t m_startBlock;
  std::uint64_t m_endBlock;
  std::uint64_t m_targetWorkWeight;
  std::vector<economics::ValidationWorkRecord> m_workRecords;
  std::vector<economics::ValidatorScoreRecord> m_scoreRecords;
};

class EpochParticipation {
public:
  static EpochParticipationSnapshot
  build(std::uint64_t epoch, std::uint64_t startBlock, std::uint64_t endBlock,
        const std::string &chainId, const core::Blockchain &blockchain,
        const core::ValidatorSetHistory &validatorSetHistory,
        const consensus::BlockFinalizationRegistry &finalizationRegistry,
        std::int64_t settlementTimestamp);
};

} // namespace nodo::node

#endif
