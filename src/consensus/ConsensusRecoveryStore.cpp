#include "consensus/ConsensusRecoveryStore.hpp"

#include <fstream>
#include <sstream>

namespace nodo::consensus {

bool ConsensusRecoveryStore::save(
    const std::filesystem::path& path,
    const ConsensusRoundState& state
) {
    if (!state.isValid()) {
        return false;
    }
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }
    out << state.serialize();
    return out.good();
}

static std::optional<ConsensusRoundState> parseState(const std::string& content) {
    // Format: ConsensusRoundState{height=H;round=R;proposer=P;startedAt=T}
    auto extractField = [&](const std::string& key) -> std::string {
        const std::string searchKey = key + "=";
        const auto pos = content.find(searchKey);
        if (pos == std::string::npos) return "";
        const auto start = pos + searchKey.size();
        const auto end = content.find_first_of(";}", start);
        if (end == std::string::npos) return "";
        return content.substr(start, end - start);
    };

    const std::string heightStr = extractField("height");
    const std::string roundStr = extractField("round");
    const std::string proposer = extractField("proposer");
    const std::string startedAtStr = extractField("startedAt");

    if (heightStr.empty() || roundStr.empty() || startedAtStr.empty()) {
        return std::nullopt;
    }

    try {
        const std::uint64_t height = std::stoull(heightStr);
        const std::uint64_t round = std::stoull(roundStr);
        const std::int64_t startedAt = std::stoll(startedAtStr);
        return ConsensusRoundState(height, round, proposer, startedAt);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<ConsensusRoundState> ConsensusRecoveryStore::load(
    const std::filesystem::path& path
) {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        return std::nullopt;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return parseState(oss.str());
}

bool ConsensusRecoveryStore::remove(
    const std::filesystem::path& path
) {
    if (!std::filesystem::exists(path)) {
        return true;
    }
    return std::filesystem::remove(path);
}

} // namespace nodo::consensus
