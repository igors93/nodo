#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/ValidationWorkRecord.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::LedgerRecord;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;

constexpr std::int64_t kTimestamp = 1900000000;

static const std::string kValidHash64 =
    "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2";

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ValidationWorkRecord workRecord(const std::string& id) {
    return ValidationWorkRecord(
        "canonical-root-test-validator",
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "canonical-root-test-target-" + id,
        id,
        1,
        kTimestamp
    );
}

// ---------------------------------------------------------------------------
// isCanonicalCommitmentRoot tests
// ---------------------------------------------------------------------------

void testAcceptsValid64CharLowercaseHex() {
    requireCondition(
        Block::isCanonicalCommitmentRoot(kValidHash64),
        "64-char lowercase hex string must be accepted as canonical."
    );
}

void testAcceptsAllZeroHash() {
    requireCondition(
        Block::isCanonicalCommitmentRoot(
            "0000000000000000000000000000000000000000000000000000000000000000"
        ),
        "64-char all-zero string must be accepted as canonical."
    );
}

void testAcceptsMaxHexHash() {
    requireCondition(
        Block::isCanonicalCommitmentRoot(
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        ),
        "64-char all-'f' string must be accepted as canonical."
    );
}

void testRejects63CharString() {
    requireCondition(
        !Block::isCanonicalCommitmentRoot(
            "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1"
        ),
        "63-char string must be rejected (too short)."
    );
}

void testRejects65CharString() {
    requireCondition(
        !Block::isCanonicalCommitmentRoot(
            "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c"
        ),
        "65-char string must be rejected (too long)."
    );
}

void testRejectsEmptyString() {
    requireCondition(
        !Block::isCanonicalCommitmentRoot(""),
        "Empty string must be rejected."
    );
}

void testRejectsUppercaseHex() {
    requireCondition(
        !Block::isCanonicalCommitmentRoot(
            "A1B2C3D4E5F6A1B2C3D4E5F6A1B2C3D4E5F6A1B2C3D4E5F6A1B2C3D4E5F6A1B2"
        ),
        "64-char uppercase hex must be rejected (not canonical)."
    );
}

void testRejectsMixedCaseHex() {
    requireCondition(
        !Block::isCanonicalCommitmentRoot(
            "a1B2c3D4e5F6a1B2c3D4e5F6a1B2c3D4e5F6a1B2c3D4e5F6a1B2c3D4e5F6a1B2"
        ),
        "Mixed-case hex must be rejected."
    );
}

void testRejectsNonHexChars() {
    requireCondition(
        !Block::isCanonicalCommitmentRoot(
            "g1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2"
        ),
        "String containing 'g' must be rejected (not hex)."
    );
    requireCondition(
        !Block::isCanonicalCommitmentRoot(
            "DRAFT_STATE_ROOT_____________________________________________________"
        ),
        "Placeholder string must be rejected."
    );
}

void testRejectsPlaceholderStrings() {
    requireCondition(
        !Block::isCanonicalCommitmentRoot("DRAFT_STATE_ROOT"),
        "DRAFT_STATE_ROOT must be rejected."
    );
    requireCondition(
        !Block::isCanonicalCommitmentRoot("DRAFT_RECEIPTS_ROOT"),
        "DRAFT_RECEIPTS_ROOT must be rejected."
    );
    requireCondition(
        !Block::isCanonicalCommitmentRoot("wrong-state-root"),
        "Hyphenated placeholder must be rejected."
    );
    requireCondition(
        !Block::isCanonicalCommitmentRoot("placeholder-receipts-root"),
        "Placeholder receipts root string must be rejected."
    );
}

// ---------------------------------------------------------------------------
// Block::isValid(requireProtocolCommitments) integration tests
// ---------------------------------------------------------------------------

void testGenesisBlockPassesIsValidWithEmptyRoots() {
    const Block genesis = Block::createGenesisBlock(
        {LedgerRecord::fromValidationWorkRecord(workRecord("genesis"), kTimestamp + 1)},
        kTimestamp + 2
    );

    requireCondition(
        genesis.isValid(true),
        "Genesis block (index 0) must pass isValid(true) even with empty roots."
    );
}

