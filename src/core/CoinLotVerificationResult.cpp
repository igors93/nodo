#include "core/CoinLotVerificationResult.hpp"

#include <sstream>
#include <utility>

namespace nodo::core {

CoinLotVerificationResult CoinLotVerificationResult::valid() {
    return CoinLotVerificationResult(
        true,
        "VALID"
    );
}

CoinLotVerificationResult CoinLotVerificationResult::invalid(
    std::string reason
) {
    if (reason.empty()) {
        reason = "INVALID";
    }

    return CoinLotVerificationResult(
        false,
        std::move(reason)
    );
}

bool CoinLotVerificationResult::success() const {
    return m_success;
}

const std::string& CoinLotVerificationResult::reason() const {
    return m_reason;
}

std::string CoinLotVerificationResult::serialize() const {
    std::ostringstream oss;

    oss << "CoinLotVerificationResult{"
        << "success=" << (m_success ? "true" : "false")
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

CoinLotVerificationResult::CoinLotVerificationResult(
    bool success,
    std::string reason
)
    : m_success(success),
      m_reason(std::move(reason)) {}

} // namespace nodo::core
