#include "core/StateRootCalculator.hpp"

#include "core/MerkleTree.hpp"

#include <sstream>
#include <vector>

namespace nodo::core {

std::string StateRootCalculator::calculateAccountStateRoot(
    const AccountStateView& view
) {
    if (!view.isValid()) {
        return "";
    }

    // Build one leaf payload per account: "address:balance:nonce".
    // Leaves are sorted by address inside MerkleTree::buildRoot so the
    // result is deterministic regardless of insertion order.
    const std::vector<AccountState> accounts = view.accounts();

    std::vector<std::string> leafPayloads;
    leafPayloads.reserve(accounts.size());

    for (const auto& account : accounts) {
        std::ostringstream oss;
        oss << "NODO_ACCOUNT_LEAF_V2{"
            << "address=" << account.address()
            << ";balance=" << account.balance().rawUnits()
            << ";nonce=" << account.nonce()
            << "}";
        leafPayloads.push_back(oss.str());
    }

    return MerkleTree::buildRoot(std::move(leafPayloads));
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

} // namespace nodo::core
