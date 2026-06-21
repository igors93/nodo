#include "core/State.hpp"

#include "core/CoinLotTransactionValidator.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "staking/SecurityWeight.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::core {

namespace {

const std::string kStateSchemaId = "NODO_CORE_STATE";

std::uint64_t parseU64Strict(
    const std::string& value,
    const std::string& field
) {
    if (value.empty()) {
        throw std::invalid_argument("empty uint64 field: " + field);
    }

    for (const char c : value) {
        if (c < '0' || c > '9') {
            throw std::invalid_argument("malformed uint64 field: " + field);
        }
    }

    std::size_t parsedCharacters = 0;
    const unsigned long long parsed = std::stoull(value, &parsedCharacters);
    if (parsedCharacters != value.size() ||
        parsed > static_cast<unsigned long long>(
            std::numeric_limits<std::uint64_t>::max()
        )) {
        throw std::invalid_argument("malformed uint64 field: " + field);
    }

    return static_cast<std::uint64_t>(parsed);
}

std::int64_t parseI64Strict(
    const std::string& value,
    const std::string& field
) {
    if (value.empty()) {
        throw std::invalid_argument("empty int64 field: " + field);
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == '-' && index == 0 && value.size() > 1) {
            continue;
        }
        if (c < '0' || c > '9') {
            throw std::invalid_argument("malformed int64 field: " + field);
        }
    }

    std::size_t parsedCharacters = 0;
    const long long parsed = std::stoll(value, &parsedCharacters);
    if (parsedCharacters != value.size()) {
        throw std::invalid_argument("malformed int64 field: " + field);
    }

    return static_cast<std::int64_t>(parsed);
}

CoinLotStatus coinLotStatusFromString(const std::string& value) {
    if (value == "AVAILABLE") return CoinLotStatus::AVAILABLE;
    if (value == "LOCKED_FOR_SECURITY") return CoinLotStatus::LOCKED_FOR_SECURITY;
    if (value == "SPENT") return CoinLotStatus::SPENT;
    if (value == "SLASHED") return CoinLotStatus::SLASHED;
    throw std::invalid_argument("Unknown CoinLotStatus: " + value);
}

void requireUnique(
    std::set<std::string>& seen,
    const std::string& value,
    const std::string& field
) {
    if (value.empty()) {
        throw std::invalid_argument("empty unique field: " + field);
    }
    if (!seen.insert(value).second) {
        throw std::invalid_argument("duplicate value in State snapshot: " + field);
    }
}

