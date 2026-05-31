#ifndef NODO_CONSENSUS_BLOCK_FINALIZER_HPP
#define NODO_CONSENSUS_BLOCK_FINALIZER_HPP

#include "consensus/QuorumCertificate.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::consensus {

/*
 * FinalizedBlockRecord is the local finality checkpoint for one block.
 *
 * Security principle:
 * A finalized record must commit to the exact block hash, previous hash,
 * block height, consensus round and quorum certificate that finalized it.
 */
class FinalizedBlockRecord {
public:
    FinalizedBlockRecord();

    FinalizedBlockRecord(
        std::uint64_t blockIndex,
        std::string blockHash,
        std::string previousHash,
        std::uint64_t round,
        std::int64_t finalizedAt,
        QuorumCertificate quorumCertificate
    );

    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    const std::string& previousHash() const;
    std::uint64_t round() const;
    std::int64_t finalizedAt() const;
    const QuorumCertificate& quorumCertificate() const;

    bool matchesBlock(
        const core::Block& block
    ) const;

    bool isStructurallyValid() const;

    bool verify(
        const core::ValidatorRegistry& validatorRegistry,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    ) const;

    std::string serialize() const;

    static FinalizedBlockRecord deserialize(
        const std::string& serialized
    );

private:
    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::string m_previousHash;
    std::uint64_t m_round;
    std::int64_t m_finalizedAt;
    QuorumCertificate m_quorumCertificate;
};

enum class BlockFinalizationRegistryStatus {
    REGISTERED,
    DUPLICATE,
    CONFLICTING_FINALIZATION,
    INVALID_RECORD,
    INVALID_REGISTRY
};

std::string blockFinalizationRegistryStatusToString(
    BlockFinalizationRegistryStatus status
);

class BlockFinalizationRegistryResult {
public:
    BlockFinalizationRegistryResult();

    static BlockFinalizationRegistryResult registered(
        FinalizedBlockRecord record
    );

    static BlockFinalizationRegistryResult duplicate(
        FinalizedBlockRecord record
    );

    static BlockFinalizationRegistryResult rejected(
        BlockFinalizationRegistryStatus status,
        std::string reason
    );

    BlockFinalizationRegistryStatus status() const;
    const std::string& reason() const;
    const FinalizedBlockRecord& record() const;

    bool registered() const;
    bool duplicate() const;
    bool success() const;

    std::string serialize() const;

private:
    BlockFinalizationRegistryStatus m_status;
    std::string m_reason;
    FinalizedBlockRecord m_record;
};

/*
 * BlockFinalizationRegistry protects local finality from conflicting decisions.
 *
 * It allows duplicate registration of the same finalized block but rejects a
 * different block hash for an already finalized height.
 */
class BlockFinalizationRegistry {
public:
    BlockFinalizationRegistry();

    bool canRegister(
        const FinalizedBlockRecord& record
    ) const;

    BlockFinalizationRegistryResult registerFinalizedBlock(
        const FinalizedBlockRecord& record
    );

    bool hasFinalizedHeight(
        std::uint64_t blockIndex
    ) const;

    bool isFinalizedBlock(
        std::uint64_t blockIndex,
        const std::string& blockHash
    ) const;

    const FinalizedBlockRecord* recordForHeight(
        std::uint64_t blockIndex
    ) const;

    std::uint64_t highestFinalizedHeight() const;
    std::size_t size() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::map<std::uint64_t, FinalizedBlockRecord> m_recordsByHeight;
};

enum class BlockFinalizationStatus {
    FINALIZED,
    DUPLICATE_FINALIZATION,
    INVALID_BLOCKCHAIN,
    INVALID_BLOCK,
    INVALID_CERTIFICATE,
    CERTIFICATE_BLOCK_MISMATCH,
    INVALID_FINALIZATION_REGISTRY,
    ALREADY_FINALIZED_CONFLICT,
    APPEND_REJECTED,
    REGISTRATION_FAILED
};

std::string blockFinalizationStatusToString(
    BlockFinalizationStatus status
);

class BlockFinalizationResult {
public:
    BlockFinalizationResult();

    static BlockFinalizationResult finalized(
        FinalizedBlockRecord record
    );

    static BlockFinalizationResult duplicate(
        FinalizedBlockRecord record
    );

    static BlockFinalizationResult rejected(
        BlockFinalizationStatus status,
        std::string reason
    );

    BlockFinalizationStatus status() const;
    const std::string& reason() const;
    const FinalizedBlockRecord& record() const;

    bool finalized() const;
    bool duplicate() const;
    bool success() const;

    std::string serialize() const;

private:
    BlockFinalizationStatus m_status;
    std::string m_reason;
    FinalizedBlockRecord m_record;
};

/*
 * BlockFinalizer appends a block only after a valid quorum certificate proves
 * that registered validators approved that exact block.
 */
class BlockFinalizer {
public:
    static BlockFinalizationResult finalizeBlock(
        core::Blockchain& blockchain,
        const core::Block& block,
        const QuorumCertificate& certificate,
        const core::ValidatorRegistry& validatorRegistry,
        BlockFinalizationRegistry& finalizationRegistry,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        std::int64_t finalizedAt
    );

    static bool certificateMatchesBlock(
        const core::Block& block,
        const QuorumCertificate& certificate
    );
};

} // namespace nodo::consensus

#endif
