#include "core/LightClientProof.hpp"

#include "core/MerkleTree.hpp"

#include <sstream>
#include <utility>

namespace nodo::core {

// ---------------------------------------------------------------------------
// TransactionInclusionProof
// ---------------------------------------------------------------------------

TransactionInclusionProof::TransactionInclusionProof()
    : m_transactionId("")
    , m_blockHeight(0)
    , m_blockHash("")
    , m_transactionsRoot("")
    , m_merklePath()
    , m_isLeftChild(false)
{}

TransactionInclusionProof::TransactionInclusionProof(
    std::string transactionId,
    std::uint64_t blockHeight,
    std::string blockHash,
    std::string transactionsRoot,
    std::vector<std::string> merklePath,
    bool isLeftChild
)
    : m_transactionId(std::move(transactionId))
    , m_blockHeight(blockHeight)
    , m_blockHash(std::move(blockHash))
    , m_transactionsRoot(std::move(transactionsRoot))
    , m_merklePath(std::move(merklePath))
    , m_isLeftChild(isLeftChild)
{}

const std::string& TransactionInclusionProof::transactionId() const { return m_transactionId; }
std::uint64_t TransactionInclusionProof::blockHeight() const { return m_blockHeight; }
const std::string& TransactionInclusionProof::blockHash() const { return m_blockHash; }
const std::string& TransactionInclusionProof::transactionsRoot() const { return m_transactionsRoot; }
const std::vector<std::string>& TransactionInclusionProof::merklePath() const { return m_merklePath; }
bool TransactionInclusionProof::isLeftChild() const { return m_isLeftChild; }

bool TransactionInclusionProof::isValid() const {
    return !m_transactionId.empty() &&
           !m_blockHash.empty() &&
           !m_transactionsRoot.empty();
}

std::string TransactionInclusionProof::serialize() const {
    std::ostringstream oss;
    oss << "TransactionInclusionProof{"
        << "transactionId=" << m_transactionId
        << ";blockHeight=" << m_blockHeight
        << ";blockHash=" << m_blockHash
        << ";transactionsRoot=" << m_transactionsRoot
        << ";pathLen=" << m_merklePath.size()
        << ";isLeftChild=" << (m_isLeftChild ? "true" : "false")
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// AccountStateProof
// ---------------------------------------------------------------------------

AccountStateProof::AccountStateProof()
    : m_address("")
    , m_blockHeight(0)
    , m_blockHash("")
    , m_stateRoot("")
    , m_accountStateSnapshot("")
    , m_merklePath()
    , m_isLeftChild(false)
{}

AccountStateProof::AccountStateProof(
    std::string address,
    std::uint64_t blockHeight,
    std::string blockHash,
    std::string stateRoot,
    std::string accountStateSnapshot,
    std::vector<std::string> merklePath,
    bool isLeftChild
)
    : m_address(std::move(address))
    , m_blockHeight(blockHeight)
    , m_blockHash(std::move(blockHash))
    , m_stateRoot(std::move(stateRoot))
    , m_accountStateSnapshot(std::move(accountStateSnapshot))
    , m_merklePath(std::move(merklePath))
    , m_isLeftChild(isLeftChild)
{}

const std::string& AccountStateProof::address() const { return m_address; }
std::uint64_t AccountStateProof::blockHeight() const { return m_blockHeight; }
const std::string& AccountStateProof::blockHash() const { return m_blockHash; }
const std::string& AccountStateProof::stateRoot() const { return m_stateRoot; }
const std::string& AccountStateProof::accountStateSnapshot() const { return m_accountStateSnapshot; }
const std::vector<std::string>& AccountStateProof::merklePath() const { return m_merklePath; }
bool AccountStateProof::isLeftChild() const { return m_isLeftChild; }

bool AccountStateProof::isValid() const {
    return !m_address.empty() &&
           !m_blockHash.empty() &&
           !m_stateRoot.empty() &&
           !m_accountStateSnapshot.empty();
}

std::string AccountStateProof::serialize() const {
    std::ostringstream oss;
    oss << "AccountStateProof{"
        << "address=" << m_address
        << ";blockHeight=" << m_blockHeight
        << ";blockHash=" << m_blockHash
        << ";stateRoot=" << m_stateRoot
        << ";pathLen=" << m_merklePath.size()
        << ";isLeftChild=" << (m_isLeftChild ? "true" : "false")
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// LightClientVerifier
// ---------------------------------------------------------------------------

std::string LightClientVerifier::computeMerkleRoot(
    const std::string& leafHash,
    const std::vector<std::string>& path,
    bool startAsLeftChild
) {
    std::string current = leafHash;
    bool isLeft = startAsLeftChild;

    for (const auto& sibling : path) {
        if (isLeft) {
            // current is the left child; sibling is on the right
            current = MerkleTree::hashNode(current, sibling);
        } else {
            // current is the right child; sibling is on the left
            current = MerkleTree::hashNode(sibling, current);
        }
        // At each subsequent level the parent position alternates based on
        // tree structure, but since we only carry one boolean for the leaf
        // starting position we propagate it consistently upwards.
        // (For a single-bit proof the position at higher levels is encoded
        // in the path ordering; we use the same convention as MerkleProof.)
        isLeft = !isLeft;
    }

    return current;
}

bool LightClientVerifier::verifyTransactionInclusion(
    const TransactionInclusionProof& proof
) {
    if (!proof.isValid()) {
        return false;
    }

    // Hash the transaction ID as a leaf using domain-separated prefix
    const std::string leafHash = MerkleTree::hashLeaf(proof.transactionId());

    const std::string computedRoot = computeMerkleRoot(
        leafHash,
        proof.merklePath(),
        proof.isLeftChild()
    );

    return computedRoot == proof.transactionsRoot();
}

bool LightClientVerifier::verifyAccountState(
    const AccountStateProof& proof
) {
    if (!proof.isValid()) {
        return false;
    }

    // Hash the account state snapshot as a leaf
    const std::string leafHash = MerkleTree::hashLeaf(proof.accountStateSnapshot());

    const std::string computedRoot = computeMerkleRoot(
        leafHash,
        proof.merklePath(),
        proof.isLeftChild()
    );

    return computedRoot == proof.stateRoot();
}

} // namespace nodo::core
