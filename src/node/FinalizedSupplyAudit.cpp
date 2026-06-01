#include "node/FinalizedSupplyAudit.hpp"

#include "economics/SupplyAudit.hpp"

#include <exception>
#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

FinalizedSupplyAuditResult fromSequenceAudit(
    const economics::SupplySequenceAuditResult& audit
) {
    if (audit.isValid()) {
        return FinalizedSupplyAuditResult::passed(
            audit.finalSupply(),
            audit.deltaCount()
        );
    }

    return FinalizedSupplyAuditResult::failed(
        "FinalizedSupplyAudit: " + audit.reason(),
        audit.failedBlockHeight()
    );
}

FinalizedSupplyAuditResult validateDeltaHeights(
    const std::vector<economics::SupplyDelta>& deltas
) {
    // Finalized artifacts start at block height 1. The genesis block (height 0)
    // is handled separately during node bootstrap and is NOT stored as a
    // FinalizedBlockArtifact. Any sequence that starts at height 0 is therefore
    // invalid in this audit context.
    std::uint64_t expectedHeight = 1;

    for (const auto& delta : deltas) {
        if (delta.blockHeight() != expectedHeight) {
            return FinalizedSupplyAuditResult::failed(
                "FinalizedSupplyAudit: out-of-order or non-contiguous "
                "SupplyDelta height: expected " +
                std::to_string(expectedHeight) + " got " +
                std::to_string(delta.blockHeight()) + ".",
                delta.blockHeight()
            );
        }

        ++expectedHeight;
    }

    return FinalizedSupplyAuditResult::passed(
        utils::Amount::fromRawUnits(0),
        deltas.size()
    );
}

} // namespace

FinalizedSupplyAuditResult::FinalizedSupplyAuditResult()
    : m_passed(false),
      m_reason("Uninitialized finalized supply audit result."),
      m_finalSupply(utils::Amount::fromRawUnits(0)),
      m_deltaCount(0),
      m_failedBlockHeight(0) {}

FinalizedSupplyAuditResult FinalizedSupplyAuditResult::passed(
    utils::Amount finalSupply,
    std::size_t deltaCount
) {
    FinalizedSupplyAuditResult result;
    result.m_passed = true;
    result.m_reason = "";
    result.m_finalSupply = finalSupply;
    result.m_deltaCount = deltaCount;
    result.m_failedBlockHeight = 0;
    return result;
}

FinalizedSupplyAuditResult FinalizedSupplyAuditResult::failed(
    std::string reason,
    std::uint64_t failedBlockHeight
) {
    FinalizedSupplyAuditResult result;
    result.m_passed = false;
    result.m_reason = std::move(reason);
    result.m_failedBlockHeight = failedBlockHeight;
    return result;
}

bool FinalizedSupplyAuditResult::passed() const {
    return m_passed;
}

const std::string& FinalizedSupplyAuditResult::reason() const {
    return m_reason;
}

utils::Amount FinalizedSupplyAuditResult::finalSupply() const {
    return m_finalSupply;
}

std::size_t FinalizedSupplyAuditResult::deltaCount() const {
    return m_deltaCount;
}

std::uint64_t FinalizedSupplyAuditResult::failedBlockHeight() const {
    return m_failedBlockHeight;
}

std::string FinalizedSupplyAuditResult::serialize() const {
    std::ostringstream oss;
    oss << "FinalizedSupplyAuditResult{"
        << "passed=" << (m_passed ? "1" : "0")
        << ";deltaCount=" << m_deltaCount
        << ";finalSupplyRaw=" << m_finalSupply.rawUnits()
        << ";failedBlockHeight=" << m_failedBlockHeight
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

FinalizedSupplyAuditResult FinalizedSupplyAudit::auditArtifacts(
    const economics::MonetaryPolicy& policy,
    const std::vector<FinalizedBlockArtifact>& artifacts
) {
    std::vector<economics::SupplyDelta> deltas;
    deltas.reserve(artifacts.size());

    std::uint64_t expectedHeight = 1;

    for (const auto& artifact : artifacts) {
        const economics::SupplyDelta& delta = artifact.supplyDelta();

        if (!delta.isValid()) {
            return FinalizedSupplyAuditResult::failed(
                "FinalizedSupplyAudit: finalized artifact has invalid or "
                "missing SupplyDelta: " + delta.rejectionReason(),
                delta.blockHeight()
            );
        }

        try {
            const core::Block& block = artifact.block();

            if (block.index() != expectedHeight) {
                return FinalizedSupplyAuditResult::failed(
                    "FinalizedSupplyAudit: out-of-order finalized artifact "
                    "height: expected " +
                    std::to_string(expectedHeight) + " got " +
                    std::to_string(block.index()) + ".",
                    block.index()
                );
            }

            if (delta.blockHeight() != block.index() ||
                delta.blockHash() != block.hash()) {
                return FinalizedSupplyAuditResult::failed(
                    "FinalizedSupplyAudit: SupplyDelta block identity does "
                    "not match finalized artifact block.",
                    delta.blockHeight()
                );
            }

            ++expectedHeight;
        } catch (const std::exception& error) {
            return FinalizedSupplyAuditResult::failed(
                std::string("FinalizedSupplyAudit: finalized artifact has no "
                            "auditable block: ") + error.what(),
                delta.blockHeight()
            );
        }

        deltas.push_back(delta);
    }

    return auditDeltas(policy, deltas);
}

FinalizedSupplyAuditResult FinalizedSupplyAudit::auditDeltas(
    const economics::MonetaryPolicy& policy,
    const std::vector<economics::SupplyDelta>& deltas
) {
    if (!policy.isValid()) {
        return fromSequenceAudit(
            economics::SupplyAudit::auditDeltaSequence(policy, deltas)
        );
    }

    const FinalizedSupplyAuditResult heightAudit =
        validateDeltaHeights(deltas);

    if (!heightAudit.passed()) {
        return heightAudit;
    }

    return fromSequenceAudit(
        economics::SupplyAudit::auditDeltaSequence(policy, deltas)
    );
}

} // namespace nodo::node
