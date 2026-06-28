#include "node/StateSnapshotStore.hpp"

#include "core/StateRootCalculator.hpp"
#include "crypto/hash.h"
#include "storage/AtomicFile.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace nodo::node {

namespace {

std::string hashStatePayload(const std::string& serialized) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(serialized.c_str(), output, sizeof(output));
    return std::string(output);
}

bool parseUint64Strict(const std::string& value, std::uint64_t& out) {
    if (value.empty()) {
        return false;
    }

    for (const char c : value) {
        if (c < '0' || c > '9') {
            return false;
        }
    }

    try {
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed = std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size()) {
            return false;
        }
        out = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

StateSnapshotStore::StateSnapshotStore(std::filesystem::path snapshotPath)
    : m_path(std::move(snapshotPath))
{}

bool StateSnapshotStore::save(
    const core::State& state,
    std::uint64_t blockHeight
) {
    if (state.currentBlockIndex() != blockHeight) {
        return false;
    }

    try {
        const std::string serializedState = state.serialize();
        const std::string stateRoot =
            core::StateRootCalculator::calculateAccountStateRoot(
                state.accountStateView()
            );

        if (stateRoot.empty()) {
            return false;
        }

        // Full-state hash guards against tampering of any field not covered by
        // the account root (e.g. validator registry jailed/tombstoned flags,
        // slashed amounts, or applied transaction ids).
        const std::string fullStateHash = hashStatePayload(serializedState);

        std::ostringstream contents;
        contents << HEADER << "\n"
                 << blockHeight << "\n"
                 << stateRoot << "\n"
                 << fullStateHash << "\n"
                 << serializedState;

        storage::AtomicFile::writeTextFile(m_path, contents.str());

        return true;
    } catch (...) {
        return false;
    }
}

std::optional<core::State> StateSnapshotStore::load(
    std::uint64_t expectedHeight
) const {
    if (!std::filesystem::exists(m_path)) {
        return std::nullopt;
    }

    try {
        std::ifstream in(m_path, std::ios::binary);
        if (!in) {
            return std::nullopt;
        }

        std::string headerLine;
        std::string heightLine;
        std::string storedStateRoot;
        std::string storedFullStateHash;
        if (!std::getline(in, headerLine)) return std::nullopt;
        if (headerLine != std::string(HEADER)) return std::nullopt;
        if (!std::getline(in, heightLine)) return std::nullopt;
        if (!std::getline(in, storedStateRoot)) return std::nullopt;
        if (storedStateRoot.empty()) return std::nullopt;
        if (!std::getline(in, storedFullStateHash)) return std::nullopt;
        if (storedFullStateHash.empty()) return std::nullopt;

        std::uint64_t parsedHeight = 0;
        if (!parseUint64Strict(heightLine, parsedHeight)) {
            return std::nullopt;
        }
        if (parsedHeight != expectedHeight) {
            return std::nullopt;
        }

        std::ostringstream serializedState;
        serializedState << in.rdbuf();
        if (!serializedState || serializedState.str().empty()) {
            return std::nullopt;
        }

        core::State state =
            core::State::deserialize(serializedState.str());

        if (state.currentBlockIndex() != parsedHeight) {
            return std::nullopt;
        }

        const std::string calculatedRoot =
            core::StateRootCalculator::calculateAccountStateRoot(
                state.accountStateView()
            );
        if (calculatedRoot.empty() || calculatedRoot != storedStateRoot) {
            return std::nullopt;
        }

        // Verify the full-state hash to detect tampering in fields not covered
        // by the account root (validator registry, economic state, etc.).
        const std::string calculatedFullHash = hashStatePayload(serializedState.str());
        if (calculatedFullHash.empty() || calculatedFullHash != storedFullStateHash) {
            return std::nullopt;
        }

        return state;
    } catch (...) {
        return std::nullopt;
    }
}

bool StateSnapshotStore::exists() const {
    return std::filesystem::exists(m_path);
}

void StateSnapshotStore::remove() {
    if (std::filesystem::exists(m_path)) {
        std::filesystem::remove(m_path);
    }
}

std::uint64_t StateSnapshotStore::snapshotHeight() const {
    if (!std::filesystem::exists(m_path)) {
        return 0;
    }

    std::ifstream in(m_path, std::ios::binary);
    if (!in) return 0;

    std::string headerLine, heightLine;
    if (!std::getline(in, headerLine)) return 0;
    if (headerLine != std::string(HEADER)) return 0;

    if (!std::getline(in, heightLine)) return 0;

    std::uint64_t parsedHeight = 0;
    if (!parseUint64Strict(heightLine, parsedHeight)) {
        return 0;
    }

    return parsedHeight;
}

std::filesystem::path StateSnapshotStore::path() const {
    return m_path;
}

} // namespace nodo::node
