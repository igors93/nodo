#include "core/TransactionExecutionRouter.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/TransactionTypePolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class RecordingDomainExecutor final : public core::TransactionDomainExecutor {
public:
    bool reject = false;
    std::string lastHandler;

    RecordingDomainExecutor() {
        m_domains = {
            {"burns", "burns"}, {"governance", "governance"},
            {"slashing", "slashing"}, {"staking", "staking"},
            {"supply", "supply"}, {"validators", "validators"}
        };
    }

#define HANDLER(name, label) \
    core::TransactionDomainExecutionResult name( \
        const core::Transaction&, const core::AccountStateView& accounts, \
        std::uint64_t, std::int64_t) override { return record(label, accounts); }

    HANDLER(applyBurn, "BURN")
    HANDLER(applyStakeDeposit, "STAKE_DEPOSIT")
    HANDLER(applyStakeWithdraw, "STAKE_WITHDRAW")
    HANDLER(applyStakeTopUp, "STAKE_TOP_UP")
    HANDLER(applyValidatorRegister, "VALIDATOR_REGISTER")
    HANDLER(applyValidatorExitRequest, "VALIDATOR_EXIT_REQUEST")
    HANDLER(applyValidatorUnjailRequest, "VALIDATOR_UNJAIL_REQUEST")
    HANDLER(applyGovernanceProposal, "GOVERNANCE_PROPOSE")
    HANDLER(applyGovernanceVote, "GOVERNANCE_VOTE")
#undef HANDLER

    core::TransactionDomainExecutionResult finalizeBlock(
        const core::AccountStateView& accounts, utils::Amount,
        const std::vector<core::LedgerRecord>&, std::uint64_t, std::int64_t
    ) override {
        return core::TransactionDomainExecutionResult::accepted(accounts, m_domains);
    }

    const std::map<std::string, std::string>& domains() const override { return m_domains; }

private:
    std::map<std::string, std::string> m_domains;

    core::TransactionDomainExecutionResult record(
        const std::string& handler,
        const core::AccountStateView& accounts
    ) {
        lastHandler = handler;
        return reject
            ? core::TransactionDomainExecutionResult::rejected("REJECTED", "domain rejected")
            : core::TransactionDomainExecutionResult::accepted(accounts, m_domains);
    }
};

core::Transaction transactionFor(core::TransactionType type) {
    const auto& policy = core::TransactionTypePolicyRegistry::policyFor(type);
    const utils::Amount amount = policy.amountRule == core::TransactionAmountRule::ZERO
        ? utils::Amount()
        : utils::Amount::fromRawUnits(100);
    std::string target = "target-1";
    if (type == core::TransactionType::TRANSFER) target = "recipient-1";
    if (type == core::TransactionType::BURN) target = "nodo_burn";
    if (type == core::TransactionType::GOVERNANCE_PROPOSE) target = "nodo_governance";
    const std::string data = policy.payloadRequired ? "canonical-payload" : "";
    return data.empty()
        ? core::Transaction(type, "sender-1", target, amount,
              utils::Amount::fromRawUnits(5), 1, 1900000000)
        : core::Transaction(type, "sender-1", target, amount,
              utils::Amount::fromRawUnits(5), 1, 1900000000, data);
}

