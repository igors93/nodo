#include "privacy/PrivateAccountingRecord.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::privacy {

std::string privateAccountingRecordTypeToString(
    PrivateAccountingRecordType type
) {
    switch (type) {
        case PrivateAccountingRecordType::PRIVATE_MINT:
            return "PRIVATE_MINT";

        case PrivateAccountingRecordType::PRIVATE_TRANSFER:
            return "PRIVATE_TRANSFER";

        case PrivateAccountingRecordType::PRIVATE_BURN:
            return "PRIVATE_BURN";

        default:
            return "UNKNOWN";
    }
}

std::string publicSupplyEffectToString(PublicSupplyEffect effect) {
    switch (effect) {
        case PublicSupplyEffect::NO_SUPPLY_CHANGE:
            return "NO_SUPPLY_CHANGE";

        case PublicSupplyEffect::SUPPLY_INCREASE:
            return "SUPPLY_INCREASE";

        case PublicSupplyEffect::SUPPLY_DECREASE:
            return "SUPPLY_DECREASE";

        default:
            return "UNKNOWN";
    }
}

PrivateAccountingRecord PrivateAccountingRecord::createPrivateMintRecord(
    std::vector<PrivacyCommitment> outputCommitments,
    utils::Amount publicSupplyAmount,
    std::string auditReference,
    std::int64_t timestamp
) {
    return createRecord(
        PrivateAccountingRecordType::PRIVATE_MINT,
        PublicSupplyEffect::SUPPLY_INCREASE,
        publicSupplyAmount,
        {},
        std::move(outputCommitments),
        std::move(auditReference),
        timestamp
    );
}

PrivateAccountingRecord PrivateAccountingRecord::createPrivateTransferRecord(
    std::vector<PrivacyNullifier> inputNullifiers,
    std::vector<PrivacyCommitment> outputCommitments,
    std::string auditReference,
    std::int64_t timestamp
) {
    return createRecord(
        PrivateAccountingRecordType::PRIVATE_TRANSFER,
        PublicSupplyEffect::NO_SUPPLY_CHANGE,
        utils::Amount::fromRawUnits(0),
        std::move(inputNullifiers),
        std::move(outputCommitments),
        std::move(auditReference),
        timestamp
    );
}

PrivateAccountingRecord PrivateAccountingRecord::createPrivateBurnRecord(
    std::vector<PrivacyNullifier> inputNullifiers,
    utils::Amount publicSupplyAmount,
    std::string auditReference,
    std::int64_t timestamp
) {
    return createRecord(
        PrivateAccountingRecordType::PRIVATE_BURN,
        PublicSupplyEffect::SUPPLY_DECREASE,
        publicSupplyAmount,
        std::move(inputNullifiers),
        {},
        std::move(auditReference),
        timestamp
    );
}

PrivateAccountingRecord::PrivateAccountingRecord(
    std::string id,
    PrivateAccountingRecordType type,
    PublicSupplyEffect supplyEffect,
    utils::Amount publicSupplyAmount,
    std::vector<PrivacyNullifier> inputNullifiers,
    std::vector<PrivacyCommitment> outputCommitments,
    std::string auditReference,
    std::string proofHash,
    std::int64_t timestamp
)
    : m_id(std::move(id)),
      m_type(type),
      m_supplyEffect(supplyEffect),
      m_publicSupplyAmount(publicSupplyAmount),
      m_inputNullifiers(std::move(inputNullifiers)),
      m_outputCommitments(std::move(outputCommitments)),
      m_auditReference(std::move(auditReference)),
      m_proofHash(std::move(proofHash)),
      m_timestamp(timestamp) {}

const std::string& PrivateAccountingRecord::id() const {
    return m_id;
}

PrivateAccountingRecordType PrivateAccountingRecord::type() const {
    return m_type;
}

PublicSupplyEffect PrivateAccountingRecord::supplyEffect() const {
    return m_supplyEffect;
}

utils::Amount PrivateAccountingRecord::publicSupplyAmount() const {
    return m_publicSupplyAmount;
}

