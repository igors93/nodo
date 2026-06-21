#ifndef NODO_CORE_MEMPOOL_BLOCK_PRODUCER_HPP
#define NODO_CORE_MEMPOOL_BLOCK_PRODUCER_HPP

#include "core/AccountStateView.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "mempool/Mempool.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::core {

class BlockProductionConfig {
public:
    BlockProductionConfig();

    BlockProductionConfig(
        std::size_t maxTransactionsPerBlock,
        std::size_t minTransactionsPerBlock
    );

    std::size_t maxTransactionsPerBlock() const;
    std::size_t minTransactionsPerBlock() const;

    bool isValid() const;

private:
    std::size_t m_maxTransactionsPerBlock;
    std::size_t m_minTransactionsPerBlock;
};

class BlockProductionPlan {
public:
    BlockProductionPlan();

    BlockProductionPlan(
        std::uint64_t blockIndex,
        std::string previousHash,
        std::vector<Transaction> transactions,
        std::vector<LedgerRecord> ledgerRecords,
        std::int64_t timestamp
    );

    std::uint64_t blockIndex() const;
    const std::string& previousHash() const;
    const std::vector<Transaction>& transactions() const;
    const std::vector<LedgerRecord>& ledgerRecords() const;
    std::int64_t timestamp() const;

    std::vector<std::string> transactionIds() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::uint64_t m_blockIndex;
    std::string m_previousHash;
    std::vector<Transaction> m_transactions;
    std::vector<LedgerRecord> m_ledgerRecords;
    std::int64_t m_timestamp;
};

enum class BlockProductionStatus {
    PRODUCED,
    INVALID_CONFIG,
    INVALID_BLOCKCHAIN,
    INVALID_MEMPOOL,
    EMPTY_MEMPOOL,
    NOT_ENOUGH_TRANSACTIONS,
    INVALID_TRANSACTION,
    BLOCK_AUDIT_FAILED
};

std::string blockProductionStatusToString(
    BlockProductionStatus status
);

class BlockProductionResult {
public:
    BlockProductionResult();

    static BlockProductionResult produced(
        Block block,
        BlockProductionPlan plan
    );

    static BlockProductionResult rejected(
        BlockProductionStatus status,
        std::string reason
    );

    BlockProductionStatus status() const;
    const std::string& reason() const;
    bool produced() const;

    const Block& block() const;
    const BlockProductionPlan& plan() const;

    std::string serialize() const;

private:
    BlockProductionStatus m_status;
    std::string m_reason;
    std::optional<Block> m_block;
    BlockProductionPlan m_plan;
};

/*
 * MempoolBlockProducer turns already-admitted mempool transactions into a
 * candidate block without mutating the mempool.
 *
 * Important:
 * Producing a block is not finalization. The block must still be signed, voted
 * on, certified, finalized and applied through State validation.
 */
class MempoolBlockProducer {
public:
    static BlockProductionResult produceCandidateBlock(
        const Blockchain& blockchain,
        const mempool::Mempool& mempool,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const BlockProductionConfig& config,
        std::int64_t timestamp
    );

    static BlockProductionResult produceCandidateBlock(
        const Blockchain& blockchain,
        const mempool::Mempool& mempool,
        const AccountStateView& accountStateView,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const BlockProductionConfig& config,
        std::int64_t timestamp
    );
};

} // namespace nodo::core

#endif
