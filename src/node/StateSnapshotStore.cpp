#include "node/StateSnapshotStore.hpp"

#include "core/StateRootCalculator.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace nodo::node {

namespace {

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

        const std::filesystem::path parent = m_path.parent_path();
        if (!parent.empty()) {
            std::error_code createError;
            std::filesystem::create_directories(parent, createError);
            if (createError) {
                return false;
            }
        }

        std::filesystem::path tmp = m_path;
        tmp += ".tmp";

        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }

            out << HEADER << "\n"
                << blockHeight << "\n"
                << stateRoot << "\n"
                << serializedState;

            if (!out) {
                std::error_code removeError;
                std::filesystem::remove(tmp, removeError);
                return false;
            }
        }

        std::error_code renameError;
        std::filesystem::rename(tmp, m_path, renameError);
        if (renameError) {
            std::error_code removeError;
            std::filesystem::remove(tmp, removeError);
            return false;
        }

        return true;
    } catch (...) {
        std::filesystem::path tmp = m_path;
        tmp += ".tmp";
        std::error_code removeError;
        std::filesystem::remove(tmp, removeError);
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
        if (!std::getline(in, headerLine)) return std::nullopt;
        if (headerLine != std::string(HEADER)) return std::nullopt;
        if (!std::getline(in, heightLine)) return std::nullopt;
        if (!std::getline(in, storedStateRoot)) return std::nullopt;
        if (storedStateRoot.empty()) return std::nullopt;

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
