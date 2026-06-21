#include "node/StateSnapshotStore.hpp"

#include <filesystem>
#include <fstream>
#include <string>

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
    const core::State& /*state*/,
    std::uint64_t /*blockHeight*/
) {
    // TODO: State::serialize() is not yet implemented.
    // When State gains a serialize() method, implement save() as:
    //
    //   const std::string content =
    //       std::string(HEADER) + "\n" +
    //       std::to_string(blockHeight) + "\n" +
    //       state.serialize();
    //
    //   const std::filesystem::path tmp = std::filesystem::path(m_path).replace_extension(".tmp");
    //   {
    //       std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    //       if (!out) return false;
    //       out << content;
    //       if (!out) return false;
    //   }
    //   std::filesystem::rename(tmp, m_path);
    //   return true;
    //
    // Until then, this is a no-op and callers must use ChainStateRebuilder as fallback.
    return false;
}

std::optional<core::State> StateSnapshotStore::load(
    std::uint64_t /*expectedHeight*/
) const {
    // TODO: State::deserialize() is not yet implemented.
    // When State gains a static deserialize(const std::string&) method,
    // implement load() as:
    //
    //   if (!std::filesystem::exists(m_path)) return std::nullopt;
    //
    //   std::ifstream in(m_path, std::ios::binary);
    //   if (!in) return std::nullopt;
    //
    //   std::string headerLine, heightLine;
    //   if (!std::getline(in, headerLine)) return std::nullopt;
    //   if (headerLine != HEADER) return std::nullopt;
    //
    //   if (!std::getline(in, heightLine)) return std::nullopt;
    //   std::uint64_t parsedHeight = 0;
    //   try {
    //       parsedHeight = std::stoull(heightLine);
    //   } catch (...) {
    //       return std::nullopt;
    //   }
    //
    //   if (parsedHeight != expectedHeight) return std::nullopt;
    //
    //   std::ostringstream rest;
    //   rest << in.rdbuf();
    //   return core::State::deserialize(rest.str());
    //
    // Until then, return nullopt so callers fall back to ChainStateRebuilder.
    return std::nullopt;
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
