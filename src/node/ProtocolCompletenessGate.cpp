#include "node/ProtocolCompletenessGate.hpp"

#include "core/StateRootCalculator.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/ProtocolInvariantChecker.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/InboundMessageValidator.hpp"

#include <exception>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

using RequirementList =
    std::vector<ProtocolCompletenessRequirement>;

void addRequirement(
    RequirementList& requirements,
    const std::string& id,
    const std::string& description,
    bool satisfied,
    const std::string& detail
) {
    requirements.emplace_back(
        id,
        description,
        satisfied ? ProtocolCompletenessStatus::SATISFIED
                  : ProtocolCompletenessStatus::FAILED,
        detail
    );
}

std::int64_t minimumFeeRawUnits(
    const config::NetworkParameters& parameters
) {
    if (parameters.minimumFeeRawUnits() >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("Network minimum fee does not fit signed amount range.");
    }

    return static_cast<std::int64_t>(
        parameters.minimumFeeRawUnits()
    );
}

bool hasEconomicSecurityRules(
    const config::NetworkParameters& parameters
) {
    return parameters.isValid() &&
           parameters.minimumValidatorCount() > 0 &&
           parameters.quorumThresholdNumerator() > 0 &&
           parameters.quorumThresholdDenominator() > 0 &&
           parameters.quorumThresholdNumerator() <=
               parameters.quorumThresholdDenominator() &&
           parameters.maxTransactionsPerBlock() > 0 &&
           parameters.maxMempoolTransactions() > 0 &&
           parameters.targetBlockTimeSeconds() > 0 &&
           parameters.finalityDepth() > 0;
}

} // namespace

std::string protocolCompletenessStatusToString(
    ProtocolCompletenessStatus status
) {
    switch (status) {
        case ProtocolCompletenessStatus::SATISFIED:
            return "SATISFIED";
        case ProtocolCompletenessStatus::FAILED:
            return "FAILED";
        default:
            return "FAILED";
    }
}

ProtocolCompletenessRequirement::ProtocolCompletenessRequirement()
    : m_id(""),
      m_description(""),
      m_status(ProtocolCompletenessStatus::FAILED),
      m_detail("Uninitialized protocol completeness requirement.") {}

ProtocolCompletenessRequirement::ProtocolCompletenessRequirement(
    std::string id,
    std::string description,
    ProtocolCompletenessStatus status,
    std::string detail
)
    : m_id(std::move(id)),
      m_description(std::move(description)),
      m_status(status),
      m_detail(std::move(detail)) {}

const std::string& ProtocolCompletenessRequirement::id() const {
    return m_id;
}

const std::string& ProtocolCompletenessRequirement::description() const {
    return m_description;
}

ProtocolCompletenessStatus ProtocolCompletenessRequirement::status() const {
    return m_status;
}

const std::string& ProtocolCompletenessRequirement::detail() const {
    return m_detail;
}

bool ProtocolCompletenessRequirement::satisfied() const {
    return m_status == ProtocolCompletenessStatus::SATISFIED;
}

std::string ProtocolCompletenessRequirement::serialize() const {
    std::ostringstream oss;

    oss << "ProtocolCompletenessRequirement{"
        << "id=" << m_id
        << ";description=" << m_description
        << ";status=" << protocolCompletenessStatusToString(m_status)
        << ";detail=" << m_detail
        << "}";

    return oss.str();
}

ProtocolCompletenessReport::ProtocolCompletenessReport()
    : m_requirements() {}

ProtocolCompletenessReport::ProtocolCompletenessReport(
    std::vector<ProtocolCompletenessRequirement> requirements
)
    : m_requirements(std::move(requirements)) {}

const std::vector<ProtocolCompletenessRequirement>&
ProtocolCompletenessReport::requirements() const {
    return m_requirements;
}

