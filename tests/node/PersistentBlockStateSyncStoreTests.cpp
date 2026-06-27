#include "node/PersistentBlockStateSync.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace nodo::node;

static const std::filesystem::path kTempDir =
    std::filesystem::temp_directory_path() / "nodo_persistent_sync_store_test";

static void cleanup() {
    std::filesystem::remove_all(kTempDir);
}

static void test_missing_returns_missing_status() {
    cleanup();
    PersistentSyncCheckpointStore store(kTempDir);
    assert(!store.exists());
    const auto result = store.read();
    assert(result.status() == PersistentSyncCheckpointReadStatus::MISSING);
    assert(!result.loaded());
}

static void test_save_and_read_roundtrip() {
    cleanup();
    PersistentSyncCheckpointStore store(kTempDir);

    PersistentSyncCheckpoint checkpoint = PersistentSyncCheckpoint::genesis(
        "genesis-hash",
        "genesis-state-root",
        1700000200
    );

    const auto writeResult = store.save(checkpoint);
    assert(writeResult.status() == PersistentSyncCheckpointWriteStatus::SAVED);
    assert(writeResult.isSaved());
    assert(store.exists());

    const auto readResult = store.read();
    assert(readResult.status() == PersistentSyncCheckpointReadStatus::LOADED);
    assert(readResult.loaded());
    assert(readResult.checkpoint().isValid());
    assert(readResult.checkpoint().finalizedHeight() == 0);
    assert(readResult.checkpoint().finalizedBlockHash() == "genesis-hash");
    assert(readResult.checkpoint().finalizedStateRoot() == "genesis-state-root");
}

static void test_save_invalid_checkpoint_returns_error() {
    cleanup();
    PersistentSyncCheckpointStore store(kTempDir);

    // Default-constructed checkpoint is invalid.
    PersistentSyncCheckpoint invalid;
    const auto writeResult = store.save(invalid);
    assert(writeResult.status() == PersistentSyncCheckpointWriteStatus::INVALID_CHECKPOINT);
    assert(!writeResult.isSaved());
    assert(!store.exists());
}

static void test_malformed_file_returns_malformed_status() {
    cleanup();
    std::filesystem::create_directories(kTempDir / "sync");
    {
        std::ofstream f(kTempDir / "sync" / "checkpoint.conf");
        f << "this is not valid checkpoint data\n!!!garbage!!!\n";
    }

    PersistentSyncCheckpointStore store(kTempDir);
    assert(store.exists());
    const auto result = store.read();
    assert(result.status() == PersistentSyncCheckpointReadStatus::MALFORMED ||
           result.status() == PersistentSyncCheckpointReadStatus::INVALID);
    assert(!result.loaded());
}

static void test_checkpoint_deserialize_roundtrip() {
    PersistentSyncCheckpoint checkpoint = PersistentSyncCheckpoint::genesis(
        "block-abc123",
        "state-root-xyz",
        1700000200
    );

    const std::string serialized = checkpoint.serialize();
    const PersistentSyncCheckpoint restored = PersistentSyncCheckpoint::deserialize(serialized);
    assert(restored.isValid());
    assert(restored.finalizedBlockHash() == checkpoint.finalizedBlockHash());
    assert(restored.finalizedStateRoot() == checkpoint.finalizedStateRoot());
    assert(restored.finalizedHeight() == checkpoint.finalizedHeight());
}

static void test_read_status_string_conversion() {
    assert(persistentSyncCheckpointReadStatusToString(
               PersistentSyncCheckpointReadStatus::LOADED) == "LOADED");
    assert(persistentSyncCheckpointReadStatusToString(
               PersistentSyncCheckpointReadStatus::MISSING) == "MISSING");
    assert(persistentSyncCheckpointReadStatusToString(
               PersistentSyncCheckpointReadStatus::MALFORMED) == "MALFORMED");
    assert(persistentSyncCheckpointReadStatusToString(
               PersistentSyncCheckpointReadStatus::INVALID) == "INVALID");
    assert(persistentSyncCheckpointReadStatusToString(
               PersistentSyncCheckpointReadStatus::IO_FAILURE) == "IO_FAILURE");
}

static void test_write_status_string_conversion() {
    assert(persistentSyncCheckpointWriteStatusToString(
               PersistentSyncCheckpointWriteStatus::SAVED) == "SAVED");
    assert(persistentSyncCheckpointWriteStatusToString(
               PersistentSyncCheckpointWriteStatus::INVALID_CHECKPOINT) == "INVALID_CHECKPOINT");
    assert(persistentSyncCheckpointWriteStatusToString(
               PersistentSyncCheckpointWriteStatus::IO_FAILURE) == "IO_FAILURE");
}

int main() {
    test_missing_returns_missing_status();
    test_save_and_read_roundtrip();
    test_save_invalid_checkpoint_returns_error();
    test_malformed_file_returns_malformed_status();
    test_checkpoint_deserialize_roundtrip();
    test_read_status_string_conversion();
    test_write_status_string_conversion();

    cleanup();
    std::cout << "persistent block/state sync store tests passed\n";
    return 0;
}