void testNonGenesisBlockWithCanonicalRootsPassesIsValid() {
    const Block block(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("block1"), kTimestamp + 1)},
        kTimestamp + 1,
        kValidHash64,
        kValidHash64
    );

    requireCondition(
        block.isValid(true),
        "Non-genesis block with canonical roots must pass isValid(true)."
    );
}

void testNonGenesisBlockWithNonCanonicalStateRootFailsIsValid() {
    const Block block(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("block1-bad"), kTimestamp + 1)},
        kTimestamp + 1,
        "wrong-state-root",
        kValidHash64
    );

    requireCondition(
        !block.isValid(true),
        "Non-genesis block with non-canonical stateRoot must fail isValid(true)."
    );
}

void testNonGenesisBlockWithNonCanonicalReceiptsRootFailsIsValid() {
    const Block block(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("block1-bad-rx"), kTimestamp + 1)},
        kTimestamp + 1,
        kValidHash64,
        "DRAFT_RECEIPTS_ROOT"
    );

    requireCondition(
        !block.isValid(true),
        "Non-genesis block with non-canonical receiptsRoot must fail isValid(true)."
    );
}

void testNonGenesisBlockPassesIsValidWithoutProtocolRequirement() {
    const Block block(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("block1-structural"), kTimestamp + 1)},
        kTimestamp + 1,
        "",
        ""
    );

    requireCondition(
        block.isValid(false),
        "Non-genesis block with empty roots must pass isValid(false) in structural mode."
    );
}

void testHasCanonicalStateRootHelper() {
    const Block goodBlock(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("good"), kTimestamp + 1)},
        kTimestamp + 1,
        kValidHash64,
        kValidHash64
    );
    requireCondition(goodBlock.hasCanonicalStateRoot(), "hasCanonicalStateRoot must be true for canonical root.");

    const Block badBlock(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("bad"), kTimestamp + 2)},
        kTimestamp + 2,
        "DRAFT_STATE_ROOT",
        kValidHash64
    );
    requireCondition(!badBlock.hasCanonicalStateRoot(), "hasCanonicalStateRoot must be false for placeholder.");
}

void testHasCanonicalReceiptsRootHelper() {
    const Block goodBlock(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("good-rx"), kTimestamp + 1)},
        kTimestamp + 1,
        kValidHash64,
        kValidHash64
    );
    requireCondition(goodBlock.hasCanonicalReceiptsRoot(), "hasCanonicalReceiptsRoot must be true for canonical root.");

    const Block badBlock(
        1,
        "previoushashplaceholder000000000000000000000000000000000000000000",
        {LedgerRecord::fromValidationWorkRecord(workRecord("bad-rx"), kTimestamp + 2)},
        kTimestamp + 2,
        kValidHash64,
        "wrong-receipts-root"
    );
    requireCondition(!badBlock.hasCanonicalReceiptsRoot(), "hasCanonicalReceiptsRoot must be false for non-canonical.");
}

} // namespace

int main() {
    try {
        testAcceptsValid64CharLowercaseHex();
        testAcceptsAllZeroHash();
        testAcceptsMaxHexHash();
        testRejects63CharString();
        testRejects65CharString();
        testRejectsEmptyString();
        testRejectsUppercaseHex();
        testRejectsMixedCaseHex();
        testRejectsNonHexChars();
        testRejectsPlaceholderStrings();
        testGenesisBlockPassesIsValidWithEmptyRoots();
        testNonGenesisBlockWithCanonicalRootsPassesIsValid();
        testNonGenesisBlockWithNonCanonicalStateRootFailsIsValid();
        testNonGenesisBlockWithNonCanonicalReceiptsRootFailsIsValid();
        testNonGenesisBlockPassesIsValidWithoutProtocolRequirement();
        testHasCanonicalStateRootHelper();
        testHasCanonicalReceiptsRootHelper();

        std::cout << "Block canonical root tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
