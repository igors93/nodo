#include "core/MempoolBlockProducer.hpp"

#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

BlockProductionConfig::BlockProductionConfig()
    : m_maxTransactionsPerBlock(1000),
      m_minTransactionsPerBlock(1) {}

BlockProductionConfig::BlockProductionConfig(
    std::size_t maxTransactionsPerBlock,
    std::size_t minTransactionsPerBlock
)
    : m_maxTransactionsPerBlock(maxTransactionsPerBlock),
      m_minTransactionsPerBlock(minTransactionsPerBlock) {}

std::size_t BlockProductionConfig::maxTransactionsPerBlock() const {
    return m_maxTransactionsPerBlock;
}

std::size_t BlockProductionConfig::minTransactionsPerBlock() const {
    return m_minTransactionsPerBlock;
}

bool BlockProductionConfig::isValid() const {
    return m_maxTransactionsPerBlock > 0 &&
           m_minTransactionsPerBlock > 0 &&
           m_minTransactionsPerBlock <= m_maxTransactionsPerBlock;
}

BlockProductionPlan::BlockProductionPlan()
    : m_blockIndex(0),
      m_previousHash(""),
      m_transactions(),
      m_ledgerRecords(),
      m_timestamp(0) {}

BlockProductionPlan::BlockProductionPlan(
    std::uint64_t blockIndex,
    std::string previousHash,
    std::vector<Transaction> transactions,
    std::vector<LedgerRecord> ledgerRecords,
    std::int64_t timestamp
)
    : m_blockIndex(blockIndex),
      m_previousHash(std::move(previousHash)),
      m_transactions(std::move(transactions)),
      m_ledgerRecords(std::move(ledgerRecords)),
      m_timestamp(timestamp) {}

std::uint64_t BlockProductionPlan::blockIndex() const {
    return m_blockIndex;
}

const std::string& BlockProductionPlan::previousHash() const {
    return m_previousHash;
}

const std::vector<Transaction>& BlockProductionPlan::transactions() const {
    return m_transactions;
}

const std::vector<LedgerRecord>& BlockProductionPlan::ledgerRecords() const {
    return m_ledgerRecords;
}

std::int64_t BlockProductionPlan::timestamp() const {
    return m_timestamp;
}

std::vector<std::string> BlockProductionPlan::transactionIds() const {
    std::vector<std::string> ids;

    for (const auto& transaction : m_transactions) {
        ids.push_back(transaction.id());
    }

    return ids;
}

bool BlockProductionPlan::isValid() const {
    if (m_blockIndex == 0 ||
        m_previousHash.empty() ||
        m_timestamp <= 0) {
        return false;
    }

    if (m_transactions.empty() ||
        m_transactions.size() != m_ledgerRecords.size()) {
        return false;
    }

    for (std::size_t index = 0; index < m_transactions.size(); ++index) {
        if (!m_ledgerRecords[index].isValid()) {
            return false;
        }

        if (m_ledgerRecords[index].type() != LedgerRecordType::TRANSACTION) {
            return false;
        }

        if (m_ledgerRecords[index].sourceId() != m_transactions[index].id()) {
            return false;
        }
    }

    return true;
}

std::string BlockProductionPlan::serialize() const {
    std::ostringstream oss;

    oss << "BlockProductionPlan{"
        << "blockIndex=" << m_blockIndex
        << ";previousHash=" << m_previousHash
        << ";timestamp=" << m_timestamp
        << ";transactionIds=[";

    const std::vector<std::string> ids =
        transactionIds();

    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (index != 0) {
            oss << ",";
        }

        oss << ids[index];
    }

    oss << "]}";

    return oss.str();
}

