#ifndef NODO_NODE_RUNTIME_SAFETY_STATE_STORE_HPP
#define NODO_NODE_RUNTIME_SAFETY_STATE_STORE_HPP

#include "node/RuntimeSafetyState.hpp"

#include <filesystem>
#include <optional>

namespace nodo::node {

/*
 * RuntimeSafetyStateStore persists and reloads the node's protocol safety
 * state using the standard Nodo key-value file format.
 *
 * Security principle:
 * Missing file → caller may treat as new-node default (INACTIVE).
 * Corrupt or schema-mismatch file → caller must treat as unsafe.
 * read() returns nullopt on corruption so the caller can fail closed.
 */
class RuntimeSafetyStateStore {
public:
    // Write state to the given path atomically.
    static void write(
        const std::filesystem::path& path,
        const RuntimeSafetyState& state
    );

    // Read state from the given path.
    // Returns nullopt if the file cannot be parsed or contains invalid state.
    // Throws std::runtime_error only for unrecoverable I/O failures.
    static std::optional<RuntimeSafetyState> read(
        const std::filesystem::path& path
    );
};

} // namespace nodo::node

#endif
