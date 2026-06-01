#include "economics/MintAuthorization.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

MintAuthorization::MintAuthorization()
    : m_authorizationId(""),
      m_policyVersion(""),
      m_epoch(0),
      m_expiresAtEpoch(0),
      m_maxMintAmount(utils::Amount::fromRawUnits(0)),
      m_reason(""),
      m_approvedBy("") {}

MintAuthorization::MintAuthorization(
    std::string authorizationId,
    std::string policyVersion,
    std::uint64_t epoch,
    std::uint64_t expiresAtEpoch,
    utils::Amount maxMintAmount,
    std::string reason,
    std::string approvedBy
)
    : m_authorizationId(std::move(authorizationId)),
      m_policyVersion(std::move(policyVersion)),
      m_epoch(epoch),
      m_expiresAtEpoch(expiresAtEpoch),
      m_maxMintAmount(maxMintAmount),
      m_reason(std::move(reason)),
      m_approvedBy(std::move(approvedBy)) {}

const std::string& MintAuthorization::authorizationId() const { return m_authorizationId; }
const std::string& MintAuthorization::policyVersion() const { return m_policyVersion; }
std::uint64_t MintAuthorization::epoch() const { return m_epoch; }
std::uint64_t MintAuthorization::expiresAtEpoch() const { return m_expiresAtEpoch; }
utils::Amount MintAuthorization::maxMintAmount() const { return m_maxMintAmount; }
const std::string& MintAuthorization::reason() const { return m_reason; }
const std::string& MintAuthorization::approvedBy() const { return m_approvedBy; }

bool MintAuthorization::isValid() const {
    return rejectionReason().empty();
}

bool MintAuthorization::isActiveAtEpoch(std::uint64_t currentEpoch) const {
    return currentEpoch >= m_epoch && currentEpoch <= m_expiresAtEpoch;
}

std::string MintAuthorization::rejectionReason() const {
    if (m_authorizationId.empty()) {
        return "MintAuthorization rejected: authorizationId is empty.";
    }
    if (m_policyVersion.empty()) {
        return "MintAuthorization rejected: policyVersion is empty.";
    }
    if (!m_maxMintAmount.isPositive()) {
        return "MintAuthorization rejected: maxMintAmount must be positive, got " +
               std::to_string(m_maxMintAmount.rawUnits()) + ".";
    }
    if (m_reason.empty()) {
        return "MintAuthorization rejected: reason is empty.";
    }
    if (m_approvedBy.empty()) {
        return "MintAuthorization rejected: approvedBy is empty.";
    }
    if (m_expiresAtEpoch < m_epoch) {
        return "MintAuthorization rejected: expiresAtEpoch (" +
               std::to_string(m_expiresAtEpoch) +
               ") is before epoch (" + std::to_string(m_epoch) + ").";
    }
    return "";
}

std::string MintAuthorization::serialize() const {
    std::ostringstream oss;
    oss << "MintAuthorization{"
        << "authorizationId=" << m_authorizationId
        << ";policyVersion=" << m_policyVersion
        << ";epoch=" << m_epoch
        << ";expiresAtEpoch=" << m_expiresAtEpoch
        << ";maxMintAmountRaw=" << m_maxMintAmount.rawUnits()
        << ";reason=" << m_reason
        << ";approvedBy=" << m_approvedBy
        << "}";
    return oss.str();
}


MintAuthorization MintAuthorization::createGenesisAuthorization(
    const MonetaryPolicy& policy,
    const std::string& authorizationId,
    utils::Amount maxMintAmount
) {
    return MintAuthorization(
        authorizationId,
        policy.policyVersion(),
        0,   // genesis epoch
        0,   // single-epoch authorization valid only at epoch 0
        maxMintAmount,
        "genesis allocation",
        "GENESIS"
    );
}

} // namespace nodo::economics