bool ProtocolCompletenessReport::complete() const {
    if (m_requirements.empty()) {
        return false;
    }

    for (const ProtocolCompletenessRequirement& requirement :
         m_requirements) {
        if (!requirement.satisfied()) {
            return false;
        }
    }

    return true;
}

std::size_t ProtocolCompletenessReport::satisfiedCount() const {
    std::size_t count = 0;

    for (const ProtocolCompletenessRequirement& requirement :
         m_requirements) {
        if (requirement.satisfied()) {
            ++count;
        }
    }

    return count;
}

std::size_t ProtocolCompletenessReport::failedCount() const {
    return m_requirements.size() - satisfiedCount();
}

std::string ProtocolCompletenessReport::firstFailure() const {
    for (const ProtocolCompletenessRequirement& requirement :
         m_requirements) {
        if (!requirement.satisfied()) {
            return requirement.id() + ": " + requirement.detail();
        }
    }

    return "";
}

std::string ProtocolCompletenessReport::serialize() const {
    std::ostringstream oss;

    oss << "ProtocolCompletenessReport{"
        << "complete=" << (complete() ? "true" : "false")
        << ";satisfiedCount=" << satisfiedCount()
        << ";failedCount=" << failedCount()
        << ";requirements=[";

    for (std::size_t index = 0; index < m_requirements.size(); ++index) {
        if (index != 0) {
            oss << ",";
        }
        oss << m_requirements[index].serialize();
    }

    oss << "]}";

    return oss.str();
}

ProtocolCompletenessReport ProtocolCompletenessGate::evaluateRuntime(
    const NodeRuntime& runtime
) {
    return evaluate(runtime, nullptr);
}

ProtocolCompletenessReport ProtocolCompletenessGate::evaluateRuntimeWithStorage(
    const NodeRuntime& runtime,
    const NodeDataDirectoryConfig& directoryConfig
) {
    return evaluate(runtime, &directoryConfig);
}

