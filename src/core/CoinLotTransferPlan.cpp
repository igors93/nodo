#include "core/CoinLotTransferPlan.hpp"

#include <sstream>
#include <set>
#include <utility>

namespace nodo::core {

CoinLotTransferPlan::CoinLotTransferPlan(
    std::string transactionId,
    std::string senderAddress,
    std::string recipientAddress,
    std::vector<std::string> inputLotIds,
    std::vector<CoinLot> outputLots,
    utils::Amount transferAmount,
    utils::Amount feeAmount,
    utils::Amount changeAmount,
    utils::Amount totalInputAmount
)
    : m_transactionId(std::move(transactionId)),
      m_senderAddress(std::move(senderAddress)),
      m_recipientAddress(std::move(recipientAddress)),
      m_inputLotIds(std::move(inputLotIds)),
      m_outputLots(std::move(outputLots)),
      m_transferAmount(transferAmount),
      m_feeAmount(feeAmount),
      m_changeAmount(changeAmount),
      m_totalInputAmount(totalInputAmount) {}

const std::string& CoinLotTransferPlan::transactionId() const {
    return m_transactionId;
}

const std::string& CoinLotTransferPlan::senderAddress() const {
    return m_senderAddress;
}

const std::string& CoinLotTransferPlan::recipientAddress() const {
    return m_recipientAddress;
}

const std::vector<std::string>& CoinLotTransferPlan::inputLotIds() const {
    return m_inputLotIds;
}

const std::vector<CoinLot>& CoinLotTransferPlan::outputLots() const {
    return m_outputLots;
}

utils::Amount CoinLotTransferPlan::transferAmount() const {
    return m_transferAmount;
}

utils::Amount CoinLotTransferPlan::feeAmount() const {
    return m_feeAmount;
}

utils::Amount CoinLotTransferPlan::changeAmount() const {
    return m_changeAmount;
}

utils::Amount CoinLotTransferPlan::totalInputAmount() const {
    return m_totalInputAmount;
}

utils::Amount CoinLotTransferPlan::totalOutputAmount() const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& outputLot : m_outputLots) {
        total = total + outputLot.amount();
    }

    return total;
}

utils::Amount CoinLotTransferPlan::totalDebitAmount() const {
    return m_transferAmount + m_feeAmount;
}

bool CoinLotTransferPlan::isValid() const {
    if (m_transactionId.empty()) {
        return false;
    }

    if (m_senderAddress.empty() || m_recipientAddress.empty()) {
        return false;
    }

    if (m_senderAddress == m_recipientAddress) {
        return false;
    }

    if (m_inputLotIds.empty()) {
        return false;
    }

    if (m_outputLots.empty()) {
        return false;
    }

    if (!m_transferAmount.isPositive()) {
        return false;
    }

    if (m_feeAmount.isNegative()) {
        return false;
    }

    if (m_changeAmount.isNegative()) {
        return false;
    }

    if (!m_totalInputAmount.isPositive()) {
        return false;
    }

    if (totalDebitAmount() + m_changeAmount != m_totalInputAmount) {
        return false;
    }

    if (totalOutputAmount() != m_totalInputAmount) {
        return false;
    }

    std::set<std::string> inputIds;

    for (const auto& inputLotId : m_inputLotIds) {
        if (inputLotId.empty()) {
            return false;
        }

        if (!inputIds.insert(inputLotId).second) {
            return false;
        }
    }

    std::set<std::string> outputIds;

    for (const auto& outputLot : m_outputLots) {
        if (!outputLot.isValid()) {
            return false;
        }

        if (!outputLot.isAvailable()) {
            return false;
        }

        if (!outputIds.insert(outputLot.id()).second) {
            return false;
        }
    }

    return true;
}

std::string CoinLotTransferPlan::serialize() const {
    std::ostringstream oss;

    oss << "CoinLotTransferPlan{"
        << "transactionId=" << m_transactionId
        << ";sender=" << m_senderAddress
        << ";recipient=" << m_recipientAddress
        << ";inputLots=" << m_inputLotIds.size()
        << ";outputLots=" << m_outputLots.size()
        << ";transferRaw=" << m_transferAmount.rawUnits()
        << ";feeRaw=" << m_feeAmount.rawUnits()
        << ";changeRaw=" << m_changeAmount.rawUnits()
        << ";totalInputRaw=" << m_totalInputAmount.rawUnits()
        << ";totalOutputRaw=" << totalOutputAmount().rawUnits()
        << "}";

    return oss.str();
}

} // namespace nodo::core
