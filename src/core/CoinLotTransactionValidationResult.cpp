#include "core/CoinLotTransactionValidationResult.hpp"

#include <sstream>
#include <utility>

namespace nodo::core {

CoinLotTransactionValidationResult CoinLotTransactionValidationResult::valid() {
    return CoinLotTransactionValidationResult(
        true,
        "VALID"
    );
}

CoinLotTransactionValidationResult CoinLotTransactionValidationResult::invalid(
    std::string reason
) {
    if (reason.empty()) {
        reason = "INVALID";
    }

    return CoinLotTransactionValidationResult(
        false,
        std::move(reason)
    );
}

bool CoinLotTransactionValidationResult::success() const {
    return m_success;
}

const std::string& CoinLotTransactionValidationResult::reason() const {
    return m_reason;
}

std::string CoinLotTransactionValidationResult::serialize() const {
    std::ostringstream oss;

    oss << "CoinLotTransactionValidationResult{"
        << "success=" << (m_success ? "true" : "false")
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

CoinLotTransactionValidationResult::CoinLotTransactionValidationResult(
    bool success,
    std::string reason
)
    : m_success(success),
      m_reason(std::move(reason)) {}

} // namespace nodo::core
