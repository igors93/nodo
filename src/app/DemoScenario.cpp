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
#include "serialization/BlockCodec.hpp"
#include "serialization/LedgerRecordCodec.hpp"

#include "privacy/NullifierSet.hpp"
#include "privacy/PrivacyCommitment.hpp"
#include "privacy/PrivacyNullifier.hpp"
#include "privacy/PrivateAccountingLedger.hpp"
#include "privacy/PrivateAccountingLedgerRebuilder.hpp"
#include "privacy/PrivateAccountingRecord.hpp"

#include "storage/BlockFileStore.hpp"
#include "storage/BlockchainLoader.hpp"
#include "storage/BlockchainStorageReader.hpp"
#include "storage/BlockSnapshotHeader.hpp"
#include "storage/BlockStorageIndex.hpp"
#include "storage/ChainManifest.hpp"

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

    using nodo::serialization::BlockCodec;
    using nodo::serialization::LedgerRecordCodec;

    using nodo::economics::MintRecord;
    using nodo::economics::MintReason;

    using nodo::storage::BlockFileStore;
    using nodo::storage::BlockchainLoader;
    using nodo::storage::BlockchainLoadReport;
    using nodo::storage::BlockchainStorageReader;
    using nodo::storage::BlockchainStorageReadReport;
    using nodo::storage::StoredBlockSnapshot;
    using nodo::storage::BlockSnapshotHeader;
    using nodo::storage::BlockStorageIndex;
    using nodo::storage::ChainManifest;

    using nodo::utils::Amount;
    using nodo::utils::currentUnixTimestamp;

    using nodo::privacy::NullifierSet;
    using nodo::privacy::PrivacyCommitment;
    using nodo::privacy::PrivacyCommitmentType;
    using nodo::privacy::PrivacyNullifier;
    using nodo::privacy::PrivacyNullifierType;
    using nodo::privacy::PrivateAccountingLedger;
    using nodo::privacy::PrivateAccountingLedgerRebuilder;
    using nodo::privacy::PrivateAccountingLedgerRebuildReport;
    using nodo::privacy::PrivateAccountingRecord;

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

    blockchain.addGenesisBlock(genesisBlock);

    std::cout << "\nGenesis Block added to Blockchain.\n";
    std::cout << "Genesis Block index: " << genesisBlock.index() << "\n";
    std::cout << "Genesis Block hash: " << genesisBlock.hash() << "\n";
    std::cout << "Blockchain size: " << blockchain.size() << "\n";
    std::cout << "Blockchain validation: "
              << (blockchain.isValid() ? "VALID" : "INVALID")
              << "\n";

    /*
     * Create a signed transfer transaction.
     *
     * This transaction proves that Nodo can create a signed,
     * policy-checked public transfer and later replay it from chain history.
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

    /*
     * First audit of the public chain before private records are anchored.
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

    if (!rebuiltFullState.isSupplyAuditable()) {
        std::cerr << "Fatal: rebuilt State failed supply audit.\n";
        return 1;
    }

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

    /*
     * Nullifier foundation.
     *
     * In future private transfers, the network should not need to know
     * which private coin was spent. However, it must know that the same
     * private coin was not spent twice.
     */
    PrivacyNullifier privateSpendNullifier =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            privateMintCommitment.id(),
            "development-owner-secret-igor-001",
            "demo-private-spend-context-001",
            currentUnixTimestamp()
        );

    NullifierSet nullifierSet;
    nullifierSet.registerNullifier(privateSpendNullifier);

    const bool duplicateNullifierRejected =
        !nullifierSet.canRegisterNullifier(privateSpendNullifier);

    std::cout << "\nPrivacy nullifier foundation preview:\n";
    std::cout << "Private spend nullifier validation: "
              << (privateSpendNullifier.isValid() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Private spend nullifier id: "
              << privateSpendNullifier.id()
              << "\n";
    std::cout << "Nullifier set size: "
              << nullifierSet.size()
              << "\n";
    std::cout << "Duplicate nullifier protection: "
              << (duplicateNullifierRejected ? "VALID" : "INVALID")
              << "\n";

    if (!duplicateNullifierRejected) {
        std::cerr << "Fatal: duplicate nullifier protection failed.\n";
        return 1;
    }

    /*
     * Private accounting record foundation.
     *
     * A private transfer should consume public nullifiers and create new
     * private commitments while keeping public supply unchanged.
     */
    PrivacyCommitment privateTransferOutputToAna =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT,
            "ana",
            Amount::fromNodo(25),
            "development-blinding-secret-ana-output-001",
            privateSpendNullifier.id(),
            currentUnixTimestamp()
        );

    PrivacyCommitment privateTransferChangeToIgor =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT,
            "igor",
            Amount::fromRawUnits(97499900000),
            "development-blinding-secret-igor-change-001",
            privateSpendNullifier.id(),
            currentUnixTimestamp()
        );

    PrivateAccountingRecord privateTransferRecord =
        PrivateAccountingRecord::createPrivateTransferRecord(
            std::vector<PrivacyNullifier>{privateSpendNullifier},
            std::vector<PrivacyCommitment>{
                privateTransferOutputToAna,
                privateTransferChangeToIgor
            },
            "demo-private-transfer-audit-reference-001",
            currentUnixTimestamp()
        );

    std::cout << "\nPrivate accounting record foundation preview:\n";
    std::cout << "Private transfer record validation: "
              << (privateTransferRecord.isValid() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Private transfer record id: "
              << privateTransferRecord.id()
              << "\n";
    std::cout << "Private transfer input nullifiers: "
              << privateTransferRecord.inputNullifiers().size()
              << "\n";
    std::cout << "Private transfer output commitments: "
              << privateTransferRecord.outputCommitments().size()
              << "\n";

    if (!privateTransferRecord.isValid()) {
        std::cerr << "Fatal: private transfer accounting record is invalid.\n";
        return 1;
    }

    /*
     * Private accounting ledger foundation.
     *
     * The ledger validates multiple private records together and prevents
     * repeated nullifiers or repeated output commitments.
     */
    PrivateAccountingRecord privateMintRecord =
        PrivateAccountingRecord::createPrivateMintRecord(
            std::vector<PrivacyCommitment>{privateMintCommitment},
            Amount::fromNodo(1000),
            "demo-private-mint-audit-reference-001",
            currentUnixTimestamp()
        );

    PrivateAccountingLedger privateAccountingLedger;
    privateAccountingLedger.addRecord(privateMintRecord);
    privateAccountingLedger.addRecord(privateTransferRecord);

    const bool duplicatePrivateRecordRejected =
        !privateAccountingLedger.canAppendRecord(privateTransferRecord);

    std::cout << "\nPrivate accounting ledger foundation preview:\n";
    std::cout << "Private ledger validation: "
              << (privateAccountingLedger.isValid() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Private ledger record count: "
              << privateAccountingLedger.size()
              << "\n";
    std::cout << "Private ledger nullifier count: "
              << privateAccountingLedger.nullifierSet().size()
              << "\n";
    std::cout << "Private ledger commitment count: "
              << privateAccountingLedger.registeredCommitmentCount()
              << "\n";
    std::cout << "Private ledger minted supply: "
              << privateAccountingLedger.privateMintedSupply().toString()
              << "\n";
    std::cout << "Private ledger burned supply: "
              << privateAccountingLedger.privateBurnedSupply().toString()
              << "\n";
    std::cout << "Private ledger outstanding supply: "
              << privateAccountingLedger.outstandingPrivateSupply().toString()
              << "\n";
    std::cout << "Duplicate private record protection: "
              << (duplicatePrivateRecordRejected ? "VALID" : "INVALID")
              << "\n";

    if (!privateAccountingLedger.isValid()) {
        std::cerr << "Fatal: private accounting ledger is invalid.\n";
        return 1;
    }

    if (!duplicatePrivateRecordRejected) {
        std::cerr << "Fatal: duplicate private record protection failed.\n";
        return 1;
    }

    /*
     * Private accounting records now enter the public blockchain as official
     * LedgerRecords.
     *
     * Both private mint and private transfer records must be anchored.
     * If only the private transfer is anchored, a node cannot rebuild the full
     * private accounting supply from blockchain history.
     */
    LedgerRecord privateMintLedgerRecord =
        LedgerRecord::fromPrivateAccountingRecord(
            privateMintRecord,
            currentUnixTimestamp()
        );

    LedgerRecord privateTransferLedgerRecord =
        LedgerRecord::fromPrivateAccountingRecord(
            privateTransferRecord,
            currentUnixTimestamp()
        );

    if (!privateMintLedgerRecord.isValid()) {
        std::cerr << "Fatal: invalid private mint LedgerRecord.\n";
        return 1;
    }

    if (!privateTransferLedgerRecord.isValid()) {
        std::cerr << "Fatal: invalid private transfer LedgerRecord.\n";
        return 1;
    }

    Block privateAccountingBlock(
        2,
        blockchain.latestBlock().hash(),
        std::vector<LedgerRecord>{
            privateMintLedgerRecord,
            privateTransferLedgerRecord
        },
        currentUnixTimestamp()
    );

    if (!privateAccountingBlock.isValid()) {
        std::cerr << "Fatal: invalid private accounting block.\n";
        return 1;
    }

    blockchain.addBlock(privateAccountingBlock);

    StateRebuildReport finalBlockchainAudit =
        ChainStateRebuilder::auditBlockchain(blockchain);

    std::cout << "\nPrivate accounting block added to Blockchain.\n";
    std::cout << "Private Accounting Block index: "
              << privateAccountingBlock.index()
              << "\n";
    std::cout << "Private Accounting Block hash: "
              << privateAccountingBlock.hash()
              << "\n";
    std::cout << "Private Accounting Block record count: "
              << privateAccountingBlock.records().size()
              << "\n";
    std::cout << "Blockchain size after private accounting: "
              << blockchain.size()
              << "\n";
    std::cout << "Final Blockchain audit report:\n";
    std::cout << finalBlockchainAudit.serialize() << "\n";
    std::cout << "Final Blockchain audit result: "
              << (finalBlockchainAudit.success() ? "VALID" : "INVALID")
              << "\n";

    if (!finalBlockchainAudit.success()) {
        std::cerr << "Fatal: final Blockchain audit failed: "
                  << finalBlockchainAudit.failureReason()
                  << "\n";
        return 1;
    }

    State rebuiltStateWithPrivateAccounting =
        ChainStateRebuilder::rebuildStateFromLedgerRecords(blockchain);

    std::cout << "\nPublic State rebuilt after private accounting block.\n";
    std::cout << "Rebuilt Igor balance after private block: "
              << rebuiltStateWithPrivateAccounting.balanceOf("igor").toString()
              << "\n";
    std::cout << "Rebuilt Ana balance after private block: "
              << rebuiltStateWithPrivateAccounting.balanceOf("ana").toString()
              << "\n";
    std::cout << "Rebuilt supply audit after private block: "
              << (rebuiltStateWithPrivateAccounting.isSupplyAuditable() ? "VALID" : "INVALID")
              << "\n";

    if (!rebuiltStateWithPrivateAccounting.isSupplyAuditable()) {
        std::cerr << "Fatal: rebuilt State after private block failed supply audit.\n";
        return 1;
    }

    /*
     * Rebuild the private accounting ledger directly from Blockchain history.
     */
    PrivateAccountingLedgerRebuildReport privateLedgerRebuildReport =
        PrivateAccountingLedgerRebuilder::auditBlockchain(blockchain);

    std::cout << "\nPrivate Accounting Ledger rebuilt from Blockchain.\n";
    std::cout << "Private ledger rebuild report:\n";
    std::cout << privateLedgerRebuildReport.serialize() << "\n";

    if (!privateLedgerRebuildReport.success()) {
        std::cerr << "Fatal: private ledger rebuild failed: "
                  << privateLedgerRebuildReport.failureReason()
                  << "\n";
        return 1;
    }

    PrivateAccountingLedger rebuiltPrivateLedger =
        PrivateAccountingLedgerRebuilder::rebuildFromBlockchain(blockchain);

    std::cout << "Rebuilt private ledger validation: "
              << (rebuiltPrivateLedger.isValid() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Rebuilt private ledger record count: "
              << rebuiltPrivateLedger.size()
              << "\n";
    std::cout << "Rebuilt private ledger nullifier count: "
              << rebuiltPrivateLedger.nullifierSet().size()
              << "\n";
    std::cout << "Rebuilt private ledger commitment count: "
              << rebuiltPrivateLedger.registeredCommitmentCount()
              << "\n";
    std::cout << "Rebuilt private ledger minted supply: "
              << rebuiltPrivateLedger.privateMintedSupply().toString()
              << "\n";
    std::cout << "Rebuilt private ledger burned supply: "
              << rebuiltPrivateLedger.privateBurnedSupply().toString()
              << "\n";
    std::cout << "Rebuilt private ledger outstanding supply: "
              << rebuiltPrivateLedger.outstandingPrivateSupply().toString()
              << "\n";

    if (!rebuiltPrivateLedger.isValid()) {
        std::cerr << "Fatal: rebuilt private ledger is invalid.\n";
        return 1;
    }

    /*
     * Storage foundation.
     *
     * The current storage layer writes deterministic block snapshots to disk.
     * Runtime reload now lives in the node data directory flow; this legacy
     * demo remains a broad component smoke test.
     */
    BlockFileStore blockFileStore("data");

    blockFileStore.clearBlockStorage();
    blockFileStore.writeBlockchain(blockchain);

    bool storedBlocksAreValid = true;

    for (const auto& block : blockchain.blocks()) {
        if (!blockFileStore.verifyStoredBlock(block)) {
            storedBlocksAreValid = false;
            break;
        }
    }

    std::cout << "\nBlock storage foundation preview:\n";
    std::cout << "Block storage directory: "
              << blockFileStore.blockDirectoryPath()
              << "\n";
    std::cout << "Stored block file count: "
              << blockFileStore.storedBlockFileCount()
              << "\n";
    std::cout << "Stored block verification: "
              << (storedBlocksAreValid ? "VALID" : "INVALID")
              << "\n";

    if (!storedBlocksAreValid) {
        std::cerr << "Fatal: stored block verification failed.\n";
        return 1;
    }

    /*
     * Chain manifest foundation.
     *
     * The manifest summarizes the persisted chain and gives future loading
     * code a small validated metadata file before reading full block snapshots.
     */
    ChainManifest chainManifest =
        ChainManifest::fromBlockchain(
            blockchain,
            currentUnixTimestamp()
        );

    chainManifest.writeToStorageRoot("data");

    ChainManifest loadedChainManifest =
        ChainManifest::readFromStorageRoot("data");

    const bool manifestIsValid =
        loadedChainManifest.isValid();

    const bool manifestMatchesBlockchain =
        loadedChainManifest.matchesBlockchain(blockchain);

    std::cout << "\nChain storage manifest preview:\n";
    std::cout << "Manifest validation: "
              << (manifestIsValid ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Manifest matches Blockchain: "
              << (manifestMatchesBlockchain ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Manifest block count: "
              << loadedChainManifest.blockCount()
              << "\n";
    std::cout << "Manifest genesis hash: "
              << loadedChainManifest.genesisHash()
              << "\n";
    std::cout << "Manifest latest hash: "
              << loadedChainManifest.latestHash()
              << "\n";
    std::cout << "Manifest hash: "
              << loadedChainManifest.manifestHash()
              << "\n";

    if (!manifestIsValid || !manifestMatchesBlockchain) {
        std::cerr << "Fatal: chain manifest verification failed.\n";
        return 1;
    }

    /*
     * Block storage index foundation.
     *
     * The index maps block heights and hashes to deterministic block snapshot
     * file names for the generic storage foundation.
     */
    BlockStorageIndex blockStorageIndex =
        BlockStorageIndex::fromBlockchainAndManifest(
            blockchain,
            loadedChainManifest,
            currentUnixTimestamp()
        );

    blockStorageIndex.writeToStorageRoot("data");

    BlockStorageIndex loadedBlockStorageIndex =
        BlockStorageIndex::readFromStorageRoot("data");

    const bool storageIndexIsValid =
        loadedBlockStorageIndex.isValid();

    const bool storageIndexMatchesChain =
        loadedBlockStorageIndex.matchesBlockchainAndManifest(
            blockchain,
            loadedChainManifest
        );

    std::cout << "\nBlock storage index preview:\n";
    std::cout << "Storage index validation: "
              << (storageIndexIsValid ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Storage index matches Blockchain: "
              << (storageIndexMatchesChain ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Storage index block count: "
              << loadedBlockStorageIndex.blockCount()
              << "\n";
    std::cout << "Storage index entry count: "
              << loadedBlockStorageIndex.entries().size()
              << "\n";
    std::cout << "Storage index manifest hash: "
              << loadedBlockStorageIndex.chainManifestHash()
              << "\n";
    std::cout << "Storage index hash: "
              << loadedBlockStorageIndex.indexHash()
              << "\n";

    if (!storageIndexIsValid || !storageIndexMatchesChain) {
        std::cerr << "Fatal: block storage index verification failed.\n";
        return 1;
    }

    /*
     * Blockchain storage reader foundation.
     *
     * This verifies storage metadata and block snapshot files before future
     * code attempts full Blockchain reconstruction from disk.
     */
    BlockchainStorageReadReport storageReadReport =
        BlockchainStorageReader::auditStorageRoot("data");

    std::vector<StoredBlockSnapshot> storedSnapshots =
        BlockchainStorageReader::readBlockSnapshots("data");

    std::cout << "\nBlockchain storage reader preview:\n";
    std::cout << "Storage reader audit: "
              << (storageReadReport.success() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Storage reader report:\n";
    std::cout << storageReadReport.serialize() << "\n";
    std::cout << "Stored snapshot count: "
              << storedSnapshots.size()
              << "\n";
    std::cout << "First stored snapshot hash: "
              << (storedSnapshots.empty() ? "" : storedSnapshots.front().contentHash())
              << "\n";
    std::cout << "Latest stored snapshot hash: "
              << (storedSnapshots.empty() ? "" : storedSnapshots.back().contentHash())
              << "\n";

    if (!storageReadReport.success()) {
        std::cerr << "Fatal: blockchain storage reader audit failed: "
                  << storageReadReport.failureReason()
                  << "\n";
        return 1;
    }

    /*
     * Block snapshot header parser foundation.
     *
     * This parses persisted block headers, validates their stored hashes, and
     * verifies previous-hash continuity without deserializing full blocks yet.
     */
    std::vector<BlockSnapshotHeader> parsedSnapshotHeaders;

    bool snapshotHeadersMatchStorageIndex = true;

    for (std::size_t i = 0; i < storedSnapshots.size(); ++i) {
        BlockSnapshotHeader parsedHeader =
            BlockSnapshotHeader::fromFile(storedSnapshots[i].filePath());

        if (parsedHeader.blockIndex() != storedSnapshots[i].blockIndex() ||
            parsedHeader.blockHash() != storedSnapshots[i].blockHash()) {
            snapshotHeadersMatchStorageIndex = false;
        }

        parsedSnapshotHeaders.push_back(parsedHeader);
    }

    const bool snapshotHeaderSequenceValid =
        BlockSnapshotHeader::validateHeaderSequence(parsedSnapshotHeaders);

    std::cout << "\nBlock snapshot header parser preview:\n";
    std::cout << "Snapshot header sequence: "
              << (snapshotHeaderSequenceValid ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Snapshot headers match storage index: "
              << (snapshotHeadersMatchStorageIndex ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Parsed snapshot header count: "
              << parsedSnapshotHeaders.size()
              << "\n";
    std::cout << "Genesis snapshot header hash: "
              << (parsedSnapshotHeaders.empty() ? "" : parsedSnapshotHeaders.front().blockHash())
              << "\n";
    std::cout << "Latest snapshot previous hash: "
              << (parsedSnapshotHeaders.empty() ? "" : parsedSnapshotHeaders.back().previousHash())
              << "\n";
    std::cout << "Latest snapshot header hash: "
              << (parsedSnapshotHeaders.empty() ? "" : parsedSnapshotHeaders.back().blockHash())
              << "\n";

    if (!snapshotHeaderSequenceValid || !snapshotHeadersMatchStorageIndex) {
        std::cerr << "Fatal: block snapshot header parser validation failed.\n";
        return 1;
    }

    /*
     * LedgerRecord deserialization foundation.
     *
     * This rebuilds LedgerRecords from persisted block snapshot headers and
     * verifies that their deterministic serialization matches the original
     * in-memory blockchain records.
     */
    std::size_t deserializedLedgerRecordCount = 0;
    bool deserializedLedgerRecordsMatchBlockchain = true;

    for (std::size_t blockPosition = 0;
         blockPosition < parsedSnapshotHeaders.size();
         ++blockPosition) {
        std::vector<LedgerRecord> deserializedRecords =
            LedgerRecordCodec::deserializeListFromBlockHeaderPayload(
                parsedSnapshotHeaders[blockPosition].headerPayload()
            );

        const auto& originalRecords =
            blockchain.blocks()[blockPosition].records();

        if (deserializedRecords.size() != originalRecords.size()) {
            deserializedLedgerRecordsMatchBlockchain = false;
            break;
        }

        for (std::size_t recordPosition = 0;
             recordPosition < deserializedRecords.size();
             ++recordPosition) {
            if (!deserializedRecords[recordPosition].isValid()) {
                deserializedLedgerRecordsMatchBlockchain = false;
                break;
            }

            if (deserializedRecords[recordPosition].serialize() !=
                originalRecords[recordPosition].serialize()) {
                deserializedLedgerRecordsMatchBlockchain = false;
                break;
            }
        }

        deserializedLedgerRecordCount += deserializedRecords.size();

        if (!deserializedLedgerRecordsMatchBlockchain) {
            break;
        }
    }

    std::cout << "\nLedgerRecord deserialization preview:\n";
    std::cout << "Deserialized LedgerRecord count: "
              << deserializedLedgerRecordCount
              << "\n";
    std::cout << "Deserialized LedgerRecords match Blockchain: "
              << (deserializedLedgerRecordsMatchBlockchain ? "VALID" : "INVALID")
              << "\n";

    if (!deserializedLedgerRecordsMatchBlockchain) {
        std::cerr << "Fatal: LedgerRecord deserialization validation failed.\n";
        return 1;
    }

    /*
     * Block snapshot deserialization foundation.
     *
     * This rebuilds full Block objects from stored block snapshot files and
     * verifies that their deterministic serialization matches the original
     * in-memory blockchain.
     */
    std::vector<core::Block> deserializedBlocks;
    bool deserializedBlocksMatchBlockchain = true;

    for (std::size_t blockPosition = 0;
         blockPosition < storedSnapshots.size();
         ++blockPosition) {
        core::Block deserializedBlock =
            BlockCodec::deserializeFromFile(
                storedSnapshots[blockPosition].filePath()
            );

        if (!deserializedBlock.isValid()) {
            deserializedBlocksMatchBlockchain = false;
            break;
        }

        if (deserializedBlock.serialize() !=
            blockchain.blocks()[blockPosition].serialize()) {
            deserializedBlocksMatchBlockchain = false;
            break;
        }

        deserializedBlocks.push_back(deserializedBlock);
    }

    std::cout << "\nBlock snapshot deserialization preview:\n";
    std::cout << "Deserialized block count: "
              << deserializedBlocks.size()
              << "\n";
    std::cout << "Deserialized blocks match Blockchain: "
              << (deserializedBlocksMatchBlockchain ? "VALID" : "INVALID")
              << "\n";
    std::cout << "First deserialized block hash: "
              << (deserializedBlocks.empty() ? "" : deserializedBlocks.front().hash())
              << "\n";
    std::cout << "Latest deserialized block hash: "
              << (deserializedBlocks.empty() ? "" : deserializedBlocks.back().hash())
              << "\n";

    if (!deserializedBlocksMatchBlockchain) {
        std::cerr << "Fatal: Block snapshot deserialization validation failed.\n";
        return 1;
    }

    /*
     * Blockchain loader foundation.
     *
     * This loads a complete Blockchain from disk using the manifest, index,
     * and stored block snapshots.
     */
    BlockchainLoadReport blockchainLoadReport =
        BlockchainLoader::auditStorageRoot("data");

    core::Blockchain loadedBlockchain =
        BlockchainLoader::loadFromStorageRoot("data");

    const bool loadedBlockchainMatchesOriginal =
        loadedBlockchain.serialize() == blockchain.serialize();

    State loadedPublicState =
        ChainStateRebuilder::rebuildStateFromLedgerRecords(loadedBlockchain);

    PrivateAccountingLedger loadedPrivateLedger =
        PrivateAccountingLedgerRebuilder::rebuildFromBlockchain(loadedBlockchain);

    const bool loadedPublicStateAuditable =
        loadedPublicState.isSupplyAuditable();

    const bool loadedPrivateLedgerValid =
        loadedPrivateLedger.isValid();

    std::cout << "\nBlockchain loader foundation preview:\n";
    std::cout << "Blockchain loader audit: "
              << (blockchainLoadReport.success() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Blockchain loader report:\n";
    std::cout << blockchainLoadReport.serialize() << "\n";
    std::cout << "Loaded Blockchain block count: "
              << loadedBlockchain.size()
              << "\n";
    std::cout << "Loaded Blockchain validation: "
              << (loadedBlockchain.isValid() ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Loaded Blockchain matches original: "
              << (loadedBlockchainMatchesOriginal ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Loaded public state supply audit: "
              << (loadedPublicStateAuditable ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Loaded private ledger validation: "
              << (loadedPrivateLedgerValid ? "VALID" : "INVALID")
              << "\n";
    std::cout << "Loaded latest block hash: "
              << loadedBlockchain.latestBlock().hash()
              << "\n";

    if (!blockchainLoadReport.success() ||
        !loadedBlockchain.isValid() ||
        !loadedBlockchainMatchesOriginal ||
        !loadedPublicStateAuditable ||
        !loadedPrivateLedgerValid) {
        std::cerr << "Fatal: Blockchain loader validation failed.\n";
        return 1;
    }

    char hashOutput[65] = {0};
    nodo_hash_string(genesisMint.serialize().c_str(), hashOutput, sizeof(hashOutput));

    std::cout << "\nGenesis MintRecord hash preview:\n";
    std::cout << hashOutput << "\n";

    std::cout << "\nBlockchain preview:\n";
    std::cout << blockchain.serialize() << "\n";

    std::cout << "\nNodo Blockchain loader foundation executed successfully.\n";

    return 0;
}

} // namespace nodo::app