core::AccountStateView initialAccounts() {
    core::AccountStateView accounts;
    require(accounts.putAccount(core::AccountState(
        "sender-1", utils::Amount::fromRawUnits(1'000'000'000), 0)),
        "sender fixture must be valid");
    return accounts;
}

void testEveryRegisteredTypeRoutesAndProducesTypedReceipt() {
    require(
        core::TransactionTypePolicyRegistry::policies().size() ==
            static_cast<std::size_t>(core::TransactionType::COUNT),
        "Every enum value must have exactly one registered policy."
    );

    for (const auto type : core::TransactionTypePolicyRegistry::allTypes()) {
        RecordingDomainExecutor domain;
        const core::Transaction tx = transactionFor(type);
        const core::AccountStateView before = initialAccounts();
        const core::TransactionExecutionResult result =
            core::TransactionExecutionRouter::execute(
                tx,
                core::TransactionExecutionContext(
                    before, 100, 1900000100, true, false, true,
                    "", nullptr, &domain
                )
            );
        require(result.success(), "Registered transaction type must route successfully: " +
            core::transactionTypeToString(type) + " / " + result.reason());
        require(result.receipt().isValid(), "Router must produce a valid receipt.");
        require(result.receipt().transactionType() == type,
            "Receipt must preserve transaction type.");
        require(before.accountOrDefault("sender-1").nonce() == 0,
            "Router must not mutate its input account view.");

        const auto& policy = core::TransactionTypePolicyRegistry::policyFor(type);
        if (policy.handler != core::TransactionHandler::TRANSFER) {
            require(domain.lastHandler == policy.name,
                "Central router dispatched the wrong domain handler.");
        }
    }
}

void testDebitAndCreditRulesAreCentralized() {
    RecordingDomainExecutor domain;
    const auto before = initialAccounts();

    const auto transfer = core::TransactionExecutionRouter::execute(
        transactionFor(core::TransactionType::TRANSFER),
        core::TransactionExecutionContext(before, 1, 1900000000, true, false, true,
            "", nullptr, &domain));
    require(transfer.accounts().accountOrDefault("sender-1").balance().rawUnits() ==
        1'000'000'000 - 105, "Transfer must debit amount plus fee.");

    const auto withdraw = core::TransactionExecutionRouter::execute(
        transactionFor(core::TransactionType::STAKE_WITHDRAW),
        core::TransactionExecutionContext(before, 100, 1900000000, true, false, true,
            "", nullptr, &domain));
    require(withdraw.accounts().accountOrDefault("sender-1").balance().rawUnits() ==
        1'000'000'000 + 95, "Stake withdrawal must credit amount and debit only fee.");
}

void testDomainRejectionIsAtomic() {
    RecordingDomainExecutor domain;
    domain.reject = true;
    const auto before = initialAccounts();
    const auto result = core::TransactionExecutionRouter::execute(
        transactionFor(core::TransactionType::BURN),
        core::TransactionExecutionContext(before, 1, 1900000000, true, false, true,
            "", nullptr, &domain));
    require(!result.success(), "Rejected domain operation must fail execution.");
    require(before.accountOrDefault("sender-1").balance().rawUnits() == 1'000'000'000 &&
            before.accountOrDefault("sender-1").nonce() == 0,
        "Rejected execution must not mutate caller state.");
}

void testProtocolOnlyConceptsAreNotTransactionTypes() {
    for (const std::string oldType : {"MINT_REWARD", "PENALTY", "VALIDATOR_VOTE",
                                      "LOCK_SECURITY", "UNLOCK_SECURITY"}) {
        bool rejected = false;
        try { (void)core::transactionTypeFromString(oldType); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, oldType + " must not remain a user transaction type.");
    }
}

void testEveryAdmittedTypeHasABlockPreviewRoute() {
    const crypto::KeyPair key =
        crypto::KeyPair::createDeterministicEd25519KeyPair("router-preview-key");
    const crypto::Ed25519SignatureProvider provider;
    std::vector<core::LedgerRecord> records;
    for (const auto type : core::TransactionTypePolicyRegistry::allTypes()) {
        const auto& policy = core::TransactionTypePolicyRegistry::policyFor(type);
        const utils::Amount amount = policy.amountRule == core::TransactionAmountRule::ZERO
            ? utils::Amount() : utils::Amount::fromRawUnits(100);
        std::string target = type == core::TransactionType::TRANSFER
            ? "preview-recipient" : "preview-target";
        if (type == core::TransactionType::BURN) target = "nodo_burn";
        if (type == core::TransactionType::GOVERNANCE_PROPOSE) target = "nodo_governance";
        const std::string data = policy.payloadRequired ? "preview-payload" : "";
        core::Transaction transaction = data.empty()
            ? core::Transaction(type, key.address().value(), target, amount,
                  utils::Amount::fromRawUnits(1), 1, 1900000000)
            : core::Transaction(type, key.address().value(), target, amount,
                  utils::Amount::fromRawUnits(1), 1, 1900000000, data);
        transaction.attachSignatureBundle(crypto::SignatureBundle::createSignature(
            transaction.signingPayload(), key.publicKey(), key.privateKeyForSigningOnly(),
            1900000000, provider, crypto::SigningDomain::USER_TRANSACTION));
        records.push_back(core::LedgerRecord::fromTransaction(
            transaction, crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION, 1900000010));
    }
    const core::StateTransitionPreviewResult preview =
        core::StateTransitionPreview::previewBlock(
            core::Block(1, "previous-hash", records, 1900000010), 0);
    require(preview.accepted() &&
            preview.processedTransactionCount() == records.size(),
        "Every admitted user type must have a deterministic preview route: " + preview.reason());
}

} // namespace

int main() {
    try {
        testEveryRegisteredTypeRoutesAndProducesTypedReceipt();
        testDebitAndCreditRulesAreCentralized();
        testDomainRejectionIsAtomic();
        testProtocolOnlyConceptsAreNotTransactionTypes();
        testEveryAdmittedTypeHasABlockPreviewRoute();
        std::cout << "Transaction execution router tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Transaction execution router tests failed: " << error.what() << '\n';
        return 1;
    }
}
