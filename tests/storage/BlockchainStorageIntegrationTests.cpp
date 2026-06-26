#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/ChainStateRebuilder.hpp"
#include "core/LedgerRecord.hpp"
#include "core/State.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"

#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

#include "economics/MintRecord.hpp"

#include "storage/BlockFileStore.hpp"
#include "storage/BlockStorageIndex.hpp"
#include "storage/BlockchainLoader.hpp"
#include "storage/BlockchainStorageReader.hpp"
#include "storage/ChainManifest.hpp"

#include "serialization/ChainManifestCodec.hpp"
#include "serialization/BlockStorageIndexCodec.hpp"
#include "serialization/BlockSnapshotHeaderCodec.hpp"

#include "utils/Amount.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::SignatureBundle;
using nodo::crypto::SigningDomain;

using nodo::economics::MintReason;
using nodo::economics::MintRecord;

using nodo::storage::BlockFileStore;
using nodo::storage::BlockIndexEntry;
using nodo::storage::BlockStorageIndex;
using nodo::storage::BlockchainLoadReport;
using nodo::storage::BlockSnapshotHeader;
using nodo::storage::BlockchainLoader;
using nodo::storage::BlockchainStorageReadReport;
using nodo::storage::BlockchainStorageReader;
using nodo::storage::ChainManifest;

using nodo::serialization::ChainManifestCodec;
using nodo::serialization::BlockStorageIndexCodec;
using nodo::serialization::BlockSnapshotHeaderCodec;

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


std::string tamperFirstHexCharacter(
    const std::string& value
) {
    if (value.empty()) {
        throw std::invalid_argument("Cannot tamper empty string.");
    }

    const std::string replacementPrefix =
        value.front() == '0' ? "1" : "0";

    return replacementPrefix + value.substr(1);
}

