#include "economics/ProtectionEpoch.hpp"

#include "serialization/FieldCodec.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

ProtectionEpoch::ProtectionEpoch()
    : m_epochId(0),
      m_startBlock(0),
      m_endBlock(0),
      m_feesCollected(utils::Amount::fromRawUnits(0)),
      m_emissionCap(utils::Amount::fromRawUnits(0)),
      m_workDemandBasisPoints(0),
      m_targetWorkWeight(0),
      m_acceptedWorkWeight(0),
      m_policyVersion(""),
      m_evidenceBlockHash("") {}

ProtectionEpoch::ProtectionEpoch(
    std::uint64_t epochId,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    utils::Amount feesCollected,
    utils::Amount emissionCap,
    std::uint32_t workDemandBasisPoints
)
    : m_epochId(epochId),
      m_startBlock(startBlock),
      m_endBlock(endBlock),
      m_feesCollected(feesCollected),
      m_emissionCap(emissionCap),
      m_workDemandBasisPoints(workDemandBasisPoints),
      m_targetWorkWeight(0),
      m_acceptedWorkWeight(0),
      m_policyVersion(""),
      m_evidenceBlockHash("") {}

ProtectionEpoch::ProtectionEpoch(
    std::uint64_t epochId,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    utils::Amount feesCollected,
    utils::Amount emissionCap,
    std::uint32_t workDemandBasisPoints,
    std::uint64_t targetWorkWeight,
    std::uint64_t acceptedWorkWeight,
    std::string policyVersion,
    std::string evidenceBlockHash
)
    : m_epochId(epochId),
      m_startBlock(startBlock),
      m_endBlock(endBlock),
      m_feesCollected(feesCollected),
      m_emissionCap(emissionCap),
      m_workDemandBasisPoints(workDemandBasisPoints),
      m_targetWorkWeight(targetWorkWeight),
      m_acceptedWorkWeight(acceptedWorkWeight),
      m_policyVersion(std::move(policyVersion)),
      m_evidenceBlockHash(std::move(evidenceBlockHash)) {}

std::uint64_t ProtectionEpoch::epochId() const {
    return m_epochId;
}

std::uint64_t ProtectionEpoch::startBlock() const {
    return m_startBlock;
}

std::uint64_t ProtectionEpoch::endBlock() const {
    return m_endBlock;
}

utils::Amount ProtectionEpoch::feesCollected() const {
    return m_feesCollected;
}

utils::Amount ProtectionEpoch::emissionCap() const {
    return m_emissionCap;
}

std::uint32_t ProtectionEpoch::workDemandBasisPoints() const {
    return m_workDemandBasisPoints;
}

std::uint64_t ProtectionEpoch::targetWorkWeight() const { return m_targetWorkWeight; }
std::uint64_t ProtectionEpoch::acceptedWorkWeight() const { return m_acceptedWorkWeight; }
const std::string& ProtectionEpoch::policyVersion() const { return m_policyVersion; }
const std::string& ProtectionEpoch::evidenceBlockHash() const { return m_evidenceBlockHash; }

bool ProtectionEpoch::hasCanonicalSettlementMetadata() const {
    return m_targetWorkWeight > 0 &&
           m_acceptedWorkWeight <= m_targetWorkWeight &&
           !m_policyVersion.empty() &&
           !m_evidenceBlockHash.empty();
}

bool ProtectionEpoch::isValid() const {
    if (m_epochId == 0) {
        return false;
    }

    if (m_endBlock < m_startBlock) {
        return false;
    }

    if (m_feesCollected.isNegative()) {
        return false;
    }

    if (m_emissionCap.isNegative()) {
        return false;
    }

    if (m_workDemandBasisPoints > BASIS_POINTS_DENOMINATOR) {
        return false;
    }

    if (hasCanonicalSettlementMetadata()) {
        const unsigned __int128 expected =
            (static_cast<unsigned __int128>(m_acceptedWorkWeight) *
             BASIS_POINTS_DENOMINATOR) / m_targetWorkWeight;
        const std::uint32_t bounded = expected >= BASIS_POINTS_DENOMINATOR
            ? BASIS_POINTS_DENOMINATOR
            : static_cast<std::uint32_t>(expected);
        if (bounded != m_workDemandBasisPoints) return false;
    } else if (m_targetWorkWeight != 0 || m_acceptedWorkWeight != 0 ||
               !m_policyVersion.empty() || !m_evidenceBlockHash.empty()) {
        return false;
    }

    return true;
}

