#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/ValidatorRegistry.hpp"
#include "node/StateReplayAuditor.hpp"
#include "node/StateSnapshot.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

const std::string BLOCK_HASH =
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";

nodo::core::AccountStateView makeAccounts(std::int64_t firstBalance) {
    nodo::core::AccountStateView view;
    assert(view.putAccount(
        nodo::core::AccountState(
            "account-a",
            nodo::utils::Amount::fromRawUnits(firstBalance),
            1
        )
    ));
    assert(view.putAccount(
        nodo::core::AccountState(
            "account-b",
            nodo::utils::Amount::fromRawUnits(200),
            2
        )
    ));
    return view;
}

} // namespace

int main() {
    nodo::core::ValidatorRegistry registry;
    const std::vector<nodo::core::LedgerRecord> ledgerRecords;

    const nodo::node::StateSnapshot snapshot =
        nodo::node::StateSnapshot::create(
            20,
            BLOCK_HASH,
            makeAccounts(100),
            registry,
            ledgerRecords,
            1700000000
        );

    assert(snapshot.isValid());
    assert(snapshot.canonicalDigest().size() == 64);
    assert(snapshot.serialize().find("StateSnapshot") != std::string::npos);

    const nodo::node::StateReplayAuditResult validAudit =
        nodo::node::StateReplayAuditor::auditSnapshot(
            snapshot,
            makeAccounts(100),
            registry,
            ledgerRecords
        );

    assert(validAudit.valid());

    const nodo::node::StateReplayAuditResult mismatchAudit =
        nodo::node::StateReplayAuditor::auditSnapshot(
            snapshot,
            makeAccounts(101),
            registry,
            ledgerRecords
        );

    assert(!mismatchAudit.valid());
    assert(mismatchAudit.status() == nodo::node::StateReplayAuditStatus::ROOT_MISMATCH);
    assert(mismatchAudit.expectedRoot() != mismatchAudit.actualRoot());

    return 0;
}
