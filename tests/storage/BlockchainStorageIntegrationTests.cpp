#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/ChainStateRebuilder.hpp"
#include "core/LedgerRecord.hpp"
#include "core/State.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"

#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"

#include "economics/MintRecord.hpp"

#include "privacy/PrivacyCommitment.hpp"
#include "privacy/PrivacyNullifier.hpp"
#include "privacy/PrivateAccountingLedger.hpp"
#include "privacy/PrivateAccountingLedgerRebuilder.hpp"
#include "privacy/PrivateAccountingRecord.hpp"

#include "storage/BlockFileStore.hpp"
#include "storage/BlockStorageIndex.hpp"
#include "storage/BlockchainLoader.hpp"
#include "storage/BlockchainStorageReader.hpp"
#include "storage/ChainManifest.hpp"

#include "utils/Amount.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::ChainStateRebuilder;
using nodo::core::LedgerRecord;
using nodo::core::State;
using nodo::core::Transaction;
using nodo::core::TransactionType;

using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::SignatureBundle;

using nodo::economics::MintReason;
using nodo::economics::MintRecord;

using nodo::privacy::PrivacyCommitment;
using nodo::privacy::PrivacyCommitmentType;
using nodo::privacy::PrivacyNullifier;
using nodo::privacy::PrivacyNullifierType;
using nodo::privacy::PrivateAccountingLedger;
using nodo::privacy::PrivateAccountingLedgerRebuilder;
using nodo::privacy::PrivateAccountingRecord;

using nodo::storage::BlockFileStore;
using nodo::storage::BlockStorageIndex;
using nodo::storage::BlockchainLoadReport;
using nodo::storage::BlockchainLoader;
using nodo::storage::BlockchainStorageReadReport;
using nodo::storage::BlockchainStorageReader;
using nodo::storage::ChainManifest;

using nodo::utils::Amount;

constexpr std::int64_t kBaseTimestamp = 1800000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

std::filesystem::path storageRootPath(
    const std::string& name
) {
    return std::filesystem::path("build") /
           "tests" /
           "storage" /
           name;
}

