#include "node/ProtocolInvariantChecker.hpp"

#include "node/RuntimeStateVerifier.hpp"

#include <exception>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

class InvariantAudit {
public:
    InvariantAudit()
        : m_checked(0) {}

    ProtocolInvariantCheckResult fail(
        std::string reason
    ) const {
        return ProtocolInvariantCheckResult::failed(
            std::move(reason),
            m_checked
        );
    }

    void checked() {
        ++m_checked;
    }

    std::size_t count() const {
        return m_checked;
    }

private:
    std::size_t m_checked;
};

ProtocolInvariantCheckResult checkFinalizedRecordsConnect(
    const NodeRuntime& runtime,
    InvariantAudit& audit
) {
    const core::Blockchain& chain =
        runtime.blockchain();

    const consensus::BlockFinalizationRegistry& finality =
        runtime.finalizationRegistry();

    const std::uint64_t highestFinalizedHeight =
        finality.highestFinalizedHeight();

    audit.checked();
    if (highestFinalizedHeight > chain.latestBlock().index()) {
        return audit.fail("Finalized height exceeds latest chain height.");
    }

    for (std::uint64_t height = 1; height <= highestFinalizedHeight; ++height) {
        const consensus::FinalizedBlockRecord* record =
            finality.recordForHeight(height);

        audit.checked();
        if (record == nullptr) {
            return audit.fail("Finalization registry has a gap before its highest finalized height.");
        }

        if (height >= chain.blocks().size()) {
            return audit.fail("Finalized record height is outside the local chain.");
        }

        const core::Block& block =
            chain.blocks()[static_cast<std::size_t>(height)];

        if (!record->isStructurallyValid() ||
            !record->matchesBlock(block)) {
            return audit.fail("Finalized record does not match its local chain block.");
        }

        if (block.previousHash() != chain.blocks()[static_cast<std::size_t>(height - 1)].hash()) {
            return audit.fail("Finalized block does not connect to its previous local block.");
        }
    }

    return ProtocolInvariantCheckResult::passed(audit.count());
}

} // namespace

ProtocolInvariantCheckResult::ProtocolInvariantCheckResult()
    : m_passed(false),
      m_reason("Uninitialized protocol invariant check result."),
      m_checkedInvariantCount(0) {}

ProtocolInvariantCheckResult ProtocolInvariantCheckResult::passed(
    std::size_t checkedInvariantCount
) {
    ProtocolInvariantCheckResult result;
    result.m_passed = true;
    result.m_reason = "";
    result.m_checkedInvariantCount = checkedInvariantCount;
    return result;
}

ProtocolInvariantCheckResult ProtocolInvariantCheckResult::failed(
    std::string reason,
    std::size_t checkedInvariantCount
) {
    ProtocolInvariantCheckResult result;
    result.m_passed = false;
    result.m_reason = std::move(reason);
    result.m_checkedInvariantCount = checkedInvariantCount;
    return result;
}

bool ProtocolInvariantCheckResult::passed() const {
    return m_passed;
}

const std::string& ProtocolInvariantCheckResult::reason() const {
    return m_reason;
}

std::size_t ProtocolInvariantCheckResult::checkedInvariantCount() const {
    return m_checkedInvariantCount;
}

std::string ProtocolInvariantCheckResult::serialize() const {
    std::ostringstream oss;

    oss << "ProtocolInvariantCheckResult{"
        << "passed=" << (m_passed ? "true" : "false")
        << ";reason=" << m_reason
        << ";checkedInvariantCount=" << m_checkedInvariantCount
        << "}";

    return oss.str();
}

