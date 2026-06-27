#include "core/StateRootCalculator.hpp"

#include "core/MerkleTree.hpp"
#include "core/SparseMerkleTree.hpp"
#include "crypto/hash.h"

#include <sstream>
#include <vector>

namespace nodo::core {

std::string StateRootCalculator::calculateAccountStateRoot(
    const AccountStateView& view
) {
    if (!view.isValid()) {
        return "";
    }

    const std::vector<AccountState> accounts = view.accounts();

    SparseMerkleTree smt;

    for (const auto& account : accounts) {
        // Derive SMT 256-bit key from address hash
        char addressHash[NODO_HASH_BUFFER_SIZE] = {0};
        nodo_hash_string(account.address().c_str(), addressHash, sizeof(addressHash));
        std::string keyHex(addressHash);

        // Derive leaf value hash
        std::ostringstream oss;
        oss << "NODO_ACCOUNT_LEAF_V2{"
            << "address=" << account.address()
            << ";balance=" << account.balance().rawUnits()
            << ";nonce=" << account.nonce()
            << "}";
        std::string leafHash = MerkleTree::hashLeaf(oss.str());

        smt.update(keyHex, leafHash);
    }

    return smt.root();
}

std::string StateRootCalculator::canonicalAccountStatePayload(
    const AccountStateView& view
) {
    if (!view.isValid()) {
        return "";
    }

    std::ostringstream oss;
    oss << "NODO_ACCOUNT_STATE_ROOT_V2{";

    const std::vector<AccountState> accounts = view.accounts();
    oss << "accountCount=" << accounts.size() << ";merkleRoot="
        << calculateAccountStateRoot(view) << "}";

    return oss.str();
}

std::string StateRootCalculator::calculateProtocolStateRoot(
    const AccountStateView& view,
    const std::map<std::string, std::string>& deterministicDomains
) {
    const std::string accountRoot = calculateAccountStateRoot(view);
    if (accountRoot.empty()) {
        return "";
    }
    if (deterministicDomains.empty()) {
        return accountRoot;
    }

    std::vector<std::string> leaves;
    leaves.push_back("NODO_STATE_DOMAIN_V1{domain=accounts;root=" + accountRoot + "}");
    for (const auto& [domain, payload] : deterministicDomains) {
        if (domain.empty() || payload.empty()) {
            return "";
        }
        leaves.push_back(
            "NODO_STATE_DOMAIN_V1{domain=" + domain + ";payload=" + payload + "}"
        );
    }
    return MerkleTree::buildRoot(leaves);
}

} // namespace nodo::core