ProtocolCompletenessReport ProtocolCompletenessGate::evaluate(
    const NodeRuntime& runtime,
    const NodeDataDirectoryConfig* directoryConfig
) {
    RequirementList requirements;

    const config::NetworkParameters& parameters =
        runtime.config().genesisConfig().networkParameters();

    const ProtocolInvariantCheckResult runtimeInvariants =
        ProtocolInvariantChecker::checkRuntime(runtime);

    addRequirement(
        requirements,
        "consensus_fork_choice_finality",
        "Runtime has a valid chain, validator set, fork summary and finality registry.",
        runtimeInvariants.passed() && runtime.chainSummary().isValid(),
        runtimeInvariants.passed()
            ? "Runtime invariants and chain fork summary passed."
            : runtimeInvariants.reason()
    );

    if (directoryConfig != nullptr) {
        const NodeDataDirectoryReadResult manifestRead =
            NodeDataDirectory::loadManifest(*directoryConfig);

        bool persistenceSatisfied = false;
        std::string persistenceDetail;

        if (!manifestRead.loaded()) {
            persistenceDetail =
                "Runtime manifest could not be loaded: "
                + manifestRead.reason();
        } else {
            const ProtocolInvariantCheckResult manifestCheck =
                ProtocolInvariantChecker::checkRuntimeAgainstManifest(
                    runtime,
                    manifestRead.manifest()
                );

            persistenceSatisfied = manifestCheck.passed();
            persistenceDetail = manifestCheck.passed()
                ? "Storage manifest matches runtime chain, state and peer summaries."
                : manifestCheck.reason();
        }

        addRequirement(
            requirements,
            "persistent_chain_state",
            "Durable node directory exists and its manifest matches the runtime tip.",
            persistenceSatisfied,
            persistenceDetail
        );
    }

    try {
        const core::AccountStateView accountState =
            RuntimeAccountStateBuilder::accountStateViewAtTip(
                runtime.config().genesisConfig(),
                runtime.blockchain(),
                minimumFeeRawUnits(parameters)
            );

        const std::string stateRoot =
            core::StateRootCalculator::calculateAccountStateRoot(
                accountState
            );

        addRequirement(
            requirements,
            "verifiable_state_root",
            "Latest account state rebuilds to a deterministic non-empty root.",
            !stateRoot.empty(),
            !stateRoot.empty()
                ? "Latest state root rebuilt successfully."
                : "Latest state root is empty."
        );
    } catch (const std::exception& error) {
        addRequirement(
            requirements,
            "verifiable_state_root",
            "Latest account state rebuilds to a deterministic non-empty root.",
            false,
            error.what()
        );
    }

    try {
        const core::StateTransitionPreviewContext context =
            RuntimeAccountStateBuilder::previewContextAtTip(
                runtime.config().genesisConfig(),
                runtime.blockchain(),
                minimumFeeRawUnits(parameters)
            );

        addRequirement(
            requirements,
            "deterministic_state_transition",
            "Block execution has a deterministic state-transition context at the tip.",
            context.enforceAccountState(),
            context.enforceAccountState()
                ? "State-transition context enforces account state, nonce and fee rules."
                : "State-transition context does not enforce account state."
        );
    } catch (const std::exception& error) {
        addRequirement(
            requirements,
            "deterministic_state_transition",
            "Block execution has a deterministic state-transition context at the tip.",
            false,
            error.what()
        );
    }

    addRequirement(
        requirements,
        "economic_rules",
        "Network parameters define validator, quorum, fee, block and finality limits.",
        hasEconomicSecurityRules(parameters),
        hasEconomicSecurityRules(parameters)
            ? "Economic and consensus limits are present in network parameters."
            : "Network parameters are missing required economic or consensus limits."
    );

    const p2p::PeerMessage summaryMessage =
        runtime.localChainSummaryMessage(
            runtime.config().localPeer().peerId(),
            runtime.config().genesisConfig().genesisTimestamp()
        );

    addRequirement(
        requirements,
        "sync_protocol",
        "Runtime can emit a valid chain-summary message for peer synchronization.",
        summaryMessage.isValid(),
        summaryMessage.isValid()
            ? "Local chain summary message is valid."
            : "Local chain summary message is invalid."
    );

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            parameters.networkName()
        );

    addRequirement(
        requirements,
        "advanced_mempool_policy",
        "Mempool policy is structurally valid under transaction crypto policy.",
        cryptoContext.isValid() &&
        runtime.mempool().isValid(
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION
        ) && runtime.mempool().replaceByHigherFee(),
        cryptoContext.isValid() && runtime.mempool().replaceByHigherFee()
            ? "Mempool validates entries and allows higher-fee replacement."
            : "Mempool policy, crypto context or higher-fee replacement is invalid."
    );

    const p2p::InboundMessagePolicy inboundPolicy;
    const p2p::GossipMeshConfig gossipConfig(
        runtime.config().localPeer().peerId(),
        parameters.networkName(),
        parameters.chainId(),
        parameters.protocolVersion(),
        runtime.config().genesisConfig().deterministicId(),
        60,
        2
    );

    addRequirement(
        requirements,
        "p2p_identity_security",
        "P2P has bounded inbound-message validation and explicit handshake identity parameters.",
        inboundPolicy.isValid() && gossipConfig.isValid() &&
            runtime.config().localPeer().isValid(),
        inboundPolicy.isValid() && gossipConfig.isValid()
            ? "Inbound-message and handshake identity parameters are valid."
            : "Inbound-message policy or handshake identity parameters are invalid."
    );

    addRequirement(
        requirements,
        "complete_block_validation",
        "Runtime block chain passes structural and protocol invariant validation.",
        runtime.blockchain().isValid() && runtimeInvariants.passed(),
        runtime.blockchain().isValid() && runtimeInvariants.passed()
            ? "Blockchain and runtime invariants passed."
            : "Blockchain or runtime invariant validation failed."
    );

    return ProtocolCompletenessReport(std::move(requirements));
}

} // namespace nodo::node