ProtocolInvariantCheckResult ProtocolInvariantChecker::checkRuntime(
    const NodeRuntime& runtime
) {
    InvariantAudit audit;

    audit.checked();
    if (!runtime.isRunning()) {
        return audit.fail("Runtime is not running.");
    }

    audit.checked();
    if (!runtime.config().isValid()) {
        return audit.fail("Runtime config is invalid.");
    }

    const core::Blockchain& chain =
        runtime.blockchain();

    audit.checked();
    if (chain.empty() || !chain.isValid()) {
        return audit.fail("Runtime blockchain is empty or invalid.");
    }

    audit.checked();
    if (chain.latestBlock().index() + 1 != chain.size()) {
        return audit.fail("Chain tip height does not match local chain length.");
    }

    audit.checked();
    if (chain.latestBlock().hash().empty()) {
        return audit.fail("Chain tip hash is empty.");
    }

    audit.checked();
    if (!runtime.validatorRegistry().isValid()) {
        return audit.fail("Validator registry is invalid.");
    }

    const ProtocolInvariantCheckResult penaltyLedgerCheck =
        checkPenaltyLedger(runtime.validatorPenaltyLedger());
    if (!penaltyLedgerCheck.passed()) {
        return penaltyLedgerCheck;
    }

    audit.checked();
    if (runtime.validatorRegistry().activeCount() <
        runtime.config().genesisConfig().networkParameters().minimumValidatorCount()) {
        return audit.fail("Active validator count is below the network minimum.");
    }

    for (const std::string& validatorAddress :
         runtime.validatorRegistry().activeValidatorAddresses()) {
        const core::ValidatorRegistryEntry* entry =
            runtime.validatorRegistry().entryForAddress(validatorAddress);

        audit.checked();
        if (entry == nullptr ||
            !entry->isValid() ||
            !entry->active()) {
            return audit.fail("Invalid validator appears in the active validator set.");
        }

        if (!runtime.validatorRegistry().verifyValidatorIdentity(
                validatorAddress,
                entry->registrationRecord().validatorPublicKey()
            )) {
            return audit.fail("Active validator identity binding failed.");
        }
    }

    audit.checked();
    if (!runtime.finalizationRegistry().isValid()) {
        return audit.fail("Finalization registry is invalid.");
    }

    const ProtocolInvariantCheckResult finalizedRecords =
        checkFinalizedRecordsConnect(
            runtime,
            audit
        );

    if (!finalizedRecords.passed()) {
        return finalizedRecords;
    }

    try {
        const std::string latestStateRoot =
            RuntimeStateVerifier::calculateLatestStateRoot(
                runtime.config().genesisConfig(),
                chain
            );

        audit.checked();
        if (latestStateRoot.empty()) {
            return audit.fail("Latest state root is empty.");
        }
    } catch (const std::exception& error) {
        return audit.fail(std::string("Latest state root calculation failed: ") + error.what());
    }

    audit.checked();
    if (!runtime.peerManager().isValid()) {
        return audit.fail("Peer manager is invalid.");
    }

    return ProtocolInvariantCheckResult::passed(
        audit.count() + penaltyLedgerCheck.checkedInvariantCount()
    );
}

ProtocolInvariantCheckResult ProtocolInvariantChecker::checkRuntimeAgainstManifest(
    const NodeRuntime& runtime,
    const NodeRuntimeManifest& manifest
) {
    ProtocolInvariantCheckResult runtimeCheck =
        checkRuntime(runtime);

    if (!runtimeCheck.passed()) {
        return runtimeCheck;
    }

    InvariantAudit audit;

    const RuntimeStateVerificationResult manifestVerification =
        RuntimeStateVerifier::verifyManifestMatchesRuntime(
            manifest,
            runtime
        );

    audit.checked();
    if (!manifestVerification.verified()) {
        return audit.fail(manifestVerification.reason());
    }

    audit.checked();
    if (manifest.validatorCount() != runtime.validatorRegistry().activeCount()) {
        return audit.fail("Manifest validator count does not match active registry count.");
    }

    audit.checked();
    if (manifest.peerCount() != runtime.peerManager().size()) {
        return audit.fail("Manifest peer count does not match peer manager.");
    }

    audit.checked();
    if (runtime.finalizationRegistry().highestFinalizedHeight() >
        manifest.latestBlockHeight()) {
        return audit.fail("Finalized height exceeds manifest latest height.");
    }

    return ProtocolInvariantCheckResult::passed(
        runtimeCheck.checkedInvariantCount() + audit.count()
    );
}

ProtocolInvariantCheckResult ProtocolInvariantChecker::checkPenaltyLedger(
    const consensus::ValidatorPenaltyLedger& ledger
) {
    InvariantAudit audit;

    audit.checked();
    if (!ledger.isValid()) {
        return audit.fail("Penalty ledger is invalid.");
    }

    std::set<std::string> evidenceIds;
    std::set<std::string> penaltyIds;

    std::int64_t totalSlash = 0;

    for (const consensus::ValidatorPenaltyDecision& decision :
         ledger.allDecisions()) {
        audit.checked();
        if (!decision.isValid()) {
            return audit.fail("Penalty ledger contains an invalid decision.");
        }

        if (!evidenceIds.insert(decision.evidenceId()).second) {
            return audit.fail("Slashable evidence produced more than one penalty decision.");
        }

        if (!penaltyIds.insert(decision.penaltyId()).second) {
            return audit.fail("Penalty decision appears more than once.");
        }

        if (decision.slashAmountRawUnits() < 0) {
            return audit.fail("Penalty decision contains a negative slash amount.");
        }

        if (decision.slashAmountRawUnits() >
            std::numeric_limits<std::int64_t>::max() - totalSlash) {
            return audit.fail("Penalty ledger slash accounting overflowed.");
        }

        totalSlash += decision.slashAmountRawUnits();
    }

    return ProtocolInvariantCheckResult::passed(audit.count());
}

} // namespace nodo::node
