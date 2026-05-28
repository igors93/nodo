#include "core/State.hpp"

#include "staking/SecurityWeight.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::core {

namespace {

utils::Amount minAmount(
    const utils::Amount& left,
    const utils::Amount& right
) {
    return left <= right ? left : right;
}

} // namespace

State::State()
    : m_currentBlockIndex(0),
      m_totalSupply(utils::Amount::fromRawUnits(0)) {}

const std::string& State::feePoolAddress() {
    static const std::string address = "nodo_fee_pool";
    return address;
}

std::uint64_t State::currentBlockIndex() const {
    return m_currentBlockIndex;
}

utils::Amount State::totalSupply() const {
    return m_totalSupply;
}

const std::vector<economics::MintRecord>& State::mintRecords() const {
    return m_mintRecords;
}

const std::vector<CoinLot>& State::coinLots() const {
    return m_coinLots;
}

void State::advanceBlock() {
    ++m_currentBlockIndex;

    for (auto& coinLot : m_coinLots) {
        coinLot.unlockIfMature(m_currentBlockIndex);
    }
}

void State::applyMintRecord(const economics::MintRecord& mintRecord) {
    if (!mintRecord.isValid()) {
        throw std::invalid_argument("Invalid MintRecord rejected by State.");
    }

    for (const auto& existing : m_mintRecords) {
        if (existing.id() == mintRecord.id()) {
            throw std::logic_error("Duplicated MintRecord id rejected.");
        }
    }

    const std::string coinLotId = createCoinLotIdFromMint(mintRecord);

    CoinLot coinLot(
        coinLotId,
        mintRecord.id(),
        mintRecord.recipientAddress(),
        mintRecord.amount(),
        CoinLotStatus::AVAILABLE,
        m_currentBlockIndex,
        0,
        mintRecord.timestamp()
    );

    if (!coinLot.isValid()) {
        throw std::logic_error("Generated CoinLot is invalid.");
    }

    m_mintRecords.push_back(mintRecord);
    m_coinLots.push_back(coinLot);
    m_totalSupply = m_totalSupply + mintRecord.amount();
}

void State::applyTransferTransaction(const Transaction& transaction) {
    if (transaction.type() != TransactionType::TRANSFER) {
        throw std::invalid_argument("Only TRANSFER transactions can be applied by this method.");
    }

    if (transaction.id().empty()) {
        throw std::invalid_argument("Transaction id cannot be empty.");
    }

    if (transaction.fromAddress().empty() || transaction.toAddress().empty()) {
        throw std::invalid_argument("Transfer addresses cannot be empty.");
    }

    if (transaction.fromAddress() == transaction.toAddress()) {
        throw std::invalid_argument("Transfer sender and recipient cannot be the same.");
    }

    if (!transaction.amount().isPositive()) {
        throw std::invalid_argument("Transfer amount must be positive.");
    }

    if (isTransactionAlreadyApplied(transaction.id())) {
        throw std::logic_error("Duplicate transaction application rejected.");
    }

    const utils::Amount totalDebit = transaction.amount() + transaction.fee();

    if (!totalDebit.isPositive()) {
        throw std::invalid_argument("Transfer total debit must be positive.");
    }

    std::vector<std::size_t> selectedInputIndexes;
    utils::Amount collected = utils::Amount::fromRawUnits(0);

    for (std::size_t index = 0; index < m_coinLots.size(); ++index) {
        const CoinLot& coinLot = m_coinLots[index];

        if (coinLot.ownerAddress() != transaction.fromAddress()) {
            continue;
        }

        if (!coinLot.isSpendable()) {
            continue;
        }

        selectedInputIndexes.push_back(index);
        collected = collected + coinLot.amount();

        if (collected >= totalDebit) {
            break;
        }
    }

    if (collected < totalDebit) {
        throw std::logic_error("Insufficient spendable balance for transfer.");
    }

    std::vector<CoinLot> outputLots;
    utils::Amount remainingRecipientAmount = transaction.amount();
    utils::Amount remainingFeeAmount = transaction.fee();
    std::size_t outputIndex = 0;

    for (const std::size_t inputIndex : selectedInputIndexes) {
        const CoinLot& inputLot = m_coinLots[inputIndex];
        utils::Amount remainingInputAmount = inputLot.amount();

        auto createOutput = [&](const std::string& ownerAddress,
                                const utils::Amount& amount,
                                const std::string& outputKind) {
            if (!amount.isPositive()) {
                return;
            }

            CoinLot outputLot(
                createTransferOutputCoinLotId(
                    transaction,
                    inputLot,
                    outputKind,
                    outputIndex
                ),
                inputLot.originMintRecordId(),
                ownerAddress,
                amount,
                CoinLotStatus::AVAILABLE,
                m_currentBlockIndex,
                0,
                transaction.timestamp()
            );

            if (!outputLot.isValid()) {
                throw std::logic_error("Generated transfer output CoinLot is invalid.");
            }

            outputLots.push_back(outputLot);
            ++outputIndex;
        };

        while (remainingInputAmount.isPositive()) {
            if (remainingRecipientAmount.isPositive()) {
                const utils::Amount outputAmount =
                    minAmount(remainingInputAmount, remainingRecipientAmount);

                createOutput(
                    transaction.toAddress(),
                    outputAmount,
                    "recipient"
                );

                remainingInputAmount = remainingInputAmount - outputAmount;
                remainingRecipientAmount = remainingRecipientAmount - outputAmount;
                continue;
            }

            if (remainingFeeAmount.isPositive()) {
                const utils::Amount outputAmount =
                    minAmount(remainingInputAmount, remainingFeeAmount);

                createOutput(
                    feePoolAddress(),
                    outputAmount,
                    "fee"
                );

                remainingInputAmount = remainingInputAmount - outputAmount;
                remainingFeeAmount = remainingFeeAmount - outputAmount;
                continue;
            }

            createOutput(
                transaction.fromAddress(),
                remainingInputAmount,
                "change"
            );

            remainingInputAmount = utils::Amount::fromRawUnits(0);
        }
    }

    if (remainingRecipientAmount.isPositive() || remainingFeeAmount.isPositive()) {
        throw std::logic_error("Transfer output allocation failed.");
    }

    for (const std::size_t inputIndex : selectedInputIndexes) {
        m_coinLots[inputIndex].markSpent();
    }

    for (const auto& outputLot : outputLots) {
        m_coinLots.push_back(outputLot);
    }

    m_appliedTransactionIds.push_back(transaction.id());
}

