#ifndef NODO_STORAGE_BLOCKCHAIN_LOADER_HPP
#define NODO_STORAGE_BLOCKCHAIN_LOADER_HPP

#include "core/Blockchain.hpp"
#include "core/StateTransitionPreviewContext.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace nodo::storage {

/*
 * BlockchainLoadReport summarizes the result of rebuilding a Blockchain from
 * persisted storage.
 *
 * Security principle:
 * Loading from disk must be auditable. A failed load should explain where the
 * storage validation failed.
 */
class BlockchainLoadReport {
public:
    BlockchainLoadReport();

    bool success() const;
    const std::string& failureReason() const;

    bool storageReaderValid() const;
    bool loadedBlockchainValid() const;
    bool loadedBlockchainMatchesManifest() const;
    bool loadedBlockchainMatchesIndex() const;

    std::size_t loadedBlockCount() const;

    void markFailure(std::string reason);

    void setStorageReaderValid(bool value);
    void setLoadedBlockchainValid(bool value);
    void setLoadedBlockchainMatchesManifest(bool value);
    void setLoadedBlockchainMatchesIndex(bool value);
    void setLoadedBlockCount(std::size_t value);

    std::string serialize() const;

private:
    bool m_success;
    std::string m_failureReason;

    bool m_storageReaderValid;
    bool m_loadedBlockchainValid;
    bool m_loadedBlockchainMatchesManifest;
    bool m_loadedBlockchainMatchesIndex;

    std::size_t m_loadedBlockCount;
};

/*
 * BlockchainCommitmentVerificationReport records the result of re-executing
 * every block in a loaded chain via StateTransitionEngine and comparing the
 * computed stateRoot/receiptsRoot to each block's declared values.
 *
 * This is a stronger check than isValid(true): it proves that the persisted
 * state commitments match deterministic re-execution, not just that the fields
 * are structurally present.
 */
class BlockchainCommitmentVerificationReport {
public:
    static BlockchainCommitmentVerificationReport passed(std::size_t verifiedBlockCount);
    static BlockchainCommitmentVerificationReport failed(std::uint64_t firstFailedHeight, std::string reason);

    bool commitmentsPassed() const;
    std::uint64_t firstFailedHeight() const;
    const std::string& reason() const;
    std::size_t verifiedBlockCount() const;

    std::string serialize() const;

private:
    BlockchainCommitmentVerificationReport();

    bool m_passed;
    std::uint64_t m_firstFailedHeight;
    std::string m_reason;
    std::size_t m_verifiedBlockCount;
};

/*
 * BlockchainLoader rebuilds a Blockchain object from Nodo storage metadata and
 * block snapshot files.
 *
 * Expected storage layout:
 *
 * data/chain_manifest.nodo
 * data/block_index.nodo
 * data/blocks/block_<height>_<hash>.nodo
 */
class BlockchainLoader {
public:
    static BlockchainLoadReport auditStorageRoot(
        const std::string& rootDirectory
    );

    static core::Blockchain loadFromStorageRoot(
        const std::string& rootDirectory
    );

    // Re-executes every non-genesis block via StateTransitionEngine and
    // compares stateRoot and receiptsRoot to the block's declared values.
    // The contextBuilder is called with the partial chain (all blocks
    // preceding the candidate) and must return the account state needed
    // for state transition preview.
    static BlockchainCommitmentVerificationReport verifyChainCommitmentsViaEngine(
        const core::Blockchain& blockchain,
        std::function<core::StateTransitionPreviewContext(const core::Blockchain&)> contextBuilder
    );

private:
    static core::Blockchain buildBlockchainFromBlocks(
        const std::vector<core::Block>& blocks
    );
};

} // namespace nodo::storage

#endif