const std::vector<PrivacyNullifier>&
PrivateAccountingRecord::inputNullifiers() const {
    return m_inputNullifiers;
}

const std::vector<PrivacyCommitment>&
PrivateAccountingRecord::outputCommitments() const {
    return m_outputCommitments;
}

const std::string& PrivateAccountingRecord::auditReference() const {
    return m_auditReference;
}

const std::string& PrivateAccountingRecord::proofHash() const {
    return m_proofHash;
}

std::int64_t PrivateAccountingRecord::timestamp() const {
    return m_timestamp;
}

bool PrivateAccountingRecord::isValid() const {
    if (m_id.empty()) {
        return false;
    }

    if (m_auditReference.empty()) {
        return false;
    }

    if (m_proofHash.empty()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (!hasValidSupplyEffect()) {
        return false;
    }

    if (!hasValidShapeForType()) {
        return false;
    }

    if (!hasValidInputsAndOutputs()) {
        return false;
    }

    const std::string expectedProofHash = computeDevelopmentProofHash(
        m_type,
        m_supplyEffect,
        m_publicSupplyAmount,
        m_inputNullifiers,
        m_outputCommitments,
        m_auditReference
    );

    if (m_proofHash != expectedProofHash) {
        return false;
    }

    const std::string expectedId = computeRecordId(
        m_type,
        m_proofHash,
        m_auditReference,
        m_timestamp
    );

    if (m_id != expectedId) {
        return false;
    }

    return true;
}

bool PrivateAccountingRecord::hasDuplicateInputNullifiers() const {
    for (std::size_t i = 0; i < m_inputNullifiers.size(); ++i) {
        for (std::size_t j = i + 1; j < m_inputNullifiers.size(); ++j) {
            if (m_inputNullifiers[i].nullifierHash() ==
                m_inputNullifiers[j].nullifierHash()) {
                return true;
            }
        }
    }

    return false;
}

bool PrivateAccountingRecord::hasDuplicateOutputCommitments() const {
    for (std::size_t i = 0; i < m_outputCommitments.size(); ++i) {
        for (std::size_t j = i + 1; j < m_outputCommitments.size(); ++j) {
            if (m_outputCommitments[i].id() ==
                m_outputCommitments[j].id()) {
                return true;
            }
        }
    }

    return false;
}

std::string PrivateAccountingRecord::serialize() const {
    std::ostringstream oss;

    oss << "PrivateAccountingRecord{"
        << "id=" << m_id
        << ";type=" << privateAccountingRecordTypeToString(m_type)
        << ";supplyEffect=" << publicSupplyEffectToString(m_supplyEffect)
        << ";publicSupplyAmountRaw=" << m_publicSupplyAmount.rawUnits()
        << ";auditReference=" << m_auditReference
        << ";proofHash=" << m_proofHash
        << ";timestamp=" << m_timestamp
        << ";inputNullifiers=[";

    for (std::size_t i = 0; i < m_inputNullifiers.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << m_inputNullifiers[i].serialize();
    }

    oss << "];outputCommitments=[";

    for (std::size_t i = 0; i < m_outputCommitments.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << m_outputCommitments[i].serialize();
    }

    oss << "]}";

    return oss.str();
}

PrivateAccountingRecord PrivateAccountingRecord::createRecord(
    PrivateAccountingRecordType type,
    PublicSupplyEffect supplyEffect,
    utils::Amount publicSupplyAmount,
    std::vector<PrivacyNullifier> inputNullifiers,
    std::vector<PrivacyCommitment> outputCommitments,
    std::string auditReference,
    std::int64_t timestamp
) {
    if (auditReference.empty()) {
        throw std::invalid_argument("Private accounting audit reference cannot be empty.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("Private accounting timestamp must be positive.");
    }

    const std::string proofHash = computeDevelopmentProofHash(
        type,
        supplyEffect,
        publicSupplyAmount,
        inputNullifiers,
        outputCommitments,
        auditReference
    );

    const std::string id = computeRecordId(
        type,
        proofHash,
        auditReference,
        timestamp
    );

    PrivateAccountingRecord record(
        id,
        type,
        supplyEffect,
        publicSupplyAmount,
        std::move(inputNullifiers),
        std::move(outputCommitments),
        std::move(auditReference),
        proofHash,
        timestamp
    );

    if (!record.isValid()) {
        throw std::logic_error("Generated PrivateAccountingRecord is invalid.");
    }

    return record;
}

std::string PrivateAccountingRecord::computeDevelopmentProofHash(
    PrivateAccountingRecordType type,
    PublicSupplyEffect supplyEffect,
    const utils::Amount& publicSupplyAmount,
    const std::vector<PrivacyNullifier>& inputNullifiers,
    const std::vector<PrivacyCommitment>& outputCommitments,
    const std::string& auditReference
) {
    std::ostringstream oss;

    /*
     * Development-only proof hash.
     *
     * Future production version:
     * Replace this with a real zero-knowledge proof object and verifier.
     */
    oss << "DevelopmentPrivateAccountingProof{"
        << "type=" << privateAccountingRecordTypeToString(type)
        << ";supplyEffect=" << publicSupplyEffectToString(supplyEffect)
        << ";publicSupplyAmountRaw=" << publicSupplyAmount.rawUnits()
        << ";auditReference=" << auditReference
        << ";inputNullifiers=[";

    for (std::size_t i = 0; i < inputNullifiers.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << inputNullifiers[i].nullifierHash();
    }

    oss << "];outputCommitments=[";

    for (std::size_t i = 0; i < outputCommitments.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << outputCommitments[i].commitmentHash();
    }

    oss << "]}";

    return hashString(oss.str());
}

std::string PrivateAccountingRecord::computeRecordId(
    PrivateAccountingRecordType type,
    const std::string& proofHash,
    const std::string& auditReference,
    std::int64_t timestamp
) {
    std::ostringstream oss;

    oss << "PrivateAccountingRecordId{"
        << "type=" << privateAccountingRecordTypeToString(type)
        << ";proofHash=" << proofHash
        << ";auditReference=" << auditReference
        << ";timestamp=" << timestamp
        << "}";

    return hashString(oss.str());
}

std::string PrivateAccountingRecord::hashString(const std::string& value) {
    char output[65] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));

    return std::string(output);
}

