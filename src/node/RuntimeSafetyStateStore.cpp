#include "node/RuntimeSafetyStateStore.hpp"

#include "economics/DefenseModeState.hpp"
#include "serialization/KeyValueFileCodec.hpp"

#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

const std::string kSchemaId = "NODO_RUNTIME_SAFETY_STATE";

const std::set<std::string> kAllowedFields = {
    "defenseMode",
    "activationTrigger",
    "activationHeight",
    "activationReason",
    "evidenceId",
    "lastChainAuditHeight",
    "exitRequiresChainAudit",
    "updatedAt"
};

std::string readFileContents(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: cannot open file: " + path.string()
        );
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

void writeFileContents(
    const std::filesystem::path& path,
    const std::string& contents
) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: cannot write file: " + path.string()
        );
    }
    file << contents;
    if (!file.good()) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: write error: " + path.string()
        );
    }
}

std::uint64_t parseUint64(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    try {
        return static_cast<std::uint64_t>(std::stoull(raw));
    } catch (const std::exception&) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: field '" + key +
            "' is not a valid uint64: " + raw
        );
    }
}

std::int64_t parseInt64(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    try {
        return std::stoll(raw);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: field '" + key +
            "' is not a valid int64: " + raw
        );
    }
}

bool parseBool(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    if (raw == "true") return true;
    if (raw == "false") return false;
    throw std::runtime_error(
        "RuntimeSafetyStateStore: field '" + key +
        "' is not a valid bool: " + raw
    );
}

} // namespace

// Codec rejects empty string values; use "none" as sentinel for optional fields.
static std::string encodeOptional(const std::string& value) {
    return value.empty() ? "none" : value;
}

static std::string decodeOptional(const std::string& raw) {
    return (raw == "none") ? "" : raw;
}

void RuntimeSafetyStateStore::write(
    const std::filesystem::path& path,
    const RuntimeSafetyState& state
) {
    const std::string contents = serialization::KeyValueFileCodec::serialize(
        kSchemaId,
        {
            {"defenseMode",
             economics::defenseModeStateToString(state.defenseMode())},
            {"activationTrigger",
             economics::defenseModeTriggerToString(state.activationTrigger())},
            {"activationHeight",
             std::to_string(state.activationHeight())},
            {"activationReason",
             encodeOptional(state.activationReason())},
            {"evidenceId",
             encodeOptional(state.evidenceId())},
            {"lastChainAuditHeight",
             std::to_string(state.lastChainAuditHeight())},
            {"exitRequiresChainAudit",
             state.exitRequiresChainAudit() ? "true" : "false"},
            {"updatedAt",
             std::to_string(state.updatedAt())}
        }
    );
    writeFileContents(path, contents);
}

std::optional<RuntimeSafetyState> RuntimeSafetyStateStore::read(
    const std::filesystem::path& path
) {
    try {
        const std::string contents = readFileContents(path);
        const serialization::KeyValueFileDocument doc =
            serialization::KeyValueFileCodec::parse(contents, kSchemaId);

        doc.requireOnlyFields(kAllowedFields);

        economics::DefenseModeState defenseMode;
        if (!economics::defenseModeStateFromString(
                doc.requireField("defenseMode"), defenseMode)) {
            return std::nullopt;
        }

        // activationTrigger: parse string to enum.
        const std::string triggerStr = doc.requireField("activationTrigger");
        economics::DefenseModeTrigger trigger =
            economics::DefenseModeTrigger::OPERATOR_DECLARED;
        if (triggerStr == "SUPPLY_DIVERGENCE") {
            trigger = economics::DefenseModeTrigger::SUPPLY_DIVERGENCE;
        } else if (triggerStr == "DOUBLE_SIGN_MASS_EVENT") {
            trigger = economics::DefenseModeTrigger::DOUBLE_SIGN_MASS_EVENT;
        } else if (triggerStr == "UNAUTHORIZED_TREASURY_SPEND_ATTEMPT") {
            trigger = economics::DefenseModeTrigger::UNAUTHORIZED_TREASURY_SPEND_ATTEMPT;
        } else if (triggerStr == "CHAIN_AUDIT_FAILURE") {
            trigger = economics::DefenseModeTrigger::CHAIN_AUDIT_FAILURE;
        } else if (triggerStr == "STORAGE_CORRUPTION") {
            trigger = economics::DefenseModeTrigger::STORAGE_CORRUPTION;
        } else if (triggerStr == "OPERATOR_DECLARED") {
            trigger = economics::DefenseModeTrigger::OPERATOR_DECLARED;
        } else if (triggerStr == "GOVERNANCE_VOTED") {
            trigger = economics::DefenseModeTrigger::GOVERNANCE_VOTED;
        } else {
            return std::nullopt;
        }

        const std::uint64_t activationHeight = parseUint64(doc, "activationHeight");
        const std::string activationReason =
            decodeOptional(doc.requireField("activationReason"));
        const std::string evidenceId =
            decodeOptional(doc.requireField("evidenceId"));
        const std::uint64_t lastChainAuditHeight =
            parseUint64(doc, "lastChainAuditHeight");
        const bool exitRequiresChainAudit = parseBool(doc, "exitRequiresChainAudit");
        const std::int64_t updatedAt = parseInt64(doc, "updatedAt");

        RuntimeSafetyState state(
            defenseMode,
            trigger,
            activationHeight,
            activationReason,
            evidenceId,
            lastChainAuditHeight,
            exitRequiresChainAudit,
            updatedAt
        );

        if (!state.isValid()) {
            return std::nullopt;
        }

        return state;

    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace nodo::node
