#ifndef NODO_ECONOMICS_VALIDATION_WORK_RECORD_HPP
#define NODO_ECONOMICS_VALIDATION_WORK_RECORD_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * ValidationWorkRecord records useful protection work performed by a validator.
 *
 * In simple terms:
 * The validator does not earn because it exists.
 * The validator becomes eligible for rewards because it did useful work.
 */
enum class ValidationWorkType {
    UNKNOWN,
    VALIDATE_TRANSACTION,
    VERIFY_COIN_EXISTENCE,
    VERIFY_SIGNATURE,
    VALIDATE_BLOCK,
    RESPOND_INTEGRITY_CHALLENGE,
    SERVE_HISTORICAL_BLOCK,
    CONSENSUS_VOTE
};

enum class ValidationWorkResult {
    UNKNOWN,
    ACCEPTED,
    REJECTED
};

std::string validationWorkTypeToString(
    ValidationWorkType type
);

ValidationWorkType validationWorkTypeFromString(
    const std::string& value
);

std::string validationWorkResultToString(
    ValidationWorkResult result
);

ValidationWorkResult validationWorkResultFromString(
    const std::string& value
);

class ValidationWorkRecord {
public:
    ValidationWorkRecord();

    ValidationWorkRecord(
        std::string validatorAddress,
        std::uint64_t epoch,
        ValidationWorkType workType,
        ValidationWorkResult result,
        std::string targetHash,
        std::string evidenceHash,
        std::uint32_t workWeight,
        std::int64_t timestamp
    );

    const std::string& validatorAddress() const;
    std::uint64_t epoch() const;
    ValidationWorkType workType() const;
    ValidationWorkResult result() const;
    const std::string& targetHash() const;
    const std::string& evidenceHash() const;
    std::uint32_t workWeight() const;
    std::int64_t timestamp() const;

    bool isAccepted() const;
    bool isRejected() const;

    /*
     * A work record contributes to reward only if the work was accepted and the
     * record is structurally valid.
     */
    bool contributesToReward() const;

    bool isValid() const;

    std::string serialize() const;

    static ValidationWorkRecord deserialize(
        const std::string& serialized
    );

private:
    std::string m_validatorAddress;
    std::uint64_t m_epoch;
    ValidationWorkType m_workType;
    ValidationWorkResult m_result;
    std::string m_targetHash;
    std::string m_evidenceHash;
    std::uint32_t m_workWeight;
    std::int64_t m_timestamp;
};

} // namespace nodo::economics

#endif
