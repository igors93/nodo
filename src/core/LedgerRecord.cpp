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

        default:
            return "UNKNOWN";
    }
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
    if (m_id.empty()) {
        return false;
    }

    if (m_sourceId.empty()) {
        return false;
    }

    if (m_payload.empty()) {
        return false;
    }

    if (m_payloadHash.empty()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (m_payloadHash != hashPayload(m_payload)) {
        return false;
    }

    if (m_id != computeRecordId(m_type, m_sourceId, m_payloadHash, m_timestamp)) {
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

std::string LedgerRecord::hashPayload(const std::string& payload) {
    char output[65] = {0};
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

} // namespace nodo::core