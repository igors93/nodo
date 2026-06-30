#include "consensus/ConsensusRecoveryStore.hpp"

#include "crypto/Hex.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

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

const std::string kRecoverySchemaId = "NODO_CONSENSUS_RECOVERY_STATE_V2";

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


std::string encodeOptionalVote(
    const std::optional<ValidatorVoteRecord>& vote
) {
    if (!vote.has_value()) {
        return "-";
    }
    const std::string serialized = vote->serialize();
    return crypto::hexEncode(
        reinterpret_cast<const unsigned char*>(serialized.data()),
        serialized.size()
    );
}

std::optional<ValidatorVoteRecord> decodeOptionalVote(
    const std::string& value
) {
    if (value == "-") {
        return std::nullopt;
    }
    if (!crypto::isHexString(value)) {
        throw std::invalid_argument("Persisted consensus vote is not canonical hex.");
    }
    const std::vector<unsigned char> bytes = crypto::hexDecode(value);
    const std::string serialized(bytes.begin(), bytes.end());
    return ValidatorVoteRecord::deserialize(serialized);
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
            {"votedPrecommit", state.votedPrecommit() ? "1" : "0"},
            {"persistedPrevoteHex", encodeOptionalVote(state.persistedPrevote())},
            {"persistedPrecommitHex", encodeOptionalVote(state.persistedPrecommit())}
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
                "votedPrecommit",
                "persistedPrevoteHex",
                "persistedPrecommitHex"
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

        const std::optional<ValidatorVoteRecord> persistedPrevote =
            decodeOptionalVote(doc.requireField("persistedPrevoteHex"));
        const std::optional<ValidatorVoteRecord> persistedPrecommit =
            decodeOptionalVote(doc.requireField("persistedPrecommitHex"));

        const ConsensusRoundState state(
            height,
            round,
            doc.requireField("proposerAddress"),
            roundStartedAt,
            lockedBlockHash,
            lockedRound,
            votedPrevoteStr == "1",
            votedPrecommitStr == "1",
            persistedPrevote,
            persistedPrecommit
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
        const std::string content = serializeState(state);

        const std::filesystem::path parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code createError;
            std::filesystem::create_directories(parent, createError);
            if (createError) {
                return false;
            }
        }

        // AtomicFile uses fsync() + rename so the state is durable even on
        // an OS crash between the two steps.  This is critical because the
        // recovery store is written after every BFT vote — losing it would
        // allow a validator to vote for a different block in the same round
        // after restart, breaking the equivocation-free invariant.
        storage::AtomicFile::writeTextFile(path, content);
        return true;
    } catch (...) {
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
