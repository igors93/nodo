#include "core/State.hpp"

#include "core/CoinLotTransactionValidator.hpp"
#include "staking/SecurityWeight.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::core {

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

const std::vector<Account>& State::accounts() const {
    return m_accounts;
}

const std::vector<economics::MintRecord>& State::mintRecords() const {
    return m_mintRecords;
}

const std::vector<CoinLot>& State::coinLots() const {
    return m_coinLots;
}

CoinLotRegistry State::coinLotRegistry() const {
    return CoinLotRegistry::fromCoinLots(
        m_coinLots
    );
}

bool State::hasAccount(const std::string& address) const {
    return findAccount(address) != nullptr;
}

std::uint64_t State::nextNonceOf(const std::string& address) const {
    const Account* account = findAccount(address);

    if (account == nullptr) {
        throw std::invalid_argument("Account not found.");
    }

    return account->nextNonce();
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

    ensureAccountExists(mintRecord.recipientAddress());

    CoinLotRegistry registry =
        coinLotRegistry();

    registry.addLot(coinLot);

    replaceCoinLotsFromRegistry(registry);

    m_mintRecords.push_back(mintRecord);
    m_totalSupply = m_totalSupply + mintRecord.amount();
}

void State::applyTransferTransaction(const Transaction& transaction) {
    applyTransferTransactionUsingRegistry(transaction);
}

void State::applyTransferTransactionUsingRegistry(const Transaction& transaction) {
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

    if (transaction.fee().isNegative()) {
        throw std::invalid_argument("Transfer fee cannot be negative.");
    }

    if (transaction.nonce() == 0) {
        throw std::invalid_argument("Transfer nonce must be positive.");
    }

    if (isTransactionAlreadyApplied(transaction.id())) {
        throw std::logic_error("Duplicate transaction application rejected.");
    }

    const Account* senderAccount = findAccount(transaction.fromAddress());

    if (senderAccount == nullptr) {
        throw std::logic_error("Sender account does not exist.");
    }

    if (!senderAccount->canApplyNonce(transaction.nonce())) {
        throw std::logic_error("Invalid sender nonce rejected.");
    }

    CoinLotRegistry registry =
        coinLotRegistry();

    const CoinLotTransactionValidationResult validation =
        CoinLotTransactionValidator::validateTransferTransaction(
            transaction,
            registry
        );

    if (!validation.success()) {
        throw std::logic_error(
            "Transfer rejected by CoinLotRegistry: " + validation.reason()
        );
    }

    CoinLotTransactionValidator::applyTransfer(
        registry,
        transaction,
        m_currentBlockIndex
    );

    ensureAccountExists(transaction.toAddress());

    if (transaction.fee().isPositive()) {
        ensureAccountExists(feePoolAddress());
    }

    Account* mutableSenderAccount = findAccount(transaction.fromAddress());

    if (mutableSenderAccount == nullptr) {
        throw std::logic_error("Sender account disappeared during transfer application.");
    }

    mutableSenderAccount->markTransactionApplied(
        transaction.nonce(),
        m_currentBlockIndex
    );

    replaceCoinLotsFromRegistry(registry);

    m_appliedTransactionIds.push_back(transaction.id());
}

void State::lockCoinLotForSecurity(
    const std::string& coinLotId,
    std::uint64_t lockedUntilBlock
) {
    CoinLotRegistry registry =
        coinLotRegistry();

    const CoinLot& targetLot =
        registry.lot(coinLotId);

    registry.lockForSecurity(
        coinLotId,
        targetLot.ownerAddress(),
        lockedUntilBlock
    );

    replaceCoinLotsFromRegistry(registry);
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

    for (const auto& account : m_accounts) {
        if (!account.isValid()) {
            return false;
        }
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

Account* State::findAccount(const std::string& address) {
    for (auto& account : m_accounts) {
        if (account.address() == address) {
            return &account;
        }
    }

    return nullptr;
}

const Account* State::findAccount(const std::string& address) const {
    for (const auto& account : m_accounts) {
        if (account.address() == address) {
            return &account;
        }
    }

    return nullptr;
}

void State::ensureAccountExists(const std::string& address) {
    if (address.empty()) {
        throw std::invalid_argument("Account address cannot be empty.");
    }

    if (findAccount(address) != nullptr) {
        return;
    }

    Account account = Account::create(
        address,
        m_currentBlockIndex
    );

    if (!account.isValid()) {
        throw std::logic_error("Generated Account is invalid.");
    }

    m_accounts.push_back(account);
}

std::string State::createCoinLotIdFromMint(
    const economics::MintRecord& mintRecord
) const {
    return "coinlot_from_" + mintRecord.id();
}

void State::replaceCoinLotsFromRegistry(
    const CoinLotRegistry& registry
) {
    if (!registry.isValid()) {
        throw std::invalid_argument("Cannot replace State CoinLots from invalid registry.");
    }

    m_coinLots.clear();

    for (const auto& entry : registry.lots()) {
        m_coinLots.push_back(entry.second);
    }
}

} // namespace nodo::core
