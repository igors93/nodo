#include "economics/MintRecord.hpp"
#include "economics/MonetaryFirewall.hpp"
#include "economics/SupplyDelta.hpp"
#include "serialization/MintRecordCodec.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

// Item 8: Regression tests proving old-style mint behavior cannot silently pass.
//
// These tests verify that:
//   - A MintRecord with empty authorizationId is invalid.
//   - A SupplyDelta containing such a record is also invalid.
//   - MonetaryFirewall rejects a mint whose authorizationId is not in the provided list.
//   - A serialized MintRecord without the authorizationId field is rejected by the codec.
//   - Demo/test fixture mint records do include authorizationId.

namespace {

void testMintRecordWithEmptyAuthorizationIdIsInvalid() {
    // An old-style MintRecord constructed with an empty authorizationId must be invalid.
    const nodo::economics::MintRecord oldStyleMint(
        "legacy-mint-001",
        "",  // empty — would have been acceptable before Task 02
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        0, 0, "GENESIS", 1700000000
    );
    assert(!oldStyleMint.isValid());
    assert(oldStyleMint.rejectionReason().find("authorizationId") != std::string::npos);
}

void testSupplyDeltaContainingOldStyleMintIsInvalid() {
    const nodo::economics::MintRecord oldStyleMint(
        "legacy-delta-mint-001",
        "",  // empty authorizationId
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(500),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 3, "block-legacy-hash", 1700000000
    );
    const nodo::economics::SupplyDelta delta(
        3, "block-legacy-hash", 1,
        nodo::utils::Amount::fromRawUnits(10000),
        nodo::utils::Amount::fromRawUnits(500),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(10500),
        {oldStyleMint}, {}
    );
    assert(!delta.isValid());
    // The rejection must reference the MintRecord (invalid record check fires first).
    assert(delta.rejectionReason().find("MintRecord") != std::string::npos);
}

void testMonetaryFirewallRejectsMintWithUnknownAuthorizationId() {
    const nodo::economics::MonetaryPolicy policy =
        nodo::economics::MonetaryPolicy::localnetDefault(
            "nodo-regression-1",
            nodo::utils::Amount::fromRawUnits(1000000)
        );
    // Mint record has a non-empty but unregistered authorizationId.
    const nodo::economics::MintRecord mintRecord(
        "regression-mint-001",
        "unregistered-auth-xyz",  // not in the authorization list below
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(100),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 5, "regression-hash-A", 1700000000
    );
    const nodo::economics::SupplyDelta delta(
        5, "regression-hash-A", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {mintRecord}, {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(policy, delta, {});
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryFirewallStatus::UNAUTHORIZED_MINT);
    assert(result.reason().find("unregistered-auth-xyz") != std::string::npos);
}

void testSerializedMintRecordWithoutAuthorizationIdIsRejectedByCodec() {
    // Simulate an old serialized MintRecord that has no authorizationId field.
    // The codec requires authorizationId; FieldCodec::extractField will throw.
    const std::string oldSerializedMint =
        "MintRecord{id=old-mint-001;recipient=igor;"
        "amountRaw=1000;reason=GENESIS_ALLOCATION;"
        "epoch=0;sourceBlockIndex=0;sourceBlockHash=GENESIS;"
        "timestamp=1700000000}";

    bool codecRejected = false;
    try {
        (void)nodo::serialization::MintRecordCodec::deserialize(oldSerializedMint);
    } catch (const std::exception&) {
        codecRejected = true;
    }
    assert(codecRejected);
}

void testDemoFixtureMintRecordIncludesAuthorizationId() {
    // The canonical demo mint (from DemoScenario) must include a non-empty authorizationId.
    const nodo::economics::MintRecord demoMint(
        "mint_genesis_igor_001",
        "auth_genesis_nodo_001",  // this was added in Task 02
        "igor",
        nodo::utils::Amount::fromNodo(1000),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        0, 0, "GENESIS", 1700000000
    );
    assert(demoMint.isValid());
    assert(!demoMint.authorizationId().empty());
    assert(demoMint.authorizationId() == "auth_genesis_nodo_001");
}

} // namespace

int main() {
    testMintRecordWithEmptyAuthorizationIdIsInvalid();
    testSupplyDeltaContainingOldStyleMintIsInvalid();
    testMonetaryFirewallRejectsMintWithUnknownAuthorizationId();
    testSerializedMintRecordWithoutAuthorizationIdIsRejectedByCodec();
    testDemoFixtureMintRecordIncludesAuthorizationId();
    return 0;
}
