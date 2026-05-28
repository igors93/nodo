#include "app/DemoScenario.hpp"

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/State.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "core/ChainStateRebuilder.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Time.hpp"

#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/hash.h"

#include <iostream>
#include <string>
#include <vector>

namespace nodo::app {

int runBlockchainFoundationDemo() {
    using nodo::core::Block;
    using nodo::core::Blockchain;
    using nodo::core::LedgerRecord;
    using nodo::core::State;
    using nodo::core::Transaction;
    using nodo::core::TransactionType;
    using nodo::core::ChainStateRebuilder;
    using nodo::core::StateRebuildReport;

    using nodo::economics::MintRecord;
    using nodo::economics::MintReason;

    using nodo::utils::Amount;
    using nodo::utils::currentUnixTimestamp;

    using nodo::crypto::CryptoAlgorithm;
    using nodo::crypto::CryptoPolicy;
    using nodo::crypto::PrivateKey;
    using nodo::crypto::PublicKey;
    using nodo::crypto::SecurityContext;
    using nodo::crypto::SignatureBundle;

    std::cout << "Nodo Blockchain - Blockchain Foundation\n";
    std::cout << "---------------------------------------\n\n";

    State state;
    Blockchain blockchain;
    CryptoPolicy cryptoPolicy = CryptoPolicy::developmentPolicy();

    /*
     * Development keys.
     *
     * Warning:
     * These keys are not secure. They exist only to validate the architecture.
     */
    PublicKey igorPublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-development-public-key"
    );

