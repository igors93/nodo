#ifndef NODO_NODE_CANONICAL_SLASHING_TRANSITION_HPP
#define NODO_NODE_CANONICAL_SLASHING_TRANSITION_HPP

#include "consensus/SlashingEvidence.hpp"
#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/Block.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <vector>

namespace nodo::node {

class CanonicalSlashingTransition {
public:
    static core::LedgerRecord buildEvidenceRecord(
        const consensus::DoubleVoteEvidence& evidence,
        std::int64_t blockTimestamp
    );

    static std::vector<consensus::DoubleVoteEvidence> evidenceFromBlock(
        const core::Block& block
    );

    static void applyBlockEvidence(
        const core::Block& block,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        consensus::ValidatorPenaltyLedger& penaltyLedger,
        core::ValidatorRegistry& validators
    );

    static void applyEvidenceRecords(
        const std::vector<core::LedgerRecord>& evidenceRecords,
        std::uint64_t blockHeight,
        std::int64_t blockTimestamp,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        consensus::ValidatorPenaltyLedger& penaltyLedger,
        core::ValidatorRegistry& validators
    );
};

} // namespace nodo::node

#endif
