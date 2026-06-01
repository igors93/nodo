#include "node/ProtocolInvariantChecker.hpp"

#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "node/RuntimeStateVerifier.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::consensus::SlashingEvidenceRecord;
using nodo::consensus::SlashingEvidenceSeverity;
using nodo::consensus::SlashingEvidenceType;
using nodo::consensus::ValidatorPenaltyLedger;
using nodo::consensus::ValidatorPenaltyPolicy;
using nodo::crypto::KeyPair;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::NodeRuntimeManifest;
using nodo::node::ProtocolInvariantChecker;
using nodo::p2p::PeerInfo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

KeyPair validatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair(
        "protocol-invariant-checker-validator"
    );
}

BootstrapValidatorConfig validator() {
    return BootstrapValidatorConfig(
        validatorKeyPair().publicKey(),
        1,
        1,
        "protocol-invariant-checker-validator"
    );
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            validator()
        },
        "protocol-invariant-checker-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "protocol-invariant-checker-peer",
        "127.0.0.1:9700",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

NodeRuntime startRuntime() {
    const auto start =
        NodeRuntimeFactory::startFromGenesis(
            NodeRuntimeConfig(
                genesisConfig(),
                localPeer(),
                16
            )
        );

    requireCondition(
        start.started(),
        "Runtime should start."
    );

    return start.runtime();
}

std::string hash64(char value) {
    return std::string(64, value);
}

SlashingEvidenceRecord evidence() {
    return SlashingEvidenceRecord(
        hash64('a'),
        SlashingEvidenceType::DOUBLE_VOTE,
        "validator-alpha",
        hash64('b'),
        SlashingEvidenceSeverity::SLASHABLE,
        kTimestamp + 10
    );
}

void testRuntimeInvariantsPassForGenesisRuntime() {
    const NodeRuntime runtime =
        startRuntime();

    const auto result =
        ProtocolInvariantChecker::checkRuntime(runtime);

    requireCondition(
        result.passed() &&
        result.checkedInvariantCount() > 0,
        "Genesis runtime should pass protocol invariant audit."
    );
}

void testRuntimeManifestMismatchFailsInvariantAudit() {
    const NodeRuntime runtime =
        startRuntime();

    const NodeRuntimeManifest manifest =
        NodeRuntimeManifest::fromRuntime(
            runtime,
            kTimestamp + 20,
            kTimestamp + 20
        );

    const NodeRuntimeManifest tamperedManifest(
        manifest.chainId(),
        manifest.networkName(),
        manifest.protocolVersion(),
        manifest.genesisConfigId(),
        manifest.latestBlockHeight() + 1,
        manifest.latestBlockHash(),
        manifest.latestStateRoot(),
        manifest.validatorCount(),
        manifest.peerCount(),
        manifest.createdAt(),
        manifest.updatedAt()
    );

    const auto result =
        ProtocolInvariantChecker::checkRuntimeAgainstManifest(
            runtime,
            tamperedManifest
        );

    requireCondition(
        !result.passed() &&
        result.reason().find("latestBlockHeight") != std::string::npos,
        "Manifest height regression should fail protocol invariant audit."
    );
}

void testPenaltyLedgerInvariantKeepsDuplicateDecisionIdempotent() {
    ValidatorPenaltyLedger ledger;
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeTestnetPolicy();
    const SlashingEvidenceRecord record =
        evidence();

    const auto first =
        ledger.applyEvidence(
            record,
            policy,
            kTimestamp + 30
        );

    requireCondition(
        first.applied(),
        "First slashable evidence should apply one penalty decision."
    );

    const auto duplicate =
        ledger.applyEvidence(
            record,
            policy,
            kTimestamp + 31
        );

    requireCondition(
        duplicate.duplicate() &&
        ledger.size() == 1,
        "Duplicate slashable evidence should be idempotent."
    );

    const auto result =
        ProtocolInvariantChecker::checkPenaltyLedger(ledger);

    requireCondition(
        result.passed(),
        "Penalty ledger should pass protocol invariant audit after duplicate evidence."
    );
}

} // namespace

int main() {
    try {
        testRuntimeInvariantsPassForGenesisRuntime();
        testRuntimeManifestMismatchFailsInvariantAudit();
        testPenaltyLedgerInvariantKeepsDuplicateDecisionIdempotent();

        std::cout << "Nodo protocol invariant checker tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protocol invariant checker tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