    PrivateKey igorPrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-development-private-key"
    );

    /*
     * Simplified genesis mint.
     *
     * Rule:
     * Even genesis coins must have an auditable MintRecord.
     */
    MintRecord genesisMint(
        "mint_genesis_igor_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        currentUnixTimestamp()
    );

    SignatureBundle genesisSignatureBundle =
        SignatureBundle::createDevelopmentSignature(
            genesisMint.serialize(),
            igorPublicKey,
            igorPrivateKey,
            currentUnixTimestamp()
        );

    const bool genesisSignaturePolicyValid =
        genesisSignatureBundle.isValidForPolicy(
            cryptoPolicy,
            SecurityContext::DEVELOPMENT_ONLY
        );

    std::cout << "Genesis signature policy check: "
              << (genesisSignaturePolicyValid ? "VALID" : "INVALID")
              << "\n";

    if (!genesisSignaturePolicyValid) {
        std::cerr << "Fatal: genesis signature rejected by crypto policy.\n";
        return 1;
    }

    LedgerRecord genesisLedgerRecord =
        LedgerRecord::fromMintRecord(
            genesisMint,
            currentUnixTimestamp()
        );

    if (!genesisLedgerRecord.isValid()) {
        std::cerr << "Fatal: invalid genesis LedgerRecord.\n";
        return 1;
    }

    Block genesisBlock =
        Block::createGenesisBlock(
            std::vector<LedgerRecord>{genesisLedgerRecord},
            currentUnixTimestamp()
        );

    if (!genesisBlock.isValid()) {
        std::cerr << "Fatal: invalid genesis block.\n";
        return 1;
    }

    /*
     * Add genesis block through Blockchain validation rules.
     */
    blockchain.addGenesisBlock(genesisBlock);

    std::cout << "\nGenesis Block added to Blockchain.\n";
    std::cout << "Genesis Block index: " << genesisBlock.index() << "\n";
    std::cout << "Genesis Block hash: " << genesisBlock.hash() << "\n";
    std::cout << "Blockchain size: " << blockchain.size() << "\n";
    std::cout << "Blockchain validation: "
              << (blockchain.isValid() ? "VALID" : "INVALID")
              << "\n";

    state.applyMintRecord(genesisMint);

    std::cout << "\nGenesis mint applied.\n";
    std::cout << "Total supply: " << state.totalSupply().toString() << "\n";
    std::cout << "Igor balance: " << state.balanceOf("igor").toString() << "\n";

    /*
     * This still happens directly through State for now.
     * In a later phase, this will become a signed transaction.
     */
    state.lockCoinLotForSecurity(
        "coinlot_from_mint_genesis_igor_001",
        500
    );

    std::cout << "\nCoinLot locked for network security.\n";
    std::cout << "Current block: " << state.currentBlockIndex() << "\n";
    std::cout << "Total security weight: " << state.totalSecurityWeight() << "\n";

    std::cout << "\nSupply audit: "
              << (state.isSupplyAuditable() ? "VALID" : "INVALID")
              << "\n";

    /*
     * Create a signed transfer transaction.
     *
     * This transaction is not applied to State yet.
     * It only proves that Nodo can create a signed, policy-checked transaction.
     */
    Transaction transferTransaction(
        TransactionType::TRANSFER,
        "igor",
        "ana",
        Amount::fromNodo(25),
        Amount::fromRawUnits(100000),
        1,
        currentUnixTimestamp()
    );

    SignatureBundle transferSignatureBundle =
        SignatureBundle::createDevelopmentSignature(
            transferTransaction.signingPayload(),
            igorPublicKey,
            igorPrivateKey,
            currentUnixTimestamp()
        );

    transferTransaction.attachSignatureBundle(transferSignatureBundle);

    const bool transferValid =
        transferTransaction.isStructurallyValid(
            cryptoPolicy,
            SecurityContext::USER_TRANSACTION
        );

    std::cout << "\nSigned transfer transaction created.\n";
    std::cout << "Transaction id: " << transferTransaction.id() << "\n";
    std::cout << "Transaction validation: "
              << (transferValid ? "VALID" : "INVALID")
              << "\n";

    if (!transferValid) {
        std::cerr << "Fatal: invalid transfer transaction.\n";
        return 1;
    }

    LedgerRecord transferLedgerRecord =
        LedgerRecord::fromTransaction(
            transferTransaction,
            cryptoPolicy,
            SecurityContext::USER_TRANSACTION,
            currentUnixTimestamp()
        );

    if (!transferLedgerRecord.isValid()) {
        std::cerr << "Fatal: invalid transfer LedgerRecord.\n";
        return 1;
    }

    Block transferBlock(
        1,
        blockchain.latestBlock().hash(),
        std::vector<LedgerRecord>{transferLedgerRecord},
        currentUnixTimestamp()
    );

    std::cout << "\nTransfer Block created.\n";
    std::cout << "Transfer Block index: " << transferBlock.index() << "\n";
    std::cout << "Transfer Block previous hash: "
              << transferBlock.previousHash()
              << "\n";
    std::cout << "Transfer Block hash: " << transferBlock.hash() << "\n";
    std::cout << "Transfer Block validation: "
              << (transferBlock.isValid() ? "VALID" : "INVALID")
              << "\n";

    if (!transferBlock.isValid()) {
        std::cerr << "Fatal: invalid transfer block.\n";
        return 1;
    }

    blockchain.addBlock(transferBlock);

    std::cout << "\nTransfer Block added to Blockchain.\n";
    std::cout << "Blockchain size: " << blockchain.size() << "\n";
    std::cout << "Latest Block hash: " << blockchain.latestBlock().hash() << "\n";
    std::cout << "Full Blockchain validation: "
              << (blockchain.isValid() ? "VALID" : "INVALID")
              << "\n";

    char hashOutput[65] = {0};
    nodo_hash_string(genesisMint.serialize().c_str(), hashOutput, sizeof(hashOutput));

    std::cout << "\nGenesis MintRecord hash preview:\n";
    std::cout << hashOutput << "\n";

    /*
     * New phase:
     * Audit the Blockchain by scanning it from genesis to latest block.
     *
     * This is the first step toward rebuilding State from chain history.
     */
    StateRebuildReport rebuildReport =
        ChainStateRebuilder::auditBlockchain(blockchain);

    std::cout << "\nChain rebuild audit report:\n";
    std::cout << rebuildReport.serialize() << "\n";

    std::cout << "Chain rebuild audit result: "
              << (rebuildReport.success() ? "VALID" : "INVALID")
              << "\n";

    if (!rebuildReport.success()) {
        std::cerr << "Fatal: chain rebuild audit failed: "
                  << rebuildReport.failureReason()
                  << "\n";
        return 1;
    }

    std::cout << "\nBlockchain preview:\n";
    std::cout << blockchain.serialize() << "\n";

    std::cout << "\nNodo ChainStateRebuilder foundation executed successfully.\n";

    return 0;
}

} // namespace nodo::app