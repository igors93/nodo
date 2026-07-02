#include "node/EpochParticipation.hpp"

#include "consensus/ProposerSchedule.hpp"
#include "crypto/hash.h"
#include "node/ProtectionRewards.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

struct ParticipationCounter {
    std::uint64_t eligibleRounds = 0;
    std::uint64_t acceptedVotes = 0;
    std::uint64_t acceptedProposals = 0;
    std::ostringstream voteEvidence;
    std::ostringstream proposalEvidence;
};

struct PreviousScore {
    std::uint64_t epoch = 0;
    std::int64_t timestamp = 0;
    std::int32_t score = 0;
};

std::uint64_t checkedAdd(
    std::uint64_t left,
    std::uint64_t right,
    const char* context
) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error(std::string(context) + " overflow.");
    }
    return left + right;
}

std::string digest(const std::string& value) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));
    return std::string(output);
}

std::map<std::string, PreviousScore> previousScores(
    const core::Blockchain& blockchain,
    std::uint64_t beforeEpoch
) {
    std::map<std::string, PreviousScore> result;
    for (const core::Block& block : blockchain.blocks()) {
        for (const core::LedgerRecord& ledgerRecord : block.records()) {
            if (ledgerRecord.type() != core::LedgerRecordType::VALIDATOR_SCORE) continue;
            const economics::ValidatorScoreRecord record =
                economics::ValidatorScoreRecord::deserialize(ledgerRecord.payload());
            if (record.epoch() >= beforeEpoch) continue;
            PreviousScore& current = result[record.validatorAddress()];
            if (record.epoch() > current.epoch ||
                (record.epoch() == current.epoch && record.timestamp() > current.timestamp)) {
                current = {record.epoch(), record.timestamp(), record.newScore()};
            }
        }
    }
    return result;
}

std::uint32_t checkedWorkWeight(std::uint64_t value) {
    if (value == 0 || value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("Epoch aggregate work weight exceeds ledger capacity.");
    }
    return static_cast<std::uint32_t>(value);
}

} // namespace

EpochParticipationSnapshot::EpochParticipationSnapshot()
    : m_epoch(0), m_startBlock(0), m_endBlock(0), m_targetWorkWeight(0) {}

EpochParticipationSnapshot::EpochParticipationSnapshot(
    std::uint64_t epoch,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    std::uint64_t targetWorkWeight,
    std::vector<economics::ValidationWorkRecord> workRecords,
    std::vector<economics::ValidatorScoreRecord> scoreRecords
) : m_epoch(epoch),
    m_startBlock(startBlock),
    m_endBlock(endBlock),
    m_targetWorkWeight(targetWorkWeight),
    m_workRecords(std::move(workRecords)),
    m_scoreRecords(std::move(scoreRecords)) {}

std::uint64_t EpochParticipationSnapshot::epoch() const { return m_epoch; }
std::uint64_t EpochParticipationSnapshot::startBlock() const { return m_startBlock; }
std::uint64_t EpochParticipationSnapshot::endBlock() const { return m_endBlock; }
std::uint64_t EpochParticipationSnapshot::targetWorkWeight() const { return m_targetWorkWeight; }
const std::vector<economics::ValidationWorkRecord>& EpochParticipationSnapshot::workRecords() const { return m_workRecords; }
const std::vector<economics::ValidatorScoreRecord>& EpochParticipationSnapshot::scoreRecords() const { return m_scoreRecords; }

std::uint64_t EpochParticipationSnapshot::acceptedWorkWeight() const {
    std::uint64_t total = 0;
    for (const auto& record : m_workRecords) {
        if (record.contributesToReward()) {
            total = checkedAdd(total, record.workWeight(), "Accepted epoch work");
        }
    }
    return total;
}

