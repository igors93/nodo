#ifndef NODO_CONSENSUS_VALIDATOR_VOTE_BUILDER_HPP
#define NODO_CONSENSUS_VALIDATOR_VOTE_BUILDER_HPP

#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/Signer.hpp"

namespace nodo::consensus {

class ValidatorVoteBuilder {
public:
    static ValidatorVoteRecord buildApprovalVote(
        const core::ValidatorRegistry& validatorRegistry,
        const core::Block& block,
        std::uint64_t round,
        std::int64_t createdAt,
        const crypto::Signer& signer
    );

    static ValidatorVoteRecord buildPrevote(
        const core::ValidatorRegistry& validatorRegistry,
        const core::Block& block,
        std::uint64_t round,
        std::int64_t createdAt,
        const crypto::Signer& signer
    );

    static ValidatorVoteRecord buildPrecommit(
        const core::ValidatorRegistry& validatorRegistry,
        const core::Block& block,
        std::uint64_t round,
        std::int64_t createdAt,
        const crypto::Signer& signer
    );
};

} // namespace nodo::consensus

#endif
