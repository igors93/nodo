#include "economics/GenesisRewardRecord.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

namespace {

std::string extractField(
    const std::string& serialized,
    const std::string& key
) {
    const std::string prefix = key + "=";
    const std::size_t start = serialized.find(prefix);

    if (start == std::string::npos) {
        throw std::invalid_argument("Missing GenesisRewardRecord field: " + key);
    }

    const std::size_t valueStart = start + prefix.size();
    std::size_t valueEnd = serialized.find(';', valueStart);

    if (valueEnd == std::string::npos) {
        valueEnd = serialized.find('}', valueStart);
    }

    if (valueEnd == std::string::npos || valueEnd <= valueStart) {
        throw std::invalid_argument("Invalid GenesisRewardRecord field: " + key);
    }

    return serialized.substr(valueStart, valueEnd - valueStart);
}

} // namespace

std::string genesisRewardReasonToString(
    GenesisRewardReason reason
) {
    switch (reason) {
        case GenesisRewardReason::NETWORK_PROTECTION:
            return "NETWORK_PROTECTION";

        case GenesisRewardReason::BOOTSTRAP_PROTECTION:
            return "BOOTSTRAP_PROTECTION";

        case GenesisRewardReason::STORAGE_PROTECTION:
            return "STORAGE_PROTECTION";

        case GenesisRewardReason::CHALLENGE_RESPONSE:
            return "CHALLENGE_RESPONSE";

        default:
            return "UNKNOWN";
    }
}

GenesisRewardReason genesisRewardReasonFromString(
    const std::string& value
) {
    if (value == "NETWORK_PROTECTION") {
        return GenesisRewardReason::NETWORK_PROTECTION;
    }

    if (value == "BOOTSTRAP_PROTECTION") {
        return GenesisRewardReason::BOOTSTRAP_PROTECTION;
    }

    if (value == "STORAGE_PROTECTION") {
        return GenesisRewardReason::STORAGE_PROTECTION;
    }

    if (value == "CHALLENGE_RESPONSE") {
        return GenesisRewardReason::CHALLENGE_RESPONSE;
    }

    throw std::invalid_argument("Unknown GenesisRewardReason: " + value);
}

GenesisRewardRecord::GenesisRewardRecord()
    : m_epoch(0),
      m_validatorAddress(""),
      m_amount(utils::Amount::fromRawUnits(0)),
      m_reason(GenesisRewardReason::UNKNOWN),
      m_workSummaryHash(""),
      m_policyVersion(""),
      m_acceptedBlockHash(""),
      m_timestamp(0) {}

GenesisRewardRecord::GenesisRewardRecord(
    std::uint64_t epoch,
    std::string validatorAddress,
    utils::Amount amount,
    GenesisRewardReason reason,
    std::string workSummaryHash,
    std::string policyVersion,
    std::string acceptedBlockHash,
    std::int64_t timestamp
)
    : m_epoch(epoch),
      m_validatorAddress(std::move(validatorAddress)),
      m_amount(amount),
      m_reason(reason),
      m_workSummaryHash(std::move(workSummaryHash)),
      m_policyVersion(std::move(policyVersion)),
      m_acceptedBlockHash(std::move(acceptedBlockHash)),
      m_timestamp(timestamp) {}

std::uint64_t GenesisRewardRecord::epoch() const {
    return m_epoch;
}

const std::string& GenesisRewardRecord::validatorAddress() const {
    return m_validatorAddress;
}

utils::Amount GenesisRewardRecord::amount() const {
    return m_amount;
}

GenesisRewardReason GenesisRewardRecord::reason() const {
    return m_reason;
}

const std::string& GenesisRewardRecord::workSummaryHash() const {
    return m_workSummaryHash;
}

const std::string& GenesisRewardRecord::policyVersion() const {
    return m_policyVersion;
}

const std::string& GenesisRewardRecord::acceptedBlockHash() const {
    return m_acceptedBlockHash;
}

std::int64_t GenesisRewardRecord::timestamp() const {
    return m_timestamp;
}

bool GenesisRewardRecord::isValid() const {
    if (m_epoch == 0) {
        return false;
    }

    if (m_validatorAddress.empty()) {
        return false;
    }

    if (!m_amount.isPositive()) {
        return false;
    }

    if (m_reason == GenesisRewardReason::UNKNOWN) {
        return false;
    }

    if (m_workSummaryHash.empty()) {
        return false;
    }

    if (m_policyVersion.empty()) {
        return false;
    }

    if (m_acceptedBlockHash.empty()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    return true;
}

std::string GenesisRewardRecord::deterministicId() const {
    if (!isValid()) {
        throw std::logic_error("Invalid GenesisRewardRecord has no deterministic id.");
    }

    char output[NODO_HASH_BUFFER_SIZE] = {0};

    const std::string payload =
        "NODO_GENESIS_REWARD_RECORD_V1|" + serialize();

    nodo_hash_string(
        payload.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output);
}

core::CoinLot GenesisRewardRecord::createRewardCoinLot(
    std::uint64_t createdAtBlock
) const {
    if (!isValid()) {
        throw std::logic_error("Invalid GenesisRewardRecord cannot create a coin lot.");
    }

    const std::string rewardId =
        deterministicId();

    return core::CoinLot(
        "reward_lot_" + rewardId,
        rewardId,
        m_validatorAddress,
        m_amount,
        core::CoinLotStatus::AVAILABLE,
        createdAtBlock,
        0,
        m_timestamp
    );
}

std::string GenesisRewardRecord::serialize() const {
    std::ostringstream oss;

    oss << "GenesisRewardRecord{"
        << "epoch=" << m_epoch
        << ";validator=" << m_validatorAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";reason=" << genesisRewardReasonToString(m_reason)
        << ";workSummaryHash=" << m_workSummaryHash
        << ";policyVersion=" << m_policyVersion
        << ";acceptedBlockHash=" << m_acceptedBlockHash
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

GenesisRewardRecord GenesisRewardRecord::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("GenesisRewardRecord{", 0) != 0) {
        throw std::invalid_argument("Serialized data is not a GenesisRewardRecord.");
    }

    GenesisRewardRecord record(
        static_cast<std::uint64_t>(
            std::stoull(
                extractField(serialized, "epoch")
            )
        ),
        extractField(serialized, "validator"),
        utils::Amount::fromRawUnits(
            std::stoll(
                extractField(serialized, "amountRaw")
            )
        ),
        genesisRewardReasonFromString(
            extractField(serialized, "reason")
        ),
        extractField(serialized, "workSummaryHash"),
        extractField(serialized, "policyVersion"),
        extractField(serialized, "acceptedBlockHash"),
        std::stoll(
            extractField(serialized, "timestamp")
        )
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Deserialized GenesisRewardRecord is invalid.");
    }

    if (record.serialize() != serialized) {
        throw std::logic_error("GenesisRewardRecord round-trip serialization mismatch.");
    }

    return record;
}

} // namespace nodo::economics