std::vector<core::LedgerRecord> EpochParticipationSnapshot::ledgerRecords(
    std::int64_t timestamp
) const {
    if (!isValid() || timestamp <= 0) {
        throw std::invalid_argument("Cannot create ledger records from invalid epoch participation.");
    }
    std::vector<core::LedgerRecord> records;
    records.reserve(m_workRecords.size() + m_scoreRecords.size());
    for (const auto& record : m_workRecords) {
        records.push_back(core::LedgerRecord::fromValidationWorkRecord(record, timestamp));
    }
    for (const auto& record : m_scoreRecords) {
        records.push_back(core::LedgerRecord::fromValidatorScoreRecord(record, timestamp));
    }
    return records;
}

bool EpochParticipationSnapshot::isValid() const {
    if (m_epoch == 0 || m_startBlock == 0 || m_endBlock < m_startBlock ||
        m_targetWorkWeight == 0 || m_scoreRecords.empty()) {
        return false;
    }
    std::set<std::string> workKeys;
    std::set<std::string> evidence;
    for (const auto& record : m_workRecords) {
        if (!record.isValid() || record.epoch() != m_epoch ||
            !record.contributesToReward() ||
            !workKeys.insert(record.validatorAddress() + ":" +
                economics::validationWorkTypeToString(record.workType())).second ||
            !evidence.insert(record.evidenceHash()).second) {
            return false;
        }
    }
    std::set<std::string> scored;
    for (const auto& record : m_scoreRecords) {
        if (!record.isValid() || record.epoch() != m_epoch ||
            !scored.insert(record.validatorAddress()).second) {
            return false;
        }
    }
    try {
        return acceptedWorkWeight() <= m_targetWorkWeight;
    } catch (const std::exception&) {
        return false;
    }
}

