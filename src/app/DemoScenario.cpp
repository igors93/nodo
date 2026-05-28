#include "app/DemoScenario.hpp"

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/ChainStateRebuilder.hpp"
#include "core/LedgerRecord.hpp"
#include "core/State.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Time.hpp"

#include "privacy/PrivacyCommitment.hpp"

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
    using nodo::core::ChainStateRebuilder;
    using nodo::core::LedgerRecord;
    using nodo::core::State;
    using nodo::core::StateRebuildReport;
    using nodo::core::Transaction;
    using nodo::core::TransactionType;

    using nodo::economics::MintRecord;
    using nodo::economics::MintReason;

    using nodo::utils::Amount;
    using nodo::utils::currentUnixTimestamp;

    using nodo::privacy::PrivacyCommitment;
    using nodo::privacy::PrivacyCommitmentType;

    using nodo::crypto::CryptoAlgorithm;
    using nodo::crypto::CryptoPolicy;
    using nodo::crypto::PrivateKey;
    using nodo::crypto::PublicKey;
    using nodo::crypto::SecurityContext;
    using nodo::crypto::SignatureBundle;

    std::cout << "Nodo Blockchain - Transfer State Reconstruction\n";
    std::cout << "-----------------------------------------------\n\n";

    Blockchain blockchain;
    CryptoPolicy cryptoPolicy = CryptoPolicy::developmentPolicy();

    PublicKey igorPublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-development-public-key"
    );

    PrivateKey igorPrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-development-private-key"
    );

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

    blockchain.addGenesisBlock(genesisBlock);

    std::cout << "\nGenesis Block added to Blockchain.\n";
    std::cout << "Genesis Block index: " << genesisBlock.index() << "\n";
    std::cout << "Genesis Block hash: " << genesisBlock.hash() << "\n";
    std::cout << "Blockchain size: " << blockchain.size() << "\n";
    std::cout << "Blockchain validation: "
              << (blockchain.isValid() ? "VALID" : "INVALID")
              << "\n";

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

    State rebuiltMintOnlyState =
        ChainStateRebuilder::rebuildStateFromMintRecords(blockchain);

    std::cout << "\nMint-only State rebuilt from Blockchain.\n";
    std::cout << "Mint-only Igor balance: "
              << rebuiltMintOnlyState.balanceOf("igor").toString()
              << "\n";
    std::cout << "Mint-only Ana balance: "
              << rebuiltMintOnlyState.balanceOf("ana").toString()
              << "\n";

    State rebuiltFullState =
        ChainStateRebuilder::rebuildStateFromLedgerRecords(blockchain);

    std::cout << "\nFull State rebuilt from Blockchain.\n";
    std::cout << "Rebuilt total supply: "
              << rebuiltFullState.totalSupply().toString()
              << "\n";
    std::cout << "Rebuilt Igor balance: "
              << rebuiltFullState.balanceOf("igor").toString()
              << "\n";
    std::cout << "Rebuilt Ana balance: "
              << rebuiltFullState.balanceOf("ana").toString()
              << "\n";
    std::cout << "Rebuilt fee pool balance: "
          << rebuiltFullState.balanceOf(State::feePoolAddress()).toString()
          << "\n";
    std::cout << "Rebuilt Igor next nonce: "
            << rebuiltFullState.nextNonceOf("igor")
            << "\n";
    std::cout << "Rebuilt Ana next nonce: "
            << rebuiltFullState.nextNonceOf("ana")
            << "\n";
    std::cout << "Rebuilt supply audit: "
            << (rebuiltFullState.isSupplyAuditable() ? "VALID" : "INVALID")
            << "\n";

    /*
     * Privacy accounting foundation.
     *
     * This does not provide production privacy yet.
     * It introduces the architectural idea of representing private value
     * through commitments instead of directly exposing all accounting data.
     */
    PrivacyCommitment privateMintCommitment =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::MINT_COMMITMENT,
            "igor",
            Amount::fromNodo(1000),
            "development-blinding-secret-001",
            genesisMint.id(),
            currentUnixTimestamp()
        );

    std::cout << "\nPrivacy accounting foundation preview:\n";
    std::cout << "Private mint commitment validation: "
              << (privateMintCommitment.isValid() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Private mint commitment id: "
              << privateMintCommitment.id()
              << "\n";
    std::cout << "Private mint commitment owner hint: "
              << privateMintCommitment.ownerHint()
              << "\n";

    if (!rebuiltFullState.isSupplyAuditable()) {
        std::cerr << "Fatal: rebuilt State failed supply audit.\n";
        return 1;
    }

    char hashOutput[65] = {0};
    nodo_hash_string(genesisMint.serialize().c_str(), hashOutput, sizeof(hashOutput));

    std::cout << "\nGenesis MintRecord hash preview:\n";
    std::cout << hashOutput << "\n";

    std::cout << "\nBlockchain preview:\n";
    std::cout << blockchain.serialize() << "\n";

    std::cout << "\nNodo transfer state reconstruction executed successfully.\n";

    return 0;
}

} // namespace nodo::app