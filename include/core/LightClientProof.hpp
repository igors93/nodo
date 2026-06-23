#ifndef NODO_CORE_LIGHT_CLIENT_PROOF_HPP
#define NODO_CORE_LIGHT_CLIENT_PROOF_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * TransactionInclusionProof proves that a transaction was included in a block
 * identified by blockHeight and blockHash, using a Merkle path from the
 * transaction leaf up to the transactionsRoot committed in the block header.
 *
 * Security principle:
 * A light client must not trust block headers it has not independently
 * verified. These proofs are only as strong as the header chain they
 * reference.
 */
class TransactionInclusionProof {
public:
    TransactionInclusionProof();

    TransactionInclusionProof(
        std::string transactionId,
        std::uint64_t blockHeight,
        std::string blockHash,
        std::string transactionsRoot,
        std::vector<std::string> merklePath,
        bool isLeftChild
    );

    const std::string& transactionId() const;
    std::uint64_t blockHeight() const;
    const std::string& blockHash() const;
    const std::string& transactionsRoot() const;
    const std::vector<std::string>& merklePath() const;
    bool isLeftChild() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_transactionId;
    std::uint64_t m_blockHeight;
    std::string m_blockHash;
    std::string m_transactionsRoot;
    std::vector<std::string> m_merklePath;
    bool m_isLeftChild;
};

/*
 * AccountStateProof proves that an account had a specific state (balance/nonce)
 * at a given block height, via a Merkle path from the account leaf up to the
 * stateRoot committed in the block header.
 */
class AccountStateProof {
public:
    AccountStateProof();

    AccountStateProof(
        std::string address,
        std::uint64_t blockHeight,
        std::string blockHash,
        std::string stateRoot,
        std::string accountStateSnapshot,
        std::vector<std::string> merklePath,
        bool isLeftChild
    );

    const std::string& address() const;
    std::uint64_t blockHeight() const;
    const std::string& blockHash() const;
    const std::string& stateRoot() const;
    const std::string& accountStateSnapshot() const;
    const std::vector<std::string>& merklePath() const;
    bool isLeftChild() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_address;
    std::uint64_t m_blockHeight;
    std::string m_blockHash;
    std::string m_stateRoot;
    std::string m_accountStateSnapshot;
    std::vector<std::string> m_merklePath;
    bool m_isLeftChild;
};

/*
 * LightClientVerifier verifies Merkle inclusion proofs without needing the
 * full block body or full state tree.
 *
 * Security principle:
 * Verification walks the Merkle path from the leaf hash upward, combining
 * sibling hashes at each level using the same domain-separated hash function
 * as MerkleTree, so a forged internal-node hash cannot impersonate a leaf.
 */
class LightClientVerifier {
public:
    // Verify that transactionId is in the block with the given transactionsRoot.
    static bool verifyTransactionInclusion(
        const TransactionInclusionProof& proof
    );

    // Verify that accountSnapshot hashes to a leaf that is in stateRoot.
    static bool verifyAccountState(
        const AccountStateProof& proof
    );

private:
    static std::string computeMerkleRoot(
        const std::string& leafHash,
        const std::vector<std::string>& path,
        bool startAsLeftChild
    );
};

} // namespace nodo::core

#endif
