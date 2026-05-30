#include "core/LedgerRecord.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

std::string ledgerRecordTypeToString(LedgerRecordType type) {
    switch (type) {
        case LedgerRecordType::MINT:
            return "MINT";
        case LedgerRecordType::TRANSACTION:
            return "TRANSACTION";
        case LedgerRecordType::PRIVATE_ACCOUNTING:
            return "PRIVATE_ACCOUNTING";
        case LedgerRecordType::VALIDATION_WORK:
            return "VALIDATION_WORK";
        case LedgerRecordType::VALIDATOR_SCORE:
            return "VALIDATOR_SCORE";
        case LedgerRecordType::PROTECTION_EPOCH:
            return "PROTECTION_EPOCH";
        case LedgerRecordType::GENESIS_REWARD:
            return "GENESIS_REWARD";
        case LedgerRecordType::VALIDATOR_PENALTY:
            return "VALIDATOR_PENALTY";
        default:
            return "UNKNOWN";
    }
}

LedgerRecordType ledgerRecordTypeFromString(
    const std::string& value
) {
    if (value == "MINT") {
        return LedgerRecordType::MINT;
    }

    if (value == "TRANSACTION") {
        return LedgerRecordType::TRANSACTION;
    }

    if (value == "PRIVATE_ACCOUNTING") {
        return LedgerRecordType::PRIVATE_ACCOUNTING;
    }

    if (value == "VALIDATION_WORK") {
        return LedgerRecordType::VALIDATION_WORK;
    }

    if (value == "VALIDATOR_SCORE") {
        return LedgerRecordType::VALIDATOR_SCORE;
    }

    if (value == "PROTECTION_EPOCH") {
        return LedgerRecordType::PROTECTION_EPOCH;
    }

    if (value == "GENESIS_REWARD") {
        return LedgerRecordType::GENESIS_REWARD;
    }

    if (value == "VALIDATOR_PENALTY") {
        return LedgerRecordType::VALIDATOR_PENALTY;
    }

    throw std::invalid_argument("Unknown LedgerRecordType: " + value);
}

LedgerRecord::LedgerRecord(
    std::string id,
    LedgerRecordType type,
    std::string sourceId,
    std::string payload,
    std::string payloadHash,
    std::int64_t timestamp
)
    : m_id(std::move(id)),
      m_type(type),
      m_sourceId(std::move(sourceId)),
      m_payload(std::move(payload)),
      m_payloadHash(std::move(payloadHash)),
      m_timestamp(timestamp) {}

LedgerRecord LedgerRecord::fromPersistedFields(
    std::string id,
    LedgerRecordType type,
    std::string sourceId,
    std::string payload,
    std::string payloadHash,
    std::int64_t timestamp
) {
    LedgerRecord record(
        std::move(id),
        type,
        std::move(sourceId),
        std::move(payload),
        std::move(payloadHash),
        timestamp
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Invalid persisted LedgerRecord rejected.");
    }

    return record;
}