Blockchain buildReferenceBlockchain() {
    Blockchain blockchain;
    CryptoPolicy cryptoPolicy = CryptoPolicy::developmentPolicy();

    PublicKey igorPublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-storage-integration-public-key"
    );

    PrivateKey igorPrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-storage-integration-private-key"
    );

    MintRecord genesisMint(
        "mint_storage_integration_igor_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        kBaseTimestamp
    );

    SignatureBundle genesisSignatureBundle =
        SignatureBundle::createDevelopmentSignature(
            genesisMint.serialize(),
            igorPublicKey,
            igorPrivateKey,
            kBaseTimestamp + 1
        );

    requireCondition(
        genesisSignatureBundle.isValidForPolicy(
            cryptoPolicy,
            SecurityContext::DEVELOPMENT_ONLY
        ),
        "Genesis signature policy check failed."
    );

    LedgerRecord genesisLedgerRecord =
        LedgerRecord::fromMintRecord(
            genesisMint,
            kBaseTimestamp + 2
        );

    Block genesisBlock =
        Block::createGenesisBlock(
            std::vector<LedgerRecord>{genesisLedgerRecord},
            kBaseTimestamp + 3
        );

    blockchain.addGenesisBlock(genesisBlock);

    Transaction transferTransaction(
        TransactionType::TRANSFER,
        "igor",
        "ana",
        Amount::fromNodo(25),
        Amount::fromRawUnits(100000),
        1,
        kBaseTimestamp + 4
    );

    SignatureBundle transferSignatureBundle =
        SignatureBundle::createDevelopmentSignature(
            transferTransaction.signingPayload(),
            igorPublicKey,
            igorPrivateKey,
            kBaseTimestamp + 5
        );

    transferTransaction.attachSignatureBundle(transferSignatureBundle);

    requireCondition(
        transferTransaction.isStructurallyValid(
            cryptoPolicy,
            SecurityContext::USER_TRANSACTION
        ),
        "Transfer transaction validation failed."
    );

    LedgerRecord transferLedgerRecord =
        LedgerRecord::fromTransaction(
            transferTransaction,
            cryptoPolicy,
            SecurityContext::USER_TRANSACTION,
            kBaseTimestamp + 6
        );

    Block transferBlock(
        1,
        blockchain.latestBlock().hash(),
        std::vector<LedgerRecord>{transferLedgerRecord},
        kBaseTimestamp + 7
    );

    blockchain.addBlock(transferBlock);

    PrivacyCommitment privateMintCommitment =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::MINT_COMMITMENT,
            "igor",
            Amount::fromNodo(1000),
            "storage-integration-blinding-secret-001",
            genesisMint.id(),
            kBaseTimestamp + 8
        );

    PrivacyNullifier privateSpendNullifier =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            privateMintCommitment.id(),
            "storage-integration-owner-secret-igor-001",
            "storage-integration-private-spend-context-001",
            kBaseTimestamp + 9
        );

    PrivacyCommitment privateTransferOutputToAna =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT,
            "ana",
            Amount::fromNodo(25),
            "storage-integration-blinding-secret-ana-output-001",
            privateSpendNullifier.id(),
            kBaseTimestamp + 10
        );

    PrivacyCommitment privateTransferChangeToIgor =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT,
            "igor",
            Amount::fromRawUnits(97499900000),
            "storage-integration-blinding-secret-igor-change-001",
            privateSpendNullifier.id(),
            kBaseTimestamp + 11
        );

    PrivateAccountingRecord privateMintRecord =
        PrivateAccountingRecord::createPrivateMintRecord(
            std::vector<PrivacyCommitment>{privateMintCommitment},
            Amount::fromNodo(1000),
            "storage-integration-private-mint-audit-reference-001",
            kBaseTimestamp + 12
        );

    PrivateAccountingRecord privateTransferRecord =
        PrivateAccountingRecord::createPrivateTransferRecord(
            std::vector<PrivacyNullifier>{privateSpendNullifier},
            std::vector<PrivacyCommitment>{
                privateTransferOutputToAna,
                privateTransferChangeToIgor
            },
            "storage-integration-private-transfer-audit-reference-001",
            kBaseTimestamp + 13
        );

    LedgerRecord privateMintLedgerRecord =
        LedgerRecord::fromPrivateAccountingRecord(
            privateMintRecord,
            kBaseTimestamp + 14
        );

    LedgerRecord privateTransferLedgerRecord =
        LedgerRecord::fromPrivateAccountingRecord(
            privateTransferRecord,
            kBaseTimestamp + 15
        );

    Block privateAccountingBlock(
        2,
        blockchain.latestBlock().hash(),
        std::vector<LedgerRecord>{
            privateMintLedgerRecord,
            privateTransferLedgerRecord
        },
        kBaseTimestamp + 16
    );

    blockchain.addBlock(privateAccountingBlock);

    requireCondition(
        blockchain.isValid(),
        "Reference Blockchain is invalid."
    );

    return blockchain;
}

void writeCompleteStorage(
    const Blockchain& blockchain,
    const std::filesystem::path& rootPath
) {
    std::filesystem::remove_all(rootPath);
    std::filesystem::create_directories(rootPath);

    BlockFileStore blockFileStore(rootPath.string());

    blockFileStore.clearBlockStorage();
    blockFileStore.writeBlockchain(blockchain);

    requireCondition(
        blockFileStore.storedBlockFileCount() == blockchain.size(),
        "Stored block file count does not match Blockchain size."
    );

    for (const auto& block : blockchain.blocks()) {
        requireCondition(
            blockFileStore.verifyStoredBlock(block),
            "Stored block snapshot verification failed."
        );
    }

    ChainManifest manifest =
        ChainManifest::fromBlockchain(
            blockchain,
            kBaseTimestamp + 20
        );

    manifest.writeToStorageRoot(rootPath.string());

    ChainManifest loadedManifest =
        ChainManifest::readFromStorageRoot(rootPath.string());

    requireCondition(
        loadedManifest.matchesBlockchain(blockchain),
        "Stored ChainManifest does not match Blockchain."
    );

    BlockStorageIndex index =
        BlockStorageIndex::fromBlockchainAndManifest(
            blockchain,
            loadedManifest,
            kBaseTimestamp + 21
        );

    index.writeToStorageRoot(rootPath.string());

    BlockStorageIndex loadedIndex =
        BlockStorageIndex::readFromStorageRoot(rootPath.string());

    requireCondition(
        loadedIndex.matchesBlockchainAndManifest(
            blockchain,
            loadedManifest
        ),
        "Stored BlockStorageIndex does not match Blockchain and ChainManifest."
    );
}

