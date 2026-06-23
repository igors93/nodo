#include "consensus/ValidatorVoteBuilder.hpp"

#include <stdexcept>

namespace nodo::consensus {

ValidatorVoteRecord ValidatorVoteBuilder::buildApprovalVote(
    const core::ValidatorRegistry& validatorRegistry,
    const core::Block& block,
    std::uint64_t round,
    std::int64_t createdAt,
    const crypto::Signer& signer
) {
    if (!block.isValid() ||
        round == 0 ||
        createdAt <= 0) {
        throw std::invalid_argument("Validator vote build input is invalid.");
    }

    if (!validatorRegistry.isActiveValidator(signer.address())) {
        throw std::invalid_argument("Signer is not an active validator.");
    }

    if (!validatorRegistry.verifyValidatorIdentity(
            signer.address(),
            signer.keyPair().publicKey()
        )) {
        throw std::invalid_argument("Signer key does not match validator registry.");
    }

    return signer.signValidatorVote(
        block.index(),
        block.hash(),
        block.previousHash(),
        round,
        ValidatorVoteDecision::APPROVE,
        "runtime-block-pipeline-approval",
        createdAt
    );
}

ValidatorVoteRecord ValidatorVoteBuilder::buildPrevote(
    const core::ValidatorRegistry& validatorRegistry,
    const core::Block& block,
    std::uint64_t round,
    std::int64_t createdAt,
    const crypto::Signer& signer
) {
    if (!block.isValid() ||
        round == 0 ||
        createdAt <= 0) {
        throw std::invalid_argument("Validator vote build input is invalid.");
    }

    if (!validatorRegistry.isActiveValidator(signer.address())) {
        throw std::invalid_argument("Signer is not an active validator.");
    }

    if (!validatorRegistry.verifyValidatorIdentity(
            signer.address(),
            signer.keyPair().publicKey()
        )) {
        throw std::invalid_argument("Signer key does not match validator registry.");
    }

    return signer.signValidatorVote(
        block.index(),
        block.hash(),
        block.previousHash(),
        round,
        ValidatorVoteDecision::PREVOTE,
        "runtime-block-pipeline-prevote",
        createdAt
    );
}

ValidatorVoteRecord ValidatorVoteBuilder::buildPrecommit(
    const core::ValidatorRegistry& validatorRegistry,
    const core::Block& block,
    std::uint64_t round,
    std::int64_t createdAt,
    const crypto::Signer& signer
) {
    if (!block.isValid() ||
        round == 0 ||
        createdAt <= 0) {
        throw std::invalid_argument("Validator vote build input is invalid.");
    }

    if (!validatorRegistry.isActiveValidator(signer.address())) {
        throw std::invalid_argument("Signer is not an active validator.");
    }

    if (!validatorRegistry.verifyValidatorIdentity(
            signer.address(),
            signer.keyPair().publicKey()
        )) {
        throw std::invalid_argument("Signer key does not match validator registry.");
    }

    return signer.signValidatorVote(
        block.index(),
        block.hash(),
        block.previousHash(),
        round,
        ValidatorVoteDecision::PRECOMMIT,
        "runtime-block-pipeline-precommit",
        createdAt
    );
}

} // namespace nodo::consensus