void State::lockCoinLotForSecurity(
    const std::string& coinLotId,
    std::uint64_t lockedUntilBlock
) {
    for (auto& coinLot : m_coinLots) {
        if (coinLot.id() == coinLotId) {
            coinLot.lockForSecurity(lockedUntilBlock);
            return;
        }
    }

    throw std::invalid_argument("CoinLot not found.");
}

utils::Amount State::balanceOf(const std::string& ownerAddress) const {
    utils::Amount balance = utils::Amount::fromRawUnits(0);

    for (const auto& coinLot : m_coinLots) {
        if (coinLot.ownerAddress() != ownerAddress) {
            continue;
        }

        if (coinLot.isAvailable() || coinLot.isLockedForSecurity()) {
            balance = balance + coinLot.amount();
        }
    }

    return balance;
}

std::uint64_t State::totalSecurityWeight() const {
    std::uint64_t total = 0;

    for (const auto& coinLot : m_coinLots) {
        total += staking::SecurityWeight::calculateForCoinLot(
            coinLot,
            m_currentBlockIndex
        );
    }

    return total;
}

bool State::isTransactionAlreadyApplied(const std::string& transactionId) const {
    for (const auto& appliedTransactionId : m_appliedTransactionIds) {
        if (appliedTransactionId == transactionId) {
            return true;
        }
    }

    return false;
}

bool State::isSupplyAuditable() const {
    utils::Amount calculatedMintedSupply = utils::Amount::fromRawUnits(0);

    for (const auto& mintRecord : m_mintRecords) {
        if (!mintRecord.isValid()) {
            return false;
        }

        calculatedMintedSupply = calculatedMintedSupply + mintRecord.amount();
    }

    if (calculatedMintedSupply != m_totalSupply) {
        return false;
    }

    utils::Amount activeCoinLotSupply = utils::Amount::fromRawUnits(0);

    for (const auto& coinLot : m_coinLots) {
        if (!coinLot.isValid()) {
            return false;
        }

        if (coinLot.isAvailable() || coinLot.isLockedForSecurity()) {
            activeCoinLotSupply = activeCoinLotSupply + coinLot.amount();
        }
    }

    return activeCoinLotSupply == m_totalSupply;
}

std::string State::createCoinLotIdFromMint(
    const economics::MintRecord& mintRecord
) const {
    return "coinlot_from_" + mintRecord.id();
}

std::string State::createTransferOutputCoinLotId(
    const Transaction& transaction,
    const CoinLot& inputLot,
    const std::string& outputKind,
    std::size_t outputIndex
) const {
    std::ostringstream oss;

    oss << "coinlot_from_tx_"
        << transaction.id()
        << "_input_"
        << inputLot.id()
        << "_"
        << outputKind
        << "_"
        << outputIndex;

    return oss.str();
}

} // namespace nodo::core