std::string readTextFile(
    const std::filesystem::path& filePath
) {
    std::ifstream input(filePath, std::ios::in | std::ios::binary);

    requireCondition(
        input.is_open(),
        "Failed to open file for test reading: " + filePath.string()
    );

    std::ostringstream buffer;
    buffer << input.rdbuf();

    requireCondition(
        input.good() || input.eof(),
        "Failed while reading test file: " + filePath.string()
    );

    return buffer.str();
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

    const KeyPair mintKeyPair =
        KeyPair::createDeterministicBls12381KeyPair("storage-integration-minter");
    const Bls12381SignatureProvider mintProvider;
    const KeyPair igorKeyPair =
        KeyPair::createDeterministicEd25519KeyPair("storage-integration-igor");
    const Ed25519SignatureProvider userProvider;

    MintRecord genesisMint(
        "mint_storage_integration_igor_001",
        "auth_storage_integration_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        kBaseTimestamp
    );

    SignatureBundle genesisSignatureBundle =
        SignatureBundle::createSignature(
            genesisMint.serialize(),
            mintKeyPair.publicKey(),
            mintKeyPair.privateKeyForSigningOnly(),
            kBaseTimestamp + 1,
            mintProvider,
            SigningDomain::MINT_AUTHORIZATION
        );

    requireCondition(
        genesisSignatureBundle.isValidForPolicy(
            cryptoPolicy,
            SecurityContext::MINT_OPERATION
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
        SignatureBundle::createSignature(
            transferTransaction.signingPayload(),
            igorKeyPair.publicKey(),
            igorKeyPair.privateKeyForSigningOnly(),
            kBaseTimestamp + 5,
            userProvider,
            SigningDomain::USER_TRANSACTION
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

    requireCondition(
        blockchain.isValid(false),
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

    ChainManifest codecRebuiltManifest =
        ChainManifestCodec::deserialize(
            loadedManifest.serialize()
        );

    requireCondition(
        codecRebuiltManifest.serialize() == loadedManifest.serialize(),
        "ChainManifestCodec round-trip changed stored manifest serialization."
    );

    std::string tamperedManifest = loadedManifest.serialize();

    const std::string tamperedManifestHash =
        tamperFirstHexCharacter(
            loadedManifest.manifestHash()
        );

    tamperedManifest.replace(
        tamperedManifest.find(loadedManifest.manifestHash()),
        loadedManifest.manifestHash().size(),
        tamperedManifestHash
    );

    bool manifestTamperRejected = false;

    try {
        (void)ChainManifestCodec::deserialize(tamperedManifest);
    } catch (const std::exception&) {
        manifestTamperRejected = true;
    }

    requireCondition(
        manifestTamperRejected,
        "ChainManifestCodec accepted a tampered manifest hash."
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

    BlockStorageIndex codecRebuiltIndex =
        BlockStorageIndexCodec::deserialize(
            loadedIndex.serialize()
        );

    requireCondition(
        codecRebuiltIndex.serialize() == loadedIndex.serialize(),
        "BlockStorageIndexCodec round-trip changed stored index serialization."
    );

    requireCondition(
        !loadedIndex.entries().empty(),
        "Stored BlockStorageIndex has no entries for codec verification."
    );

    BlockIndexEntry codecRebuiltEntry =
        BlockStorageIndexCodec::deserializeEntry(
            loadedIndex.entries().front().serialize()
        );

    requireCondition(
        codecRebuiltEntry.serialize() == loadedIndex.entries().front().serialize(),
        "BlockStorageIndexCodec round-trip changed first index entry serialization."
    );

    std::string tamperedIndex = loadedIndex.serialize();

    const std::string tamperedIndexHash =
        tamperFirstHexCharacter(
            loadedIndex.indexHash()
        );

    tamperedIndex.replace(
        tamperedIndex.find(loadedIndex.indexHash()),
        loadedIndex.indexHash().size(),
        tamperedIndexHash
    );

    bool indexTamperRejected = false;

    try {
        (void)BlockStorageIndexCodec::deserialize(tamperedIndex);
    } catch (const std::exception&) {
        indexTamperRejected = true;
    }

    requireCondition(
        indexTamperRejected,
        "BlockStorageIndexCodec accepted a tampered index hash."
    );

    std::string tamperedEntry = loadedIndex.entries().front().serialize();

    const std::string originalEntryFileName =
        loadedIndex.entries().front().fileName();

    tamperedEntry.replace(
        tamperedEntry.find(originalEntryFileName),
        originalEntryFileName.size(),
        "../" + originalEntryFileName
    );

    bool entryTamperRejected = false;

    try {
        (void)BlockStorageIndexCodec::deserializeEntry(tamperedEntry);
    } catch (const std::exception&) {
        entryTamperRejected = true;
    }

    requireCondition(
        entryTamperRejected,
        "BlockStorageIndexCodec accepted an unsafe index entry file name."
    );

    const std::filesystem::path firstBlockSnapshotPath =
        rootPath /
        "blocks" /
        loadedIndex.entries().front().fileName();

    const std::string firstSerializedBlock =
        readTextFile(firstBlockSnapshotPath);

    const BlockSnapshotHeader codecRebuiltHeader =
        BlockSnapshotHeaderCodec::deserializeFromSerializedBlock(
            firstSerializedBlock
        );

    requireCondition(
        codecRebuiltHeader.isValid(),
        "BlockSnapshotHeaderCodec produced an invalid header."
    );

    requireCondition(
        codecRebuiltHeader.blockIndex() == loadedIndex.entries().front().blockIndex(),
        "BlockSnapshotHeaderCodec produced an unexpected block index."
    );

    requireCondition(
        codecRebuiltHeader.blockHash() == loadedIndex.entries().front().blockHash(),
        "BlockSnapshotHeaderCodec produced an unexpected block hash."
    );

    std::string tamperedSerializedBlock = firstSerializedBlock;

    const std::string recordCountMarker = "recordCount=1";
    const std::size_t recordCountPosition =
        tamperedSerializedBlock.find(recordCountMarker);

    requireCondition(
        recordCountPosition != std::string::npos,
        "Could not find recordCount marker for header tamper test."
    );

    tamperedSerializedBlock.replace(
        recordCountPosition,
        recordCountMarker.size(),
        "recordCount=999"
    );

    bool headerTamperRejected = false;

    try {
        (void)BlockSnapshotHeaderCodec::deserializeFromSerializedBlock(
            tamperedSerializedBlock
        );
    } catch (const std::exception&) {
        headerTamperRejected = true;
    }

    requireCondition(
        headerTamperRejected,
        "BlockSnapshotHeaderCodec accepted a tampered record count."
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
        loadedBlockchain.isValid(false),
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