template <typename T, typename Less>
std::vector<T> sortedCopy(
    const std::vector<T>& values,
    Less less
) {
    std::vector<T> copy = values;
    std::sort(copy.begin(), copy.end(), less);
    return copy;
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

const std::vector<Account>& State::accounts() const {
    return m_accounts;
}

const std::vector<economics::MintRecord>& State::mintRecords() const {
    return m_mintRecords;
}

const std::vector<economics::GenesisRewardRecord>& State::genesisRewardRecords() const {
    return m_genesisRewardRecords;
}

const std::vector<CoinLot>& State::coinLots() const {
    return m_coinLots;
}

AccountStateView State::accountStateView() const {
    AccountStateView view;

    for (const Account& account : m_accounts) {
        if (!view.putAccount(
                AccountState(
                    account.address(),
                    balanceOf(account.address()),
                    account.nextNonce()
                )
            )) {
            throw std::logic_error("State produced invalid AccountStateView.");
        }
    }

    return view;
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

    if (hasLegacyMintRecord(mintRecord.id())) {
        throw std::logic_error("Duplicated MintRecord id rejected.");
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
        throw std::logic_error("Generated legacy mint CoinLot is invalid.");
    }

    ensureAccountExists(mintRecord.recipientAddress());

    CoinLotRegistry registry =
        coinLotRegistry();

    registry.addLot(coinLot);

    replaceCoinLotsFromRegistry(registry);

    m_mintRecords.push_back(mintRecord);
    m_totalSupply = m_totalSupply + mintRecord.amount();
}

void State::applyGenesisRewardRecord(
    const economics::GenesisRewardRecord& genesisRewardRecord
) {
    if (!genesisRewardRecord.isValid()) {
        throw std::invalid_argument("Invalid GenesisRewardRecord rejected by State.");
    }

    const std::string rewardId =
        genesisRewardRecord.deterministicId();

    if (hasGenesisRewardRecord(rewardId)) {
        throw std::logic_error("Duplicated GenesisRewardRecord id rejected.");
    }

    const CoinLot rewardLot =
        genesisRewardRecord.createRewardCoinLot(
            m_currentBlockIndex
        );

    if (!rewardLot.isValid()) {
        throw std::logic_error("Generated GenesisReward CoinLot is invalid.");
    }

    if (rewardLot.originMintRecordId() != rewardId) {
        throw std::logic_error("Generated GenesisReward CoinLot origin mismatch.");
    }

    ensureAccountExists(genesisRewardRecord.validatorAddress());

    CoinLotRegistry registry =
        coinLotRegistry();

    registry.addLot(rewardLot);

    replaceCoinLotsFromRegistry(registry);

    m_genesisRewardRecords.push_back(genesisRewardRecord);
    m_totalSupply = m_totalSupply + genesisRewardRecord.amount();
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
    utils::Amount calculatedCreatedSupply = utils::Amount::fromRawUnits(0);

    for (const auto& mintRecord : m_mintRecords) {
        if (!mintRecord.isValid()) {
            return false;
        }

        calculatedCreatedSupply = calculatedCreatedSupply + mintRecord.amount();
    }

    for (const auto& genesisRewardRecord : m_genesisRewardRecords) {
        if (!genesisRewardRecord.isValid()) {
            return false;
        }

        calculatedCreatedSupply = calculatedCreatedSupply + genesisRewardRecord.amount();
    }

    if (calculatedCreatedSupply != m_totalSupply) {
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

std::string State::serialize() const {
    if (!isSupplyAuditable()) {
        throw std::invalid_argument("Cannot serialize invalid or unauditable State.");
    }

    std::vector<std::pair<std::string, std::string>> fields;
    fields.emplace_back("currentBlockIndex", std::to_string(m_currentBlockIndex));
    fields.emplace_back("totalSupplyRawUnits", std::to_string(m_totalSupply.rawUnits()));

    const std::vector<Account> accounts =
        sortedCopy(m_accounts, [](const Account& left, const Account& right) {
            return left.address() < right.address();
        });
    fields.emplace_back("accountCount", std::to_string(accounts.size()));
    for (std::size_t i = 0; i < accounts.size(); ++i) {
        const std::string p = "account." + std::to_string(i) + ".";
        fields.emplace_back(p + "address", accounts[i].address());
        fields.emplace_back(p + "nextNonce", std::to_string(accounts[i].nextNonce()));
        fields.emplace_back(p + "createdAtBlock", std::to_string(accounts[i].createdAtBlock()));
        fields.emplace_back(p + "lastUpdatedAtBlock", std::to_string(accounts[i].lastUpdatedAtBlock()));
    }

    const std::vector<economics::MintRecord> mintRecords =
        sortedCopy(m_mintRecords, [](const economics::MintRecord& left, const economics::MintRecord& right) {
            return left.id() < right.id();
        });
    fields.emplace_back("mintRecordCount", std::to_string(mintRecords.size()));
    for (std::size_t i = 0; i < mintRecords.size(); ++i) {
        const std::string p = "mintRecord." + std::to_string(i) + ".";
        fields.emplace_back(p + "id", mintRecords[i].id());
        fields.emplace_back(p + "authorizationId", mintRecords[i].authorizationId());
        fields.emplace_back(p + "recipientAddress", mintRecords[i].recipientAddress());
        fields.emplace_back(p + "amountRawUnits", std::to_string(mintRecords[i].amount().rawUnits()));
        fields.emplace_back(p + "reason", economics::mintReasonToString(mintRecords[i].reason()));
        fields.emplace_back(p + "epoch", std::to_string(mintRecords[i].epoch()));
        fields.emplace_back(p + "sourceBlockIndex", std::to_string(mintRecords[i].sourceBlockIndex()));
        fields.emplace_back(p + "sourceBlockHash", mintRecords[i].sourceBlockHash());
        fields.emplace_back(p + "timestamp", std::to_string(mintRecords[i].timestamp()));
    }

    const std::vector<economics::GenesisRewardRecord> rewardRecords =
        sortedCopy(m_genesisRewardRecords, [](
            const economics::GenesisRewardRecord& left,
            const economics::GenesisRewardRecord& right
        ) {
            return left.deterministicId() < right.deterministicId();
        });
    fields.emplace_back("genesisRewardRecordCount", std::to_string(rewardRecords.size()));
    for (std::size_t i = 0; i < rewardRecords.size(); ++i) {
        const std::string p = "genesisRewardRecord." + std::to_string(i) + ".";
        fields.emplace_back(p + "epoch", std::to_string(rewardRecords[i].epoch()));
        fields.emplace_back(p + "validatorAddress", rewardRecords[i].validatorAddress());
        fields.emplace_back(p + "amountRawUnits", std::to_string(rewardRecords[i].amount().rawUnits()));
        fields.emplace_back(p + "reason", economics::genesisRewardReasonToString(rewardRecords[i].reason()));
        fields.emplace_back(p + "workSummaryHash", rewardRecords[i].workSummaryHash());
        fields.emplace_back(p + "policyVersion", rewardRecords[i].policyVersion());
        fields.emplace_back(p + "acceptedBlockHash", rewardRecords[i].acceptedBlockHash());
        fields.emplace_back(p + "timestamp", std::to_string(rewardRecords[i].timestamp()));
    }

    const std::vector<CoinLot> coinLots =
        sortedCopy(m_coinLots, [](const CoinLot& left, const CoinLot& right) {
            return left.id() < right.id();
        });
    fields.emplace_back("coinLotCount", std::to_string(coinLots.size()));
    for (std::size_t i = 0; i < coinLots.size(); ++i) {
        const std::string p = "coinLot." + std::to_string(i) + ".";
        fields.emplace_back(p + "id", coinLots[i].id());
        fields.emplace_back(p + "originMintRecordId", coinLots[i].originMintRecordId());
        fields.emplace_back(p + "ownerAddress", coinLots[i].ownerAddress());
        fields.emplace_back(p + "amountRawUnits", std::to_string(coinLots[i].amount().rawUnits()));
        fields.emplace_back(p + "status", coinLotStatusToString(coinLots[i].status()));
        fields.emplace_back(p + "createdAtBlock", std::to_string(coinLots[i].createdAtBlock()));
        fields.emplace_back(p + "lockedUntilBlock", std::to_string(coinLots[i].lockedUntilBlock()));
        fields.emplace_back(p + "timestamp", std::to_string(coinLots[i].timestamp()));
    }

    std::vector<std::string> appliedTransactionIds = m_appliedTransactionIds;
    std::sort(appliedTransactionIds.begin(), appliedTransactionIds.end());
    fields.emplace_back("appliedTransactionCount", std::to_string(appliedTransactionIds.size()));
    for (std::size_t i = 0; i < appliedTransactionIds.size(); ++i) {
        fields.emplace_back(
            "appliedTransaction." + std::to_string(i) + ".id",
            appliedTransactionIds[i]
        );
    }

    return serialization::KeyValueFileCodec::serialize(kStateSchemaId, fields);
}

State State::deserialize(
    const std::string& serialized
) {
    const serialization::KeyValueFileDocument doc =
        serialization::KeyValueFileCodec::parse(serialized, kStateSchemaId);

    const std::uint64_t accountCount =
        parseU64Strict(doc.requireField("accountCount"), "accountCount");
    const std::uint64_t mintRecordCount =
        parseU64Strict(doc.requireField("mintRecordCount"), "mintRecordCount");
    const std::uint64_t rewardRecordCount =
        parseU64Strict(doc.requireField("genesisRewardRecordCount"), "genesisRewardRecordCount");
    const std::uint64_t coinLotCount =
        parseU64Strict(doc.requireField("coinLotCount"), "coinLotCount");
    const std::uint64_t appliedTransactionCount =
        parseU64Strict(doc.requireField("appliedTransactionCount"), "appliedTransactionCount");

    std::set<std::string> allowed = {
        "currentBlockIndex",
        "totalSupplyRawUnits",
        "accountCount",
        "mintRecordCount",
        "genesisRewardRecordCount",
        "coinLotCount",
        "appliedTransactionCount"
    };

    for (std::uint64_t i = 0; i < accountCount; ++i) {
        const std::string p = "account." + std::to_string(i) + ".";
        allowed.insert(p + "address");
        allowed.insert(p + "nextNonce");
        allowed.insert(p + "createdAtBlock");
        allowed.insert(p + "lastUpdatedAtBlock");
    }
    for (std::uint64_t i = 0; i < mintRecordCount; ++i) {
        const std::string p = "mintRecord." + std::to_string(i) + ".";
        allowed.insert(p + "id");
        allowed.insert(p + "authorizationId");
        allowed.insert(p + "recipientAddress");
        allowed.insert(p + "amountRawUnits");
        allowed.insert(p + "reason");
        allowed.insert(p + "epoch");
        allowed.insert(p + "sourceBlockIndex");
        allowed.insert(p + "sourceBlockHash");
        allowed.insert(p + "timestamp");
    }
    for (std::uint64_t i = 0; i < rewardRecordCount; ++i) {
        const std::string p = "genesisRewardRecord." + std::to_string(i) + ".";
        allowed.insert(p + "epoch");
        allowed.insert(p + "validatorAddress");
        allowed.insert(p + "amountRawUnits");
        allowed.insert(p + "reason");
        allowed.insert(p + "workSummaryHash");
        allowed.insert(p + "policyVersion");
        allowed.insert(p + "acceptedBlockHash");
        allowed.insert(p + "timestamp");
    }
    for (std::uint64_t i = 0; i < coinLotCount; ++i) {
        const std::string p = "coinLot." + std::to_string(i) + ".";
        allowed.insert(p + "id");
        allowed.insert(p + "originMintRecordId");
        allowed.insert(p + "ownerAddress");
        allowed.insert(p + "amountRawUnits");
        allowed.insert(p + "status");
        allowed.insert(p + "createdAtBlock");
        allowed.insert(p + "lockedUntilBlock");
        allowed.insert(p + "timestamp");
    }
    for (std::uint64_t i = 0; i < appliedTransactionCount; ++i) {
        allowed.insert("appliedTransaction." + std::to_string(i) + ".id");
    }

    doc.requireOnlyFields(allowed);

    State state;
    state.m_currentBlockIndex =
        parseU64Strict(doc.requireField("currentBlockIndex"), "currentBlockIndex");
    state.m_totalSupply = utils::Amount::fromRawUnits(
        parseI64Strict(doc.requireField("totalSupplyRawUnits"), "totalSupplyRawUnits")
    );

    std::set<std::string> accountAddresses;
    for (std::uint64_t i = 0; i < accountCount; ++i) {
        const std::string p = "account." + std::to_string(i) + ".";
        Account account(
            doc.requireField(p + "address"),
            parseU64Strict(doc.requireField(p + "nextNonce"), p + "nextNonce"),
            parseU64Strict(doc.requireField(p + "createdAtBlock"), p + "createdAtBlock"),
            parseU64Strict(doc.requireField(p + "lastUpdatedAtBlock"), p + "lastUpdatedAtBlock")
        );
        if (!account.isValid()) {
            throw std::invalid_argument("State snapshot contains invalid account.");
        }
        requireUnique(accountAddresses, account.address(), p + "address");
        state.m_accounts.push_back(std::move(account));
    }

    std::set<std::string> mintRecordIds;
    for (std::uint64_t i = 0; i < mintRecordCount; ++i) {
        const std::string p = "mintRecord." + std::to_string(i) + ".";
        economics::MintRecord record(
            doc.requireField(p + "id"),
            doc.requireField(p + "authorizationId"),
            doc.requireField(p + "recipientAddress"),
            utils::Amount::fromRawUnits(parseI64Strict(doc.requireField(p + "amountRawUnits"), p + "amountRawUnits")),
            economics::mintReasonFromString(doc.requireField(p + "reason")),
            parseU64Strict(doc.requireField(p + "epoch"), p + "epoch"),
            parseU64Strict(doc.requireField(p + "sourceBlockIndex"), p + "sourceBlockIndex"),
            doc.requireField(p + "sourceBlockHash"),
            parseI64Strict(doc.requireField(p + "timestamp"), p + "timestamp")
        );
        if (!record.isValid()) {
            throw std::invalid_argument("State snapshot contains invalid MintRecord: " + record.rejectionReason());
        }
        requireUnique(mintRecordIds, record.id(), p + "id");
        state.m_mintRecords.push_back(std::move(record));
    }

    std::set<std::string> rewardRecordIds;
    for (std::uint64_t i = 0; i < rewardRecordCount; ++i) {
        const std::string p = "genesisRewardRecord." + std::to_string(i) + ".";
        economics::GenesisRewardRecord record(
            parseU64Strict(doc.requireField(p + "epoch"), p + "epoch"),
            doc.requireField(p + "validatorAddress"),
            utils::Amount::fromRawUnits(parseI64Strict(doc.requireField(p + "amountRawUnits"), p + "amountRawUnits")),
            economics::genesisRewardReasonFromString(doc.requireField(p + "reason")),
            doc.requireField(p + "workSummaryHash"),
            doc.requireField(p + "policyVersion"),
            doc.requireField(p + "acceptedBlockHash"),
            parseI64Strict(doc.requireField(p + "timestamp"), p + "timestamp")
        );
        if (!record.isValid()) {
            throw std::invalid_argument("State snapshot contains invalid GenesisRewardRecord.");
        }
        requireUnique(rewardRecordIds, record.deterministicId(), p + "deterministicId");
        state.m_genesisRewardRecords.push_back(std::move(record));
    }

    std::set<std::string> coinLotIds;
    for (std::uint64_t i = 0; i < coinLotCount; ++i) {
        const std::string p = "coinLot." + std::to_string(i) + ".";
        CoinLot lot(
            doc.requireField(p + "id"),
            doc.requireField(p + "originMintRecordId"),
            doc.requireField(p + "ownerAddress"),
            utils::Amount::fromRawUnits(parseI64Strict(doc.requireField(p + "amountRawUnits"), p + "amountRawUnits")),
            coinLotStatusFromString(doc.requireField(p + "status")),
            parseU64Strict(doc.requireField(p + "createdAtBlock"), p + "createdAtBlock"),
            parseU64Strict(doc.requireField(p + "lockedUntilBlock"), p + "lockedUntilBlock"),
            parseI64Strict(doc.requireField(p + "timestamp"), p + "timestamp")
        );
        if (!lot.isValid()) {
            throw std::invalid_argument("State snapshot contains invalid CoinLot.");
        }
        requireUnique(coinLotIds, lot.id(), p + "id");
        state.m_coinLots.push_back(std::move(lot));
    }

    std::set<std::string> appliedTransactionIds;
    for (std::uint64_t i = 0; i < appliedTransactionCount; ++i) {
        const std::string id =
            doc.requireField("appliedTransaction." + std::to_string(i) + ".id");
        requireUnique(appliedTransactionIds, id, "appliedTransaction.id");
        state.m_appliedTransactionIds.push_back(id);
    }

    if (!state.isSupplyAuditable()) {
        throw std::invalid_argument("State snapshot failed supply audit.");
    }

    if (state.serialize() != serialized) {
        throw std::invalid_argument("State snapshot serialization is not canonical.");
    }

    return state;
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

bool State::hasLegacyMintRecord(
    const std::string& mintRecordId
) const {
    for (const auto& mintRecord : m_mintRecords) {
        if (mintRecord.id() == mintRecordId) {
            return true;
        }
    }

    return false;
}

bool State::hasGenesisRewardRecord(
    const std::string& genesisRewardId
) const {
    for (const auto& genesisRewardRecord : m_genesisRewardRecords) {
        if (genesisRewardRecord.deterministicId() == genesisRewardId) {
            return true;
        }
    }

    return false;
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