LedgerRecord LedgerRecord::fromMintRecord(
    const economics::MintRecord& mintRecord,
    std::int64_t timestamp
) {
    if (!mintRecord.isValid()) {
        throw std::invalid_argument("Invalid MintRecord rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = mintRecord.serialize();
    const std::string payloadHash = hashPayload(payload);

    const std::string recordId = computeRecordId(
        LedgerRecordType::MINT,
        mintRecord.id(),
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::MINT,
        mintRecord.id(),
        payload,
        payloadHash,
        timestamp
    );
}

LedgerRecord LedgerRecord::fromTransaction(
    const Transaction& transaction,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    std::int64_t timestamp
) {
    if (!transaction.isStructurallyValid(policy, context)) {
        throw std::invalid_argument("Invalid Transaction rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = transaction.serialize();
    const std::string payloadHash = hashPayload(payload);

    const std::string recordId = computeRecordId(
        LedgerRecordType::TRANSACTION,
        transaction.id(),
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::TRANSACTION,
        transaction.id(),
        payload,
        payloadHash,
        timestamp
    );
}

LedgerRecord LedgerRecord::fromPrivateAccountingRecord(
    const privacy::PrivateAccountingRecord& privateAccountingRecord,
    std::int64_t timestamp
) {
    if (!privateAccountingRecord.isValid()) {
        throw std::invalid_argument("Invalid PrivateAccountingRecord rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = privateAccountingRecord.serialize();
    const std::string payloadHash = hashPayload(payload);

    const std::string recordId = computeRecordId(
        LedgerRecordType::PRIVATE_ACCOUNTING,
        privateAccountingRecord.id(),
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::PRIVATE_ACCOUNTING,
        privateAccountingRecord.id(),
        payload,
        payloadHash,
        timestamp
    );
}

LedgerRecord LedgerRecord::fromValidationWorkRecord(
    const economics::ValidationWorkRecord& validationWorkRecord,
    std::int64_t timestamp
) {
    if (!validationWorkRecord.isValid()) {
        throw std::invalid_argument("Invalid ValidationWorkRecord rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = validationWorkRecord.serialize();
    const std::string payloadHash = hashPayload(payload);
    const std::string sourceId =
        computeDerivedSourceId("validation_work", payloadHash);

    const std::string recordId = computeRecordId(
        LedgerRecordType::VALIDATION_WORK,
        sourceId,
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::VALIDATION_WORK,
        sourceId,
        payload,
        payloadHash,
        timestamp
    );
}

LedgerRecord LedgerRecord::fromValidatorScoreRecord(
    const economics::ValidatorScoreRecord& validatorScoreRecord,
    std::int64_t timestamp
) {
    if (!validatorScoreRecord.isValid()) {
        throw std::invalid_argument("Invalid ValidatorScoreRecord rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = validatorScoreRecord.serialize();
    const std::string payloadHash = hashPayload(payload);
    const std::string sourceId =
        computeDerivedSourceId("validator_score", payloadHash);

    const std::string recordId = computeRecordId(
        LedgerRecordType::VALIDATOR_SCORE,
        sourceId,
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::VALIDATOR_SCORE,
        sourceId,
        payload,
        payloadHash,
        timestamp
    );
}

LedgerRecord LedgerRecord::fromProtectionEpoch(
    const economics::ProtectionEpoch& protectionEpoch,
    std::int64_t timestamp
) {
    if (!protectionEpoch.isValid()) {
        throw std::invalid_argument("Invalid ProtectionEpoch rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = protectionEpoch.serialize();
    const std::string payloadHash = hashPayload(payload);
    const std::string sourceId =
        "protection_epoch_" + std::to_string(protectionEpoch.epochId());

    const std::string recordId = computeRecordId(
        LedgerRecordType::PROTECTION_EPOCH,
        sourceId,
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::PROTECTION_EPOCH,
        sourceId,
        payload,
        payloadHash,
        timestamp
    );
}

LedgerRecord LedgerRecord::fromGenesisRewardRecord(
    const economics::GenesisRewardRecord& genesisRewardRecord,
    std::int64_t timestamp
) {
    if (!genesisRewardRecord.isValid()) {
        throw std::invalid_argument("Invalid GenesisRewardRecord rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = genesisRewardRecord.serialize();
    const std::string payloadHash = hashPayload(payload);
    const std::string sourceId = genesisRewardRecord.deterministicId();

    const std::string recordId = computeRecordId(
        LedgerRecordType::GENESIS_REWARD,
        sourceId,
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::GENESIS_REWARD,
        sourceId,
        payload,
        payloadHash,
        timestamp
    );
}

LedgerRecord LedgerRecord::fromValidatorPenaltyRecord(
    const economics::ValidatorPenaltyRecord& validatorPenaltyRecord,
    std::int64_t timestamp
) {
    if (!validatorPenaltyRecord.isValid()) {
        throw std::invalid_argument("Invalid ValidatorPenaltyRecord rejected by LedgerRecord.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("LedgerRecord timestamp must be positive.");
    }

    const std::string payload = validatorPenaltyRecord.serialize();
    const std::string payloadHash = hashPayload(payload);
    const std::string sourceId = validatorPenaltyRecord.deterministicId();

    const std::string recordId = computeRecordId(
        LedgerRecordType::VALIDATOR_PENALTY,
        sourceId,
        payloadHash,
        timestamp
    );

    return LedgerRecord(
        recordId,
        LedgerRecordType::VALIDATOR_PENALTY,
        sourceId,
        payload,
        payloadHash,
        timestamp
    );
}

const std::string& LedgerRecord::id() const {
    return m_id;
}

LedgerRecordType LedgerRecord::type() const {
    return m_type;
}

const std::string& LedgerRecord::sourceId() const {
    return m_sourceId;
}

const std::string& LedgerRecord::payload() const {
    return m_payload;
}

const std::string& LedgerRecord::payloadHash() const {
    return m_payloadHash;
}

std::int64_t LedgerRecord::timestamp() const {
    return m_timestamp;
}

bool LedgerRecord::isValid() const {
    if (m_id.empty() ||
        m_sourceId.empty() ||
        m_payload.empty() ||
        m_payloadHash.empty()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (m_payloadHash != hashPayload(m_payload)) {
        return false;
    }

    if (m_id != computeRecordId(
            m_type,
            m_sourceId,
            m_payloadHash,
            m_timestamp
        )) {
        return false;
    }

    return true;
}

std::string LedgerRecord::serialize() const {
    std::ostringstream oss;

    oss << "LedgerRecord{"
        << "id=" << m_id
        << ";type=" << ledgerRecordTypeToString(m_type)
        << ";sourceId=" << m_sourceId
        << ";payloadHash=" << m_payloadHash
        << ";timestamp=" << m_timestamp
        << ";payload=" << m_payload
        << "}";

    return oss.str();
}

std::string LedgerRecord::hashPayload(
    const std::string& payload
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(payload.c_str(), output, sizeof(output));
    return std::string(output);
}

std::string LedgerRecord::computeRecordId(
    LedgerRecordType type,
    const std::string& sourceId,
    const std::string& payloadHash,
    std::int64_t timestamp
) {
    std::ostringstream oss;

    oss << "LedgerRecordId{"
        << "type=" << ledgerRecordTypeToString(type)
        << ";sourceId=" << sourceId
        << ";payloadHash=" << payloadHash
        << ";timestamp=" << timestamp
        << "}";

    return hashPayload(oss.str());
}

std::string LedgerRecord::computeDerivedSourceId(
    const std::string& sourcePrefix,
    const std::string& payloadHash
) {
    return sourcePrefix + "_" + payloadHash;
}

} // namespace nodo::core