std::string blockProductionStatusToString(
    BlockProductionStatus status
) {
    switch (status) {
        case BlockProductionStatus::PRODUCED:
            return "PRODUCED";
        case BlockProductionStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case BlockProductionStatus::INVALID_BLOCKCHAIN:
            return "INVALID_BLOCKCHAIN";
        case BlockProductionStatus::INVALID_MEMPOOL:
            return "INVALID_MEMPOOL";
        case BlockProductionStatus::EMPTY_MEMPOOL:
            return "EMPTY_MEMPOOL";
        case BlockProductionStatus::NOT_ENOUGH_TRANSACTIONS:
            return "NOT_ENOUGH_TRANSACTIONS";
        case BlockProductionStatus::INVALID_TRANSACTION:
            return "INVALID_TRANSACTION";
        case BlockProductionStatus::BLOCK_AUDIT_FAILED:
            return "BLOCK_AUDIT_FAILED";
        default:
            return "BLOCK_AUDIT_FAILED";
    }
}

BlockProductionResult::BlockProductionResult()
    : m_status(BlockProductionStatus::BLOCK_AUDIT_FAILED),
      m_reason("Uninitialized block production result."),
      m_block(std::nullopt),
      m_plan() {}

BlockProductionResult BlockProductionResult::produced(
    Block block,
    BlockProductionPlan plan
) {
    BlockProductionResult result;
    result.m_status = BlockProductionStatus::PRODUCED;
    result.m_reason = "";
    result.m_block = std::move(block);
    result.m_plan = std::move(plan);
    return result;
}

