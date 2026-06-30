#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionEngine.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;
constexpr const char* kChainId = "engine-test-chain";
constexpr const char* kNetworkName = "localnet";

void requireCondition(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class NoopProtocolDomainExecutor final : public core::TransactionDomainExecutor {
public:
    core::TransactionDomainExecutionResult applyBurn(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeDeposit(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeUnlock(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeWithdraw(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeTopUp(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyValidatorRegister(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyValidatorExitRequest(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyValidatorUnjailRequest(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyGovernanceProposal(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyGovernanceVote(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult finalizeBlock(
        const core::AccountStateView& accounts,
        utils::Amount,
        const std::vector<core::LedgerRecord>&,
        std::uint64_t,
        std::int64_t
    ) override { return accepted(accounts); }

    const std::map<std::string, std::string>& domains() const override {
        return m_domains;
    }

private:
    std::map<std::string, std::string> m_domains{{"engine_test_domain", "v1"}};

    core::TransactionDomainExecutionResult accepted(
        const core::AccountStateView& accounts
    ) {
        return core::TransactionDomainExecutionResult::accepted(accounts, m_domains);
    }
};

crypto::KeyPair protocolKeyPair() {
    return crypto::KeyPair::createDeterministicEd25519KeyPair(
        "state-transition-engine-key"
    );
}

std::string senderAddress() {
    return crypto::AddressDerivation::deriveFromPublicKey(
        protocolKeyPair().publicKey()
    ).value();
}

core::Transaction signedProtocolTransaction() {
    const crypto::KeyPair keyPair = protocolKeyPair();
    const crypto::Ed25519SignatureProvider provider;

    core::Transaction tx(
        core::TransactionType::TRANSFER,
        senderAddress(),
        "engine-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(1),
        1,
        kTimestamp
    );
    tx.withChainId(kChainId);
    tx.attachSignatureBundle(
        crypto::SignatureBundle::createSignature(
            tx.signingPayload(),
            keyPair.publicKey(),
            keyPair.privateKeyForSigningOnly(),
            kTimestamp,
            provider,
            crypto::SigningDomain::USER_TRANSACTION
        )
    );
    return tx;
}

core::LedgerRecord ledgerRecord() {
    return core::LedgerRecord::fromTransaction(
        signedProtocolTransaction(),
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

core::AccountStateView accountView() {
    core::AccountStateView view;
    if (!view.putAccount(core::AccountState(
            senderAddress(),
            utils::Amount::fromRawUnits(1000),
            0
        ))) {
        throw std::runtime_error("Failed to create engine test account.");
    }
    return view;
}

core::StateTransitionPreviewContext authoritativeContext() {
    return core::StateTransitionPreviewContext(
        1,
        accountView(),
        false,
        true,
        "",
        0,
        kChainId,
        kNetworkName,
        {},
        {},
        []() {
            return std::make_unique<NoopProtocolDomainExecutor>();
        },
        true
    );
}

core::Block testBlock() {
    return core::Block(1, "previous-hash", {ledgerRecord()}, kTimestamp + 1, "", "");
}

void testEngineRejectsStructuralContext() {
    const core::StateTransitionPreviewContext context =
        core::StateTransitionPreviewContext::structuralOnly(1);

    const core::StateTransitionPreviewResult result =
        core::StateTransitionEngine::executeBlock(testBlock(), context);

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_CONTEXT &&
        result.reason().find("Authoritative protocol execution") != std::string::npos,
        "StateTransitionEngine must reject structural-only contexts."
    );
}

void testEngineRejectsLegacyDomainTransitionFallback() {
    const core::StateTransitionPreviewContext context(
        1,
        accountView(),
        false,
        true,
        "",
        0,
        kChainId,
        kNetworkName,
        {},
        [](const core::AccountStateView& accounts,
           utils::Amount,
           const std::vector<core::Transaction>&,
           const std::vector<core::LedgerRecord>&,
           std::int64_t) {
            return core::DeterministicStateTransitionResult::accepted(
                accounts,
                {{"legacy_domain", "fallback"}}
            );
        }
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionEngine::executeBlock(testBlock(), context);

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_CONTEXT &&
        result.reason().find("protocol-domain executor") != std::string::npos,
        "StateTransitionEngine must reject legacy state-domain transition fallback contexts."
    );
}

void testPreviewStillSupportsNonAuthoritativeSimulation() {
    const core::StateTransitionPreviewContext context =
        core::StateTransitionPreviewContext::structuralOnly(1);

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(testBlock(), context);

    requireCondition(
        result.accepted(),
        "StateTransitionPreview should remain available for non-authoritative simulation."
    );
}

void testEngineAcceptsAuthoritativeContext() {
    const core::StateTransitionPreviewResult result =
        core::StateTransitionEngine::executeBlock(
            testBlock(),
            authoritativeContext()
        );

    requireCondition(
        result.accepted() &&
        !result.stateRoot().empty() &&
        !result.receiptsRoot().empty(),
        "StateTransitionEngine must accept a complete authoritative protocol context."
    );
}

} // namespace

int main() {
    try {
        testEngineRejectsStructuralContext();
        testEngineRejectsLegacyDomainTransitionFallback();
        testPreviewStillSupportsNonAuthoritativeSimulation();
        testEngineAcceptsAuthoritativeContext();
        std::cout << "Nodo state transition engine tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo state transition engine tests failed: "
                  << error.what() << "\n";
        return 1;
    }
}
