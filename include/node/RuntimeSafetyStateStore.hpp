#ifndef NODO_NODE_RUNTIME_SAFETY_STATE_STORE_HPP
#define NODO_NODE_RUNTIME_SAFETY_STATE_STORE_HPP

#include "node/RuntimeSafetyState.hpp"

#include <filesystem>
#include <string>

namespace nodo::node {

enum class RuntimeSafetyStateReadStatus {
    LOADED,
    MISSING,
    MALFORMED,
    INVALID,
    SCHEMA_MISMATCH,
    IO_FAILURE
};

std::string runtimeSafetyStateReadStatusToString(RuntimeSafetyStateReadStatus status);

class RuntimeSafetyStateReadResult {
public:
    RuntimeSafetyStateReadResult();

    static RuntimeSafetyStateReadResult loaded(RuntimeSafetyState state);
    static RuntimeSafetyStateReadResult missing();
    static RuntimeSafetyStateReadResult malformed(std::string reason);
    static RuntimeSafetyStateReadResult invalid(std::string reason);
    static RuntimeSafetyStateReadResult schemaMismatch(std::string reason);
    static RuntimeSafetyStateReadResult ioFailure(std::string reason);

    bool isLoaded() const;
    bool isMissing() const;
    bool isFailure() const;

    RuntimeSafetyStateReadStatus status() const;
    const std::string& reason() const;
    const RuntimeSafetyState& state() const;

private:
    RuntimeSafetyStateReadStatus m_status;
    std::string m_reason;
    RuntimeSafetyState m_state;
};

enum class RuntimeSafetyStateWriteStatus {
    WRITTEN,
    INVALID_STATE,
    IO_FAILURE
};

std::string runtimeSafetyStateWriteStatusToString(RuntimeSafetyStateWriteStatus status);

class RuntimeSafetyStateWriteResult {
public:
    RuntimeSafetyStateWriteResult();

    static RuntimeSafetyStateWriteResult written();
    static RuntimeSafetyStateWriteResult invalidState(std::string reason);
    static RuntimeSafetyStateWriteResult ioFailure(std::string reason);

    bool isWritten() const;
    RuntimeSafetyStateWriteStatus status() const;
    const std::string& reason() const;

private:
    RuntimeSafetyStateWriteStatus m_status;
    std::string m_reason;
};

/*
 * RuntimeSafetyStateStore persists and reloads the node's protocol safety
 * state using an atomic write pattern and the standard Nodo key-value format.
 *
 * Security principle:
 * Missing file → MISSING (may initialize to canonical safe default for new nodes).
 * Corrupt or schema-mismatch file → failure status (caller must treat as unsafe).
 * Invalid state content → INVALID (caller must fail readiness).
 * I/O failure → IO_FAILURE (caller must fail readiness).
 * read() never silently treats unreadable state as inactive defense mode.
 */
class RuntimeSafetyStateStore {
public:
    // Write state atomically. Rejects invalid state without writing.
    static RuntimeSafetyStateWriteResult write(
        const std::filesystem::path& path,
        const RuntimeSafetyState& state
    );

    // Read state from path. Returns a structured result indicating exactly why
    // the read succeeded or failed — never collapses failure reasons.
    static RuntimeSafetyStateReadResult read(
        const std::filesystem::path& path
    );
};

} // namespace nodo::node

#endif