utils::Amount ProtectionEpoch::securityEmission() const {
    if (!isValid()) {
        throw std::logic_error("Invalid protection epoch.");
    }

    return utils::Amount::fromRawUnits(
        multiplyDivideRawUnits(
            m_emissionCap.rawUnits(),
            m_workDemandBasisPoints,
            BASIS_POINTS_DENOMINATOR
        )
    );
}

utils::Amount ProtectionEpoch::rewardPool() const {
    return m_feesCollected + securityEmission();
}

std::string ProtectionEpoch::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionEpoch{"
        << "epochId=" << m_epochId
        << ";startBlock=" << m_startBlock
        << ";endBlock=" << m_endBlock
        << ";feesCollectedRaw=" << m_feesCollected.rawUnits()
        << ";emissionCapRaw=" << m_emissionCap.rawUnits()
        << ";workDemandBasisPoints=" << m_workDemandBasisPoints
        << ";targetWorkWeight=" << m_targetWorkWeight
        << ";acceptedWorkWeight=" << m_acceptedWorkWeight
        << ";policyVersion=" << m_policyVersion
        << ";evidenceBlockHash=" << m_evidenceBlockHash
        << ";securityEmissionRaw=" << (isValid() ? securityEmission().rawUnits() : 0)
        << ";rewardPoolRaw=" << (isValid() ? rewardPool().rawUnits() : 0)
        << "}";

    return oss.str();
}

ProtectionEpoch ProtectionEpoch::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("ProtectionEpoch{", 0) != 0) {
        throw std::invalid_argument("Serialized data is not a ProtectionEpoch.");
    }
    const auto field = [&serialized](const std::string& name) {
        return serialization::FieldCodec::extractField(serialized, name);
    };
    ProtectionEpoch epoch(
        static_cast<std::uint64_t>(std::stoull(field("epochId"))),
        static_cast<std::uint64_t>(std::stoull(field("startBlock"))),
        static_cast<std::uint64_t>(std::stoull(field("endBlock"))),
        utils::Amount::fromRawUnits(std::stoll(field("feesCollectedRaw"))),
        utils::Amount::fromRawUnits(std::stoll(field("emissionCapRaw"))),
        static_cast<std::uint32_t>(std::stoul(field("workDemandBasisPoints"))),
        static_cast<std::uint64_t>(std::stoull(field("targetWorkWeight"))),
        static_cast<std::uint64_t>(std::stoull(field("acceptedWorkWeight"))),
        field("policyVersion"),
        field("evidenceBlockHash")
    );
    if (!epoch.isValid() || epoch.serialize() != serialized) {
        throw std::invalid_argument("Non-canonical ProtectionEpoch rejected.");
    }
    return epoch;
}

std::int64_t ProtectionEpoch::multiplyDivideRawUnits(
    std::int64_t rawUnits,
    std::uint64_t numerator,
    std::uint64_t denominator
) {
    if (rawUnits < 0) {
        throw std::invalid_argument("Raw units cannot be negative.");
    }

    if (denominator == 0) {
        throw std::invalid_argument("Denominator cannot be zero.");
    }

    const std::uint64_t raw =
        static_cast<std::uint64_t>(rawUnits);

    const std::uint64_t quotient =
        raw / denominator;

    const std::uint64_t remainder =
        raw % denominator;

    if (numerator != 0 &&
        quotient > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / numerator) {
        throw std::overflow_error("Protection epoch emission calculation overflow.");
    }

    const std::uint64_t result =
        quotient * numerator +
        (remainder * numerator) / denominator;

    if (result > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Protection epoch emission calculation overflow.");
    }

    return static_cast<std::int64_t>(result);
}

} // namespace nodo::economics
