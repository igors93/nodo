#include "economics/ValidationWorkRecord.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

std::string validationWorkTypeToString(
    ValidationWorkType type
) {
    switch (type) {
        case ValidationWorkType::VALIDATE_TRANSACTION:
            return "VALIDATE_TRANSACTION";

        case ValidationWorkType::VERIFY_COIN_EXISTENCE:
            return "VERIFY_COIN_EXISTENCE";

        case ValidationWorkType::VERIFY_SIGNATURE:
            return "VERIFY_SIGNATURE";

        case ValidationWorkType::VALIDATE_BLOCK:
            return "VALIDATE_BLOCK";

        case ValidationWorkType::RESPOND_INTEGRITY_CHALLENGE:
            return "RESPOND_INTEGRITY_CHALLENGE";

        case ValidationWorkType::SERVE_HISTORICAL_BLOCK:
            return "SERVE_HISTORICAL_BLOCK";

        case ValidationWorkType::CONSENSUS_VOTE:
            return "CONSENSUS_VOTE";

        default:
            return "UNKNOWN";
    }
}

std::string validationWorkResultToString(
    ValidationWorkResult result
) {
    switch (result) {
        case ValidationWorkResult::ACCEPTED:
            return "ACCEPTED";

        case ValidationWorkResult::REJECTED:
            return "REJECTED";

        default:
            return "UNKNOWN";
    }
}

ValidationWorkRecord::ValidationWorkRecord()
    : m_validatorAddress(""),
      m_epoch(0),
      m_workType(ValidationWorkType::UNKNOWN),
      m_result(ValidationWorkResult::UNKNOWN),
      m_targetHash(""),
      m_evidenceHash(""),
      m_workWeight(0),
      m_timestamp(0) {}

ValidationWorkRecord::ValidationWorkRecord(
    std::string validatorAddress,
    std::uint64_t epoch,
    ValidationWorkType workType,
    ValidationWorkResult result,
    std::string targetHash,
    std::string evidenceHash,
    std::uint32_t workWeight,
    std::int64_t timestamp
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_epoch(epoch),
      m_workType(workType),
      m_result(result),
      m_targetHash(std::move(targetHash)),
      m_evidenceHash(std::move(evidenceHash)),
      m_workWeight(workWeight),
      m_timestamp(timestamp) {}

const std::string& ValidationWorkRecord::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ValidationWorkRecord::epoch() const {
    return m_epoch;
}

ValidationWorkType ValidationWorkRecord::workType() const {
    return m_workType;
}

ValidationWorkResult ValidationWorkRecord::result() const {
    return m_result;
}

const std::string& ValidationWorkRecord::targetHash() const {
    return m_targetHash;
}

const std::string& ValidationWorkRecord::evidenceHash() const {
    return m_evidenceHash;
}

std::uint32_t ValidationWorkRecord::workWeight() const {
    return m_workWeight;
}

std::int64_t ValidationWorkRecord::timestamp() const {
    return m_timestamp;
}

bool ValidationWorkRecord::isAccepted() const {
    return m_result == ValidationWorkResult::ACCEPTED;
}

bool ValidationWorkRecord::isRejected() const {
    return m_result == ValidationWorkResult::REJECTED;
}

bool ValidationWorkRecord::contributesToReward() const {
    return isValid() && isAccepted();
}

bool ValidationWorkRecord::isValid() const {
    if (m_validatorAddress.empty()) {
        return false;
    }

    if (m_epoch == 0) {
        return false;
    }

    if (m_workType == ValidationWorkType::UNKNOWN) {
        return false;
    }

    if (m_result == ValidationWorkResult::UNKNOWN) {
        return false;
    }

    if (m_targetHash.empty()) {
        return false;
    }

    if (m_evidenceHash.empty()) {
        return false;
    }

    if (m_workWeight == 0) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    return true;
}

std::string ValidationWorkRecord::serialize() const {
    std::ostringstream oss;

    oss << "ValidationWorkRecord{"
        << "validator=" << m_validatorAddress
        << ";epoch=" << m_epoch
        << ";workType=" << validationWorkTypeToString(m_workType)
        << ";result=" << validationWorkResultToString(m_result)
        << ";targetHash=" << m_targetHash
        << ";evidenceHash=" << m_evidenceHash
        << ";workWeight=" << m_workWeight
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

} // namespace nodo::economics
