#include "consensus/ConsensusRecoveryStore.hpp"

#include "serialization/KeyValueFileCodec.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace nodo::consensus {

namespace {

const std::string kRecoverySchemaId = "NODO_CONSENSUS_RECOVERY_STATE_V1";

bool parseUint64Strict(
    const std::string& value,
    std::uint64_t& out
) {
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
        const unsigned long long parsed =
            std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size() ||
            parsed > static_cast<unsigned long long>(
                std::numeric_limits<std::uint64_t>::max()
            )) {
            return false;
        }
        out = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseInt64Strict(
    const std::string& value,
    std::int64_t& out
) {
    if (value.empty()) {
        return false;
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == '-' && index == 0 && value.size() > 1) {
            continue;
        }
        if (c < '0' || c > '9') {
            return false;
        }
    }

    try {
        std::size_t parsedCharacters = 0;
        const long long parsed =
            std::stoll(value, &parsedCharacters);
        if (parsedCharacters != value.size()) {
            return false;
        }
        out = static_cast<std::int64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool isPersistableRoundState(
    const ConsensusRoundState& state
) {
    return state.isValid() &&
           state.height() > 0 &&
           state.round() > 0 &&
           !state.proposerAddress().empty();
}

std::string serializeState(
    const ConsensusRoundState& state
) {
    if (!isPersistableRoundState(state)) {
        throw std::invalid_argument("Invalid consensus round state cannot be persisted.");
    }

    return serialization::KeyValueFileCodec::serialize(
        kRecoverySchemaId,
        {
            {"height", std::to_string(state.height())},
            {"round", std::to_string(state.round())},
            {"proposerAddress", state.proposerAddress()},
            {"roundStartedAt", std::to_string(state.roundStartedAt())},
            {"lockedBlockHash", state.lockedBlockHash().empty() ? "-" : state.lockedBlockHash()},
            {"lockedRound", std::to_string(state.lockedRound())},
            {"votedPrevote", state.votedPrevote() ? "1" : "0"},
            {"votedPrecommit", state.votedPrecommit() ? "1" : "0"}
        }
    );
}

std::optional<ConsensusRoundState> parseState(
    const std::string& content
) {
    try {
        const serialization::KeyValueFileDocument doc =
            serialization::KeyValueFileCodec::parse(
                content,
                kRecoverySchemaId
            );

        doc.requireOnlyFields(
            {
                "height",
                "round",
                "proposerAddress",
                "roundStartedAt",
                "lockedBlockHash",
                "lockedRound",
                "votedPrevote",
                "votedPrecommit"
            }
        );

        std::uint64_t height = 0;
        std::uint64_t round = 0;
        std::int64_t roundStartedAt = 0;
        std::uint64_t lockedRound = 0;

        if (!parseUint64Strict(doc.requireField("height"), height) ||
            !parseUint64Strict(doc.requireField("round"), round) ||
            !parseInt64Strict(doc.requireField("roundStartedAt"), roundStartedAt) ||
            !parseUint64Strict(doc.requireField("lockedRound"), lockedRound)) {
            return std::nullopt;
        }

        const std::string votedPrevoteStr = doc.requireField("votedPrevote");
        const std::string votedPrecommitStr = doc.requireField("votedPrecommit");
        if ((votedPrevoteStr != "0" && votedPrevoteStr != "1") ||
            (votedPrecommitStr != "0" && votedPrecommitStr != "1")) {
            return std::nullopt;
        }

        const std::string rawLockedBlockHash = doc.requireField("lockedBlockHash");
        const std::string lockedBlockHash = (rawLockedBlockHash == "-") ? "" : rawLockedBlockHash;

        const ConsensusRoundState state(
            height,
            round,
            doc.requireField("proposerAddress"),
            roundStartedAt,
            lockedBlockHash,
            lockedRound,
            votedPrevoteStr == "1",
            votedPrecommitStr == "1"
        );

        if (!isPersistableRoundState(state)) {
            return std::nullopt;
        }

        if (serializeState(state) != content) {
            return std::nullopt;
        }

        return state;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

bool ConsensusRecoveryStore::save(
    const std::filesystem::path& path,
    const ConsensusRoundState& state
) {
    try {
        const std::string content =
            serializeState(state);

        const std::filesystem::path parent =
            path.parent_path();
        if (!parent.empty()) {
            std::error_code createError;
            std::filesystem::create_directories(parent, createError);
            if (createError) {
                return false;
            }
        }

        std::filesystem::path tmp = path;
        tmp += ".tmp";

        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }

            out << content;
            if (!out) {
                std::error_code removeError;
                std::filesystem::remove(tmp, removeError);
                return false;
            }
        }

        std::error_code renameError;
        std::filesystem::rename(tmp, path, renameError);
        if (renameError) {
            std::error_code removeError;
            std::filesystem::remove(tmp, removeError);
            return false;
        }

        return true;
    } catch (...) {
        std::filesystem::path tmp = path;
        tmp += ".tmp";
        std::error_code removeError;
        std::filesystem::remove(tmp, removeError);
        return false;
    }
}

std::optional<ConsensusRoundState> ConsensusRecoveryStore::load(
    const std::filesystem::path& path
) {
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError) || existsError) {
        return std::nullopt;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    std::ostringstream content;
    content << in.rdbuf();
    if (!content || content.str().empty()) {
        return std::nullopt;
    }

    return parseState(content.str());
}

bool ConsensusRecoveryStore::remove(
    const std::filesystem::path& path
) {
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError)) {
        return !existsError;
    }

    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    return !removeError;
}

} // namespace nodo::consensus