bool PrivateAccountingRecord::hasValidSupplyEffect() const {
    if (m_supplyEffect == PublicSupplyEffect::NO_SUPPLY_CHANGE) {
        return m_publicSupplyAmount.rawUnits() == 0;
    }

    if (m_supplyEffect == PublicSupplyEffect::SUPPLY_INCREASE ||
        m_supplyEffect == PublicSupplyEffect::SUPPLY_DECREASE) {
        return m_publicSupplyAmount.isPositive();
    }

    return false;
}

bool PrivateAccountingRecord::hasValidShapeForType() const {
    if (m_type == PrivateAccountingRecordType::PRIVATE_MINT) {
        return m_inputNullifiers.empty() &&
               !m_outputCommitments.empty() &&
               m_supplyEffect == PublicSupplyEffect::SUPPLY_INCREASE;
    }

    if (m_type == PrivateAccountingRecordType::PRIVATE_TRANSFER) {
        return !m_inputNullifiers.empty() &&
               !m_outputCommitments.empty() &&
               m_supplyEffect == PublicSupplyEffect::NO_SUPPLY_CHANGE;
    }

    if (m_type == PrivateAccountingRecordType::PRIVATE_BURN) {
        return !m_inputNullifiers.empty() &&
               m_outputCommitments.empty() &&
               m_supplyEffect == PublicSupplyEffect::SUPPLY_DECREASE;
    }

    return false;
}

bool PrivateAccountingRecord::hasValidInputsAndOutputs() const {
    if (hasDuplicateInputNullifiers()) {
        return false;
    }

    if (hasDuplicateOutputCommitments()) {
        return false;
    }

    for (const auto& nullifier : m_inputNullifiers) {
        if (!nullifier.isValid()) {
            return false;
        }
    }

    for (const auto& commitment : m_outputCommitments) {
        if (!commitment.isValid()) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::privacy