void assertLoadedBlockchainMatchesReference(
    const Blockchain& referenceBlockchain,
    const std::filesystem::path& rootPath
) {
    BlockchainStorageReadReport storageReadReport =
        BlockchainStorageReader::auditStorageRoot(rootPath.string());

    requireCondition(
        storageReadReport.success(),
        "BlockchainStorageReader audit failed: " +
            storageReadReport.failureReason()
    );

    BlockchainLoadReport loadReport =
        BlockchainLoader::auditStorageRoot(rootPath.string());

    requireCondition(
        loadReport.success(),
        "BlockchainLoader audit failed: " +
            loadReport.failureReason()
    );

    Blockchain loadedBlockchain =
        BlockchainLoader::loadFromStorageRoot(rootPath.string());

    requireCondition(
        loadedBlockchain.isValid(),
        "Loaded Blockchain is invalid."
    );

    requireCondition(
        loadedBlockchain.serialize() == referenceBlockchain.serialize(),
        "Loaded Blockchain serialization does not match reference Blockchain."
    );

    State loadedPublicState =
        ChainStateRebuilder::rebuildStateFromLedgerRecords(loadedBlockchain);

    requireCondition(
        loadedPublicState.isSupplyAuditable(),
        "Loaded public State failed supply audit."
    );

    requireCondition(
        loadedPublicState.balanceOf("igor").toString() ==
            "974.99900000 NODO",
        "Loaded Igor balance is incorrect."
    );

    requireCondition(
        loadedPublicState.balanceOf("ana").toString() ==
            "25.00000000 NODO",
        "Loaded Ana balance is incorrect."
    );

    requireCondition(
        loadedPublicState.balanceOf(State::feePoolAddress()).toString() ==
            "0.00100000 NODO",
        "Loaded fee pool balance is incorrect."
    );

    PrivateAccountingLedger loadedPrivateLedger =
        PrivateAccountingLedgerRebuilder::rebuildFromBlockchain(
            loadedBlockchain
        );

    requireCondition(
        loadedPrivateLedger.isValid(),
        "Loaded private ledger is invalid."
    );

    requireCondition(
        loadedPrivateLedger.size() == 2,
        "Loaded private ledger record count is incorrect."
    );

    requireCondition(
        loadedPrivateLedger.outstandingPrivateSupply().toString() ==
            "1000.00000000 NODO",
        "Loaded private outstanding supply is incorrect."
    );
}

void assertTamperedStorageIsRejected(
    const Blockchain& referenceBlockchain,
    const std::filesystem::path& rootPath
) {
    writeCompleteStorage(referenceBlockchain, rootPath);

    const BlockStorageIndex index =
        BlockStorageIndex::readFromStorageRoot(rootPath.string());

    requireCondition(
        !index.entries().empty(),
        "BlockStorageIndex has no entries for tamper test."
    );

    const std::filesystem::path firstBlockPath =
        rootPath /
        "blocks" /
        index.entries().front().fileName();

    {
        std::ofstream output(
            firstBlockPath,
            std::ios::out | std::ios::binary | std::ios::app
        );

        requireCondition(
            output.is_open(),
            "Failed to open block snapshot for tamper test."
        );

        output << "tampered-data";
    }

    const BlockchainLoadReport tamperedReport =
        BlockchainLoader::auditStorageRoot(rootPath.string());

    requireCondition(
        !tamperedReport.success(),
        "Tampered storage was not rejected."
    );
}

void testBlockchainStorageIntegration() {
    const Blockchain referenceBlockchain =
        buildReferenceBlockchain();

    const std::filesystem::path validStorageRoot =
        storageRootPath("valid_chain");

    writeCompleteStorage(referenceBlockchain, validStorageRoot);

    assertLoadedBlockchainMatchesReference(
        referenceBlockchain,
        validStorageRoot
    );

    const std::filesystem::path tamperedStorageRoot =
        storageRootPath("tampered_chain");

    assertTamperedStorageIsRejected(
        referenceBlockchain,
        tamperedStorageRoot
    );

    std::filesystem::remove_all(validStorageRoot);
    std::filesystem::remove_all(tamperedStorageRoot);
}

} // namespace

int main() {
    try {
        testBlockchainStorageIntegration();

        std::cout << "Nodo blockchain storage integration tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo blockchain storage integration tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}