EpochParticipationSnapshot EpochParticipation::build(
    std::uint64_t epoch,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    const std::string& chainId,
    const core::Blockchain& blockchain,
    const core::ValidatorSetHistory& validatorSetHistory,
    const consensus::BlockFinalizationRegistry& finalizationRegistry,
    std::int64_t settlementTimestamp
) {
    if (epoch == 0 || startBlock == 0 || endBlock < startBlock || chainId.empty() ||
        settlementTimestamp <= 0 || blockchain.empty() || !blockchain.isValid(false) ||
        !validatorSetHistory.isValid() || !finalizationRegistry.isValid() ||
        blockchain.latestBlock().index() < endBlock) {
        throw std::invalid_argument("Invalid context for epoch participation reconstruction.");
    }

    std::map<std::string, ParticipationCounter> counters;
    std::uint64_t targetWorkWeight = 0;

    for (std::uint64_t height = startBlock; height <= endBlock; ++height) {
        if (!validatorSetHistory.hasSet(height)) {
            throw std::logic_error("Validator set history is missing epoch height " + std::to_string(height) + ".");
        }
        const consensus::FinalizedBlockRecord* finalized =
            finalizationRegistry.recordForHeight(height);
        if (finalized == nullptr || finalized->blockIndex() != height ||
            finalized->blockHash() != blockchain.blocks().at(height).hash() ||
            !finalized->quorumCertificate().isStructurallyValid()) {
            throw std::logic_error("Finalization evidence is missing or inconsistent at height " + std::to_string(height) + ".");
        }

        const core::ValidatorRegistry& validators = validatorSetHistory.setAt(height);
        const std::uint64_t roundOpportunities = std::max<std::uint64_t>(finalized->round(), 1U);
        for (const std::string& address : validators.eligibleValidatorAddresses()) {
            ParticipationCounter& counter = counters[address];
            counter.eligibleRounds = checkedAdd(
                counter.eligibleRounds, roundOpportunities, "Validator round opportunities"
            );
            targetWorkWeight = checkedAdd(
                targetWorkWeight, roundOpportunities, "Epoch target work"
            );
        }

        const std::string proposer = consensus::ProposerSchedule::selectProposer(
            validators, chainId, height, finalized->round()
        );
        if (proposer.empty()) {
            throw std::logic_error("Finalized block has no deterministic proposer.");
        }
        ParticipationCounter& proposerCounter = counters[proposer];
        proposerCounter.acceptedProposals = checkedAdd(
            proposerCounter.acceptedProposals, 1, "Accepted proposals"
        );
        proposerCounter.proposalEvidence << height << ':' << finalized->round()
            << ':' << finalized->blockHash() << '|';
        targetWorkWeight = checkedAdd(targetWorkWeight, 1, "Epoch target work");

        std::set<std::string> voters;
        for (const consensus::ValidatorVoteRecord& vote :
             finalized->quorumCertificate().votes()) {
            if (!voters.insert(vote.validatorAddress()).second ||
                !validators.isEligibleForConsensus(vote.validatorAddress())) {
                throw std::logic_error("Finalized certificate contains invalid epoch voter evidence.");
            }
            ParticipationCounter& counter = counters[vote.validatorAddress()];
            counter.acceptedVotes = checkedAdd(counter.acceptedVotes, 1, "Accepted votes");
            counter.voteEvidence << height << ':' << finalized->round()
                << ':' << finalized->blockHash() << '|';
        }
        if (height == std::numeric_limits<std::uint64_t>::max()) break;
    }

    const std::string evidenceBlockHash = blockchain.blocks().at(endBlock).hash();
    const auto priorScores = previousScores(blockchain, epoch);
    std::vector<economics::ValidationWorkRecord> workRecords;
    std::vector<economics::ValidatorScoreRecord> scoreRecords;
    scoreRecords.reserve(counters.size());

    for (auto& [address, counter] : counters) {
        if (counter.acceptedProposals > 0) {
            workRecords.emplace_back(
                address, epoch, economics::ValidationWorkType::VALIDATE_BLOCK,
                economics::ValidationWorkResult::ACCEPTED, evidenceBlockHash,
                digest("NODO_EPOCH_PROPOSALS_V1|" + std::to_string(epoch) + "|" +
                       address + "|" + counter.proposalEvidence.str()),
                checkedWorkWeight(counter.acceptedProposals), settlementTimestamp
            );
        }
        if (counter.acceptedVotes > 0) {
            workRecords.emplace_back(
                address, epoch, economics::ValidationWorkType::CONSENSUS_VOTE,
                economics::ValidationWorkResult::ACCEPTED, evidenceBlockHash,
                digest("NODO_EPOCH_VOTES_V1|" + std::to_string(epoch) + "|" +
                       address + "|" + counter.voteEvidence.str()),
                checkedWorkWeight(counter.acceptedVotes), settlementTimestamp
            );
        }
        const auto prior = priorScores.find(address);
        const std::int32_t oldScore = prior == priorScores.end() ? 0 : prior->second.score;
        const std::int32_t newScore = static_cast<std::int32_t>(
            ProtectionRewards::epochParticipationPercent(
                counter.acceptedVotes, counter.eligibleRounds
            )
        );
        scoreRecords.emplace_back(
            address, epoch, oldScore, newScore,
            newScore >= oldScore
                ? economics::ValidatorScoreReason::CONSISTENT_VALIDATION
                : economics::ValidatorScoreReason::MISSED_CHALLENGE,
            digest("NODO_EPOCH_UPTIME_V1|" + std::to_string(epoch) + "|" + address +
                   "|votes=" + std::to_string(counter.acceptedVotes) +
                   "|rounds=" + std::to_string(counter.eligibleRounds)),
            settlementTimestamp
        );
    }

    std::sort(workRecords.begin(), workRecords.end(), [](const auto& left, const auto& right) {
        if (left.validatorAddress() != right.validatorAddress()) {
            return left.validatorAddress() < right.validatorAddress();
        }
        return economics::validationWorkTypeToString(left.workType()) <
               economics::validationWorkTypeToString(right.workType());
    });

    EpochParticipationSnapshot snapshot(
        epoch, startBlock, endBlock, targetWorkWeight,
        std::move(workRecords), std::move(scoreRecords)
    );
    if (!snapshot.isValid()) {
        throw std::logic_error("Epoch participation reconstruction produced an invalid snapshot.");
    }
    return snapshot;
}

} // namespace nodo::node
