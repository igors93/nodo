#include "core/ValidatorProposalRegistry.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

namespace {

std::string hashString(const std::string& value) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));
    return std::string(output);
}

bool isSafeRegistryText(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character == ';' ||
            character == '{' ||
            character == '}' ||
            character == '\n' ||
            character == '\r' ||
            character == '\t') {
            return false;
        }
    }

    return true;
}

} // namespace

ValidatorProposalRegistryEntry::ValidatorProposalRegistryEntry()
    : m_validatorAddress(""),
      m_validatorPublicKeyFingerprint(""),
      m_blockIndex(0),
      m_blockHash(""),
      m_previousHash(""),
      m_chainSizeBeforeProposal(0),
      m_expectedPreviousHash(""),
      m_proposedAt(0),
      m_signatureDigest("") {}

ValidatorProposalRegistryEntry::ValidatorProposalRegistryEntry(
    std::string validatorAddress,
    std::string validatorPublicKeyFingerprint,
    std::uint64_t blockIndex,
    std::string blockHash,
    std::string previousHash,
    std::uint64_t chainSizeBeforeProposal,
    std::string expectedPreviousHash,
    std::int64_t proposedAt,
    std::string signatureDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_validatorPublicKeyFingerprint(std::move(validatorPublicKeyFingerprint)),
      m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_chainSizeBeforeProposal(chainSizeBeforeProposal),
      m_expectedPreviousHash(std::move(expectedPreviousHash)),
      m_proposedAt(proposedAt),
      m_signatureDigest(std::move(signatureDigest)) {}

const std::string& ValidatorProposalRegistryEntry::validatorAddress() const {
    return m_validatorAddress;
}

const std::string& ValidatorProposalRegistryEntry::validatorPublicKeyFingerprint() const {
    return m_validatorPublicKeyFingerprint;
}

std::uint64_t ValidatorProposalRegistryEntry::blockIndex() const {
    return m_blockIndex;
}

const std::string& ValidatorProposalRegistryEntry::blockHash() const {
    return m_blockHash;
}

const std::string& ValidatorProposalRegistryEntry::previousHash() const {
    return m_previousHash;
}

std::uint64_t ValidatorProposalRegistryEntry::chainSizeBeforeProposal() const {
    return m_chainSizeBeforeProposal;
}

const std::string& ValidatorProposalRegistryEntry::expectedPreviousHash() const {
    return m_expectedPreviousHash;
}

std::int64_t ValidatorProposalRegistryEntry::proposedAt() const {
    return m_proposedAt;
}

const std::string& ValidatorProposalRegistryEntry::signatureDigest() const {
    return m_signatureDigest;
}

bool ValidatorProposalRegistryEntry::isValid() const {
    if (!isSafeRegistryText(m_validatorAddress)) {
        return false;
    }

    if (m_validatorPublicKeyFingerprint.empty() ||
        m_blockHash.empty() ||
        m_previousHash.empty() ||
        m_expectedPreviousHash.empty() ||
        m_signatureDigest.empty()) {
        return false;
    }

    if (m_proposedAt <= 0) {
        return false;
    }

    return true;
}

bool ValidatorProposalRegistryEntry::sameProposalAs(
    const ValidatorProposalRegistryEntry& other
) const {
    return m_validatorAddress == other.m_validatorAddress &&
           m_validatorPublicKeyFingerprint == other.m_validatorPublicKeyFingerprint &&
           m_blockIndex == other.m_blockIndex &&
           m_blockHash == other.m_blockHash &&
           m_previousHash == other.m_previousHash &&
           m_chainSizeBeforeProposal == other.m_chainSizeBeforeProposal &&
           m_expectedPreviousHash == other.m_expectedPreviousHash;
}

bool ValidatorProposalRegistryEntry::conflictsWith(
    const ValidatorProposalRegistryEntry& other
) const {
    if (m_validatorAddress != other.m_validatorAddress) {
        return false;
    }

    if (m_validatorPublicKeyFingerprint != other.m_validatorPublicKeyFingerprint) {
        return false;
    }

    if (m_blockIndex != other.m_blockIndex) {
        return false;
    }

    return m_blockHash != other.m_blockHash;
}

std::string ValidatorProposalRegistryEntry::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorProposalRegistryEntry{"
        << "validator=" << m_validatorAddress
        << ";publicKeyFingerprint=" << m_validatorPublicKeyFingerprint
        << ";blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";previousHash=" << m_previousHash
        << ";chainSizeBeforeProposal=" << m_chainSizeBeforeProposal
        << ";expectedPreviousHash=" << m_expectedPreviousHash
        << ";proposedAt=" << m_proposedAt
        << ";signatureDigest=" << m_signatureDigest
        << "}";

    return oss.str();
}

ValidatorProposalRegistryEntry ValidatorProposalRegistryEntry::fromSignedProposal(
    const SignedProtectionBlockProposal& signedProposal
) {
    const ValidatorBlockProposalSignature& signature =
        signedProposal.proposalSignature();

    ValidatorProposalRegistryEntry entry(
        signature.validatorAddress(),
        signature.validatorPublicKey().fingerprint(),
        signature.signedBlockIndex(),
        signature.signedBlockHash(),
        signature.signedPreviousHash(),
        signature.signedChainSizeBeforeProposal(),
        signature.signedExpectedPreviousHash(),
        signature.proposedAt(),
        hashString(signature.serialize())
    );

    if (!entry.isValid()) {
        throw std::logic_error("Signed proposal produced invalid registry entry.");
    }

    return entry;
}

ValidatorDoubleSignEvidence::ValidatorDoubleSignEvidence()
    : m_firstProposal(),
      m_conflictingProposal() {}

ValidatorDoubleSignEvidence::ValidatorDoubleSignEvidence(
    ValidatorProposalRegistryEntry firstProposal,
    ValidatorProposalRegistryEntry conflictingProposal
)
    : m_firstProposal(std::move(firstProposal)),
      m_conflictingProposal(std::move(conflictingProposal)) {}

const ValidatorProposalRegistryEntry& ValidatorDoubleSignEvidence::firstProposal() const {
    return m_firstProposal;
}

const ValidatorProposalRegistryEntry& ValidatorDoubleSignEvidence::conflictingProposal() const {
    return m_conflictingProposal;
}

const std::string& ValidatorDoubleSignEvidence::validatorAddress() const {
    return m_firstProposal.validatorAddress();
}

std::uint64_t ValidatorDoubleSignEvidence::blockIndex() const {
    return m_firstProposal.blockIndex();
}

bool ValidatorDoubleSignEvidence::isValid() const {
    if (!m_firstProposal.isValid() || !m_conflictingProposal.isValid()) {
        return false;
    }

    return m_firstProposal.conflictsWith(m_conflictingProposal);
}

std::string ValidatorDoubleSignEvidence::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorDoubleSignEvidence{"
        << "validator=" << validatorAddress()
        << ";blockIndex=" << blockIndex()
        << ";first=" << m_firstProposal.serialize()
        << ";conflicting=" << m_conflictingProposal.serialize()
        << "}";

    return oss.str();
}

std::string validatorProposalRegistrationStatusToString(
    ValidatorProposalRegistrationStatus status
) {
    switch (status) {
        case ValidatorProposalRegistrationStatus::ACCEPTED:
            return "ACCEPTED";
        case ValidatorProposalRegistrationStatus::DUPLICATE:
            return "DUPLICATE";
        case ValidatorProposalRegistrationStatus::DOUBLE_SIGN_CONFLICT:
            return "DOUBLE_SIGN_CONFLICT";
        case ValidatorProposalRegistrationStatus::INVALID_PROPOSAL:
            return "INVALID_PROPOSAL";
        case ValidatorProposalRegistrationStatus::INVALID_SIGNATURE:
            return "INVALID_SIGNATURE";
        case ValidatorProposalRegistrationStatus::INVALID_REGISTRY:
            return "INVALID_REGISTRY";
        default:
            return "UNKNOWN";
    }
}

ValidatorProposalRegistrationResult::ValidatorProposalRegistrationResult()
    : m_status(ValidatorProposalRegistrationStatus::INVALID_REGISTRY),
      m_reason("Uninitialized registration result."),
      m_entry(),
      m_doubleSignEvidence() {}

ValidatorProposalRegistrationResult ValidatorProposalRegistrationResult::accepted(
    ValidatorProposalRegistryEntry entry
) {
    ValidatorProposalRegistrationResult result;
    result.m_status = ValidatorProposalRegistrationStatus::ACCEPTED;
    result.m_reason = "Signed proposal accepted.";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorProposalRegistrationResult ValidatorProposalRegistrationResult::duplicate(
    ValidatorProposalRegistryEntry existingEntry
) {
    ValidatorProposalRegistrationResult result;
    result.m_status = ValidatorProposalRegistrationStatus::DUPLICATE;
    result.m_reason = "Signed proposal already exists in registry.";
    result.m_entry = std::move(existingEntry);
    return result;
}

ValidatorProposalRegistrationResult ValidatorProposalRegistrationResult::conflict(
    ValidatorDoubleSignEvidence evidence
) {
    ValidatorProposalRegistrationResult result;
    result.m_status = ValidatorProposalRegistrationStatus::DOUBLE_SIGN_CONFLICT;
    result.m_reason = "Validator signed conflicting proposals for the same block index.";
    result.m_doubleSignEvidence = std::move(evidence);
    return result;
}

ValidatorProposalRegistrationResult ValidatorProposalRegistrationResult::invalid(
    ValidatorProposalRegistrationStatus status,
    std::string reason
) {
    if (status == ValidatorProposalRegistrationStatus::ACCEPTED ||
        status == ValidatorProposalRegistrationStatus::DUPLICATE ||
        status == ValidatorProposalRegistrationStatus::DOUBLE_SIGN_CONFLICT) {
        throw std::invalid_argument("Invalid registration status helper used with non-invalid status.");
    }

    ValidatorProposalRegistrationResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

ValidatorProposalRegistrationStatus ValidatorProposalRegistrationResult::status() const {
    return m_status;
}

const std::string& ValidatorProposalRegistrationResult::reason() const {
    return m_reason;
}

const ValidatorProposalRegistryEntry& ValidatorProposalRegistrationResult::entry() const {
    return m_entry;
}

const ValidatorDoubleSignEvidence& ValidatorProposalRegistrationResult::doubleSignEvidence() const {
    return m_doubleSignEvidence;
}

bool ValidatorProposalRegistrationResult::accepted() const {
    return m_status == ValidatorProposalRegistrationStatus::ACCEPTED;
}

bool ValidatorProposalRegistrationResult::duplicate() const {
    return m_status == ValidatorProposalRegistrationStatus::DUPLICATE;
}

bool ValidatorProposalRegistrationResult::conflictDetected() const {
    return m_status == ValidatorProposalRegistrationStatus::DOUBLE_SIGN_CONFLICT;
}

bool ValidatorProposalRegistrationResult::successWithoutConflict() const {
    return accepted() || duplicate();
}

std::string ValidatorProposalRegistrationResult::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorProposalRegistrationResult{"
        << "status=" << validatorProposalRegistrationStatusToString(m_status)
        << ";reason=" << m_reason;

    if (m_entry.isValid()) {
        oss << ";entry=" << m_entry.serialize();
    }

    if (m_doubleSignEvidence.isValid()) {
        oss << ";doubleSignEvidence=" << m_doubleSignEvidence.serialize();
    }

    oss << "}";

    return oss.str();
}

ValidatorProposalRegistry::ValidatorProposalRegistry()
    : m_entries(),
      m_conflicts() {}

ValidatorProposalRegistrationResult ValidatorProposalRegistry::registerSignedProposal(
    const SignedProtectionBlockProposal& signedProposal,
    const Blockchain& blockchain,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    if (!isValid()) {
        return ValidatorProposalRegistrationResult::invalid(
            ValidatorProposalRegistrationStatus::INVALID_REGISTRY,
            "Registry already contains invalid data."
        );
    }

    if (!signedProposal.proposal().isValidForBlockchain(blockchain)) {
        return ValidatorProposalRegistrationResult::invalid(
            ValidatorProposalRegistrationStatus::INVALID_PROPOSAL,
            "Signed proposal is not valid for the supplied blockchain tip."
        );
    }

    if (!signedProposal.proposalSignature().verifyForProposal(
            signedProposal.proposal(),
            policy,
            provider
        )) {
        return ValidatorProposalRegistrationResult::invalid(
            ValidatorProposalRegistrationStatus::INVALID_SIGNATURE,
            "Proposal signature does not verify against proposal payload."
        );
    }

    ValidatorProposalRegistryEntry candidate;

    try {
        candidate = ValidatorProposalRegistryEntry::fromSignedProposal(signedProposal);
    } catch (const std::exception& error) {
        return ValidatorProposalRegistrationResult::invalid(
            ValidatorProposalRegistrationStatus::INVALID_SIGNATURE,
            error.what()
        );
    }

    const ValidatorProposalRegistryEntry* sameProposal =
        findSameProposal(candidate);

    if (sameProposal != nullptr) {
        return ValidatorProposalRegistrationResult::duplicate(*sameProposal);
    }

    const ValidatorProposalRegistryEntry* conflictingProposal =
        findConflictingProposal(candidate);

    if (conflictingProposal != nullptr) {
        ValidatorDoubleSignEvidence evidence(
            *conflictingProposal,
            candidate
        );

        if (!evidence.isValid()) {
            return ValidatorProposalRegistrationResult::invalid(
                ValidatorProposalRegistrationStatus::INVALID_REGISTRY,
                "Detected double-sign evidence is invalid."
            );
        }

        m_conflicts.push_back(evidence);
        return ValidatorProposalRegistrationResult::conflict(evidence);
    }

    m_entries.push_back(candidate);
    return ValidatorProposalRegistrationResult::accepted(candidate);
}

const std::vector<ValidatorProposalRegistryEntry>& ValidatorProposalRegistry::entries() const {
    return m_entries;
}

const std::vector<ValidatorDoubleSignEvidence>& ValidatorProposalRegistry::conflicts() const {
    return m_conflicts;
}

bool ValidatorProposalRegistry::hasProposal(
    const std::string& validatorAddress,
    std::uint64_t blockIndex,
    const std::string& blockHash
) const {
    for (const auto& entry : m_entries) {
        if (entry.validatorAddress() == validatorAddress &&
            entry.blockIndex() == blockIndex &&
            entry.blockHash() == blockHash) {
            return true;
        }
    }

    return false;
}

std::size_t ValidatorProposalRegistry::proposalCountForValidator(
    const std::string& validatorAddress
) const {
    std::size_t count = 0;

    for (const auto& entry : m_entries) {
        if (entry.validatorAddress() == validatorAddress) {
            ++count;
        }
    }

    return count;
}

std::size_t ValidatorProposalRegistry::conflictCountForValidator(
    const std::string& validatorAddress
) const {
    std::size_t count = 0;

    for (const auto& conflict : m_conflicts) {
        if (conflict.isValid() && conflict.validatorAddress() == validatorAddress) {
            ++count;
        }
    }

    return count;
}

bool ValidatorProposalRegistry::hasDoubleSignConflict(
    const std::string& validatorAddress,
    std::uint64_t blockIndex
) const {
    for (const auto& conflict : m_conflicts) {
        if (!conflict.isValid()) {
            continue;
        }

        if (conflict.validatorAddress() == validatorAddress &&
            conflict.blockIndex() == blockIndex) {
            return true;
        }
    }

    return false;
}

bool ValidatorProposalRegistry::isValid() const {
    for (const auto& entry : m_entries) {
        if (!entry.isValid()) {
            return false;
        }
    }

    for (std::size_t left = 0; left < m_entries.size(); ++left) {
        for (std::size_t right = left + 1; right < m_entries.size(); ++right) {
            if (m_entries[left].sameProposalAs(m_entries[right])) {
                return false;
            }
        }
    }

    for (const auto& conflict : m_conflicts) {
        if (!conflict.isValid()) {
            return false;
        }
    }

    return true;
}

std::string ValidatorProposalRegistry::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorProposalRegistry{"
        << "entryCount=" << m_entries.size()
        << ";conflictCount=" << m_conflicts.size()
        << "}";

    return oss.str();
}

const ValidatorProposalRegistryEntry* ValidatorProposalRegistry::findSameProposal(
    const ValidatorProposalRegistryEntry& candidate
) const {
    for (const auto& entry : m_entries) {
        if (entry.sameProposalAs(candidate)) {
            return &entry;
        }
    }

    return nullptr;
}

const ValidatorProposalRegistryEntry* ValidatorProposalRegistry::findConflictingProposal(
    const ValidatorProposalRegistryEntry& candidate
) const {
    for (const auto& entry : m_entries) {
        if (entry.conflictsWith(candidate)) {
            return &entry;
        }
    }

    return nullptr;
}

} // namespace nodo::core