BlockProductionResult BlockProductionResult::rejected(
    BlockProductionStatus status,
    std::string reason
) {
    BlockProductionResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

BlockProductionStatus BlockProductionResult::status() const {
    return m_status;
}

const std::string& BlockProductionResult::reason() const {
    return m_reason;
}

bool BlockProductionResult::produced() const {
    return m_status == BlockProductionStatus::PRODUCED &&
           m_block.has_value() &&
           m_plan.isValid();
}

const Block& BlockProductionResult::block() const {
    if (!m_block.has_value()) {
        throw std::logic_error("BlockProductionResult has no produced block.");
    }

    return m_block.value();
}

const BlockProductionPlan& BlockProductionResult::plan() const {
    return m_plan;
}

std::string BlockProductionResult::serialize() const {
    std::ostringstream oss;

    oss << "BlockProductionResult{"
        << "status=" << blockProductionStatusToString(m_status)
        << ";reason=" << m_reason
        << ";block=" << (m_block.has_value() ? m_block->serialize() : "NONE")
        << ";plan=" << (m_plan.isValid() ? m_plan.serialize() : "NONE")
        << "}";

    return oss.str();
}

namespace {

BlockProductionResult produceCandidateBlockImpl(
    const Blockchain& blockchain,
    const mempool::Mempool& mempool,
    const AccountStateView* accountStateView,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const BlockProductionConfig& config,
    std::int64_t timestamp
) {
    if (!config.isValid()) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::INVALID_CONFIG,
            "Block production config is invalid."
        );
    }

    if (blockchain.empty() || !blockchain.isValid()) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::INVALID_BLOCKCHAIN,
            "Blockchain is empty or invalid."
        );
    }

    if (timestamp <= 0) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::BLOCK_AUDIT_FAILED,
            "Block timestamp must be positive."
        );
    }

    if (!mempool.isValid(
            policy,
            context
        )) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::INVALID_MEMPOOL,
            "Mempool failed integrity audit."
        );
    }

    std::vector<Transaction> transactions =
        accountStateView == nullptr
            ? mempool.transactionsForBlock(
                  config.maxTransactionsPerBlock()
              )
            : mempool.transactionsForBlock(
                  config.maxTransactionsPerBlock(),
                  *accountStateView
              );

    if (transactions.empty()) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::EMPTY_MEMPOOL,
            "Mempool has no transactions available for block production."
        );
    }

    if (transactions.size() < config.minTransactionsPerBlock()) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::NOT_ENOUGH_TRANSACTIONS,
            "Not enough transactions available for block production."
        );
    }

    std::vector<LedgerRecord> ledgerRecords;

    for (const auto& transaction : transactions) {
        if (!transaction.isStructurallyValid(
                policy,
                context
            )) {
            return BlockProductionResult::rejected(
                BlockProductionStatus::INVALID_TRANSACTION,
                "Selected transaction failed policy validation."
            );
        }

        ledgerRecords.push_back(
            LedgerRecord::fromTransaction(
                transaction,
                policy,
                context,
                timestamp
            )
        );
    }

    BlockProductionPlan plan(
        blockchain.size(),
        blockchain.latestBlock().hash(),
        transactions,
        ledgerRecords,
        timestamp
    );

    if (!plan.isValid()) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::BLOCK_AUDIT_FAILED,
            "Block production plan failed audit."
        );
    }

    // A non-genesis protocol block must be produced with a real AccountStateView.
    // Without one, the state-transition preview runs on an empty account state and
    // produces stateRoot/receiptsRoot values that do not match the real economic
    // state.  Such a block would be rejected by every other validator and must
    // never leave the producer as a finalized protocol block.
    if (accountStateView == nullptr && plan.blockIndex() > 0) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::BLOCK_AUDIT_FAILED,
            "Cannot produce a non-genesis protocol block without AccountStateView: "
            "stateRoot and receiptsRoot cannot be computed from real account state."
        );
    }

    Block draftBlock(
        plan.blockIndex(),
        plan.previousHash(),
        plan.ledgerRecords(),
        timestamp,
        "",
        ""
    );

    StateTransitionPreviewContext previewContext;
    if (accountStateView != nullptr) {
        previewContext = StateTransitionPreviewContext(
            0,
            *accountStateView,
            true,
            true
        );
    } else {
        previewContext = StateTransitionPreviewContext::structuralOnly(0);
    }

    const StateTransitionPreviewResult preview =
        StateTransitionPreview::previewBlock(
            draftBlock,
            previewContext
        );

    std::string finalStateRoot;
    std::string finalReceiptsRoot;

    if (plan.blockIndex() == 0) {
        finalStateRoot = "";
        finalReceiptsRoot = "";
    } else {
        if (!preview.accepted()) {
            BlockProductionStatus status = BlockProductionStatus::BLOCK_AUDIT_FAILED;
            if (preview.status() == StateTransitionPreviewStatus::INVALID_TRANSACTION ||
                preview.status() == StateTransitionPreviewStatus::INSUFFICIENT_BALANCE ||
                preview.status() == StateTransitionPreviewStatus::INVALID_NONCE ||
                preview.status() == StateTransitionPreviewStatus::DUPLICATE_TRANSACTION) {
                status = BlockProductionStatus::INVALID_TRANSACTION;
            }
            return BlockProductionResult::rejected(
                status,
                "State transition preview failed: " + preview.reason()
            );
        }
        finalStateRoot = preview.stateRoot();
        finalReceiptsRoot = preview.receiptsRoot();
    }

    Block finalBlock(
        plan.blockIndex(),
        plan.previousHash(),
        plan.ledgerRecords(),
        timestamp,
        finalStateRoot,
        finalReceiptsRoot
    );

    if (!finalBlock.isValid(true) ||
        !blockchain.canAppendBlock(finalBlock)) {
        return BlockProductionResult::rejected(
            BlockProductionStatus::BLOCK_AUDIT_FAILED,
            "Produced block failed append audit."
        );
    }

    return BlockProductionResult::produced(
        finalBlock,
        plan
    );
}

} // namespace

BlockProductionResult MempoolBlockProducer::produceCandidateBlock(
    const Blockchain& blockchain,
    const mempool::Mempool& mempool,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const BlockProductionConfig& config,
    std::int64_t timestamp
) {
    return produceCandidateBlockImpl(
        blockchain,
        mempool,
        nullptr,
        policy,
        context,
        config,
        timestamp
    );
}

BlockProductionResult MempoolBlockProducer::produceCandidateBlock(
    const Blockchain& blockchain,
    const mempool::Mempool& mempool,
    const AccountStateView& accountStateView,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const BlockProductionConfig& config,
    std::int64_t timestamp
) {
    return produceCandidateBlockImpl(
        blockchain,
        mempool,
        &accountStateView,
        policy,
        context,
        config,
        timestamp
    );
}

} // namespace nodo::core
