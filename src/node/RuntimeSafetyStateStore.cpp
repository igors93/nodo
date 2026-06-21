#include "node/RuntimeSafetyStateStore.hpp"

#include "economics/DefenseModeState.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <filesystem>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::node {

// ---- RuntimeSafetyStateReadResult ----

std::string runtimeSafetyStateReadStatusToString(
    RuntimeSafetyStateReadStatus status
) {
    switch (status) {
        case RuntimeSafetyStateReadStatus::LOADED:          return "LOADED";
        case RuntimeSafetyStateReadStatus::MISSING:         return "MISSING";
        case RuntimeSafetyStateReadStatus::MALFORMED:       return "MALFORMED";
        case RuntimeSafetyStateReadStatus::INVALID:         return "INVALID";
        case RuntimeSafetyStateReadStatus::SCHEMA_MISMATCH: return "SCHEMA_MISMATCH";
        case RuntimeSafetyStateReadStatus::IO_FAILURE:      return "IO_FAILURE";
        default:                                            return "UNKNOWN";
    }
}

RuntimeSafetyStateReadResult::RuntimeSafetyStateReadResult()
    : m_status(RuntimeSafetyStateReadStatus::MISSING),
      m_reason(""),
      m_state() {}

RuntimeSafetyStateReadResult RuntimeSafetyStateReadResult::loaded(
    RuntimeSafetyState state
) {
    RuntimeSafetyStateReadResult r;
    r.m_status = RuntimeSafetyStateReadStatus::LOADED;
    r.m_state = std::move(state);
    return r;
}

RuntimeSafetyStateReadResult RuntimeSafetyStateReadResult::missing() {
    RuntimeSafetyStateReadResult r;
    r.m_status = RuntimeSafetyStateReadStatus::MISSING;
    return r;
}

RuntimeSafetyStateReadResult RuntimeSafetyStateReadResult::malformed(
    std::string reason
) {
    RuntimeSafetyStateReadResult r;
    r.m_status = RuntimeSafetyStateReadStatus::MALFORMED;
    r.m_reason = std::move(reason);
    return r;
}

RuntimeSafetyStateReadResult RuntimeSafetyStateReadResult::invalid(
    std::string reason
) {
    RuntimeSafetyStateReadResult r;
    r.m_status = RuntimeSafetyStateReadStatus::INVALID;
    r.m_reason = std::move(reason);
    return r;
}

RuntimeSafetyStateReadResult RuntimeSafetyStateReadResult::schemaMismatch(
    std::string reason
) {
    RuntimeSafetyStateReadResult r;
    r.m_status = RuntimeSafetyStateReadStatus::SCHEMA_MISMATCH;
    r.m_reason = std::move(reason);
    return r;
}

RuntimeSafetyStateReadResult RuntimeSafetyStateReadResult::ioFailure(
    std::string reason
) {
    RuntimeSafetyStateReadResult r;
    r.m_status = RuntimeSafetyStateReadStatus::IO_FAILURE;
    r.m_reason = std::move(reason);
    return r;
}

bool RuntimeSafetyStateReadResult::isLoaded() const {
    return m_status == RuntimeSafetyStateReadStatus::LOADED;
}

bool RuntimeSafetyStateReadResult::isMissing() const {
    return m_status == RuntimeSafetyStateReadStatus::MISSING;
}

bool RuntimeSafetyStateReadResult::isFailure() const {
    return m_status != RuntimeSafetyStateReadStatus::LOADED &&
           m_status != RuntimeSafetyStateReadStatus::MISSING;
}

RuntimeSafetyStateReadStatus RuntimeSafetyStateReadResult::status() const {
    return m_status;
}

const std::string& RuntimeSafetyStateReadResult::reason() const {
    return m_reason;
}

const RuntimeSafetyState& RuntimeSafetyStateReadResult::state() const {
    return m_state;
}

// ---- RuntimeSafetyStateWriteResult ----

std::string runtimeSafetyStateWriteStatusToString(
    RuntimeSafetyStateWriteStatus status
) {
    switch (status) {
        case RuntimeSafetyStateWriteStatus::WRITTEN:       return "WRITTEN";
        case RuntimeSafetyStateWriteStatus::INVALID_STATE: return "INVALID_STATE";
        case RuntimeSafetyStateWriteStatus::IO_FAILURE:    return "IO_FAILURE";
        default:                                           return "UNKNOWN";
    }
}

RuntimeSafetyStateWriteResult::RuntimeSafetyStateWriteResult()
    : m_status(RuntimeSafetyStateWriteStatus::IO_FAILURE),
      m_reason("") {}

RuntimeSafetyStateWriteResult RuntimeSafetyStateWriteResult::written() {
    RuntimeSafetyStateWriteResult r;
    r.m_status = RuntimeSafetyStateWriteStatus::WRITTEN;
    return r;
}

RuntimeSafetyStateWriteResult RuntimeSafetyStateWriteResult::invalidState(
    std::string reason
) {
    RuntimeSafetyStateWriteResult r;
    r.m_status = RuntimeSafetyStateWriteStatus::INVALID_STATE;
    r.m_reason = std::move(reason);
    return r;
}

RuntimeSafetyStateWriteResult RuntimeSafetyStateWriteResult::ioFailure(
    std::string reason
) {
    RuntimeSafetyStateWriteResult r;
    r.m_status = RuntimeSafetyStateWriteStatus::IO_FAILURE;
    r.m_reason = std::move(reason);
    return r;
}

bool RuntimeSafetyStateWriteResult::isWritten() const {
    return m_status == RuntimeSafetyStateWriteStatus::WRITTEN;
}

RuntimeSafetyStateWriteStatus RuntimeSafetyStateWriteResult::status() const {
    return m_status;
}

const std::string& RuntimeSafetyStateWriteResult::reason() const {
    return m_reason;
}

// ---- RuntimeSafetyStateStore ----

namespace {

const std::string kSchemaId = "NODO_RUNTIME_SAFETY_STATE";

const std::set<std::string> kAllowedFields = {
    "defenseMode",
    "activationTrigger",
    "activationHeight",
    "activationReason",
    "evidenceId",
    "governanceProposalId",
    "lastChainAuditHeight",
    "exitRequiresChainAudit",
    "updatedAt"
};

// Codec rejects empty string values; use "none" as sentinel for optional fields.
std::string encodeOptional(const std::string& value) {
    return value.empty() ? "none" : value;
}

std::string decodeOptional(const std::string& raw) {
    return (raw == "none") ? "" : raw;
}

std::uint64_t parseUint64Field(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    if (raw.empty()) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: field '" + key + "' is empty"
        );
    }
    for (const char c : raw) {
        if (c < '0' || c > '9') {
            throw std::runtime_error(
                "RuntimeSafetyStateStore: field '" + key + "' is not a valid uint64"
            );
        }
    }
    std::size_t parsedCharacters = 0;
    const unsigned long long parsed = std::stoull(raw, &parsedCharacters);
    if (parsedCharacters != raw.size()) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: field '" + key + "' is not a valid uint64"
        );
    }
    return static_cast<std::uint64_t>(parsed);
}

std::int64_t parseInt64Field(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    if (raw.empty()) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: field '" + key + "' is empty"
        );
    }
    for (std::size_t index = 0; index < raw.size(); ++index) {
        const char c = raw[index];
        if (c == '-' && index == 0 && raw.size() > 1) {
            continue;
        }
        if (c < '0' || c > '9') {
            throw std::runtime_error(
                "RuntimeSafetyStateStore: field '" + key + "' is not a valid int64"
            );
        }
    }
    std::size_t parsedCharacters = 0;
    const long long parsed = std::stoll(raw, &parsedCharacters);
    if (parsedCharacters != raw.size()) {
        throw std::runtime_error(
            "RuntimeSafetyStateStore: field '" + key + "' is not a valid int64"
        );
    }
    return static_cast<std::int64_t>(parsed);
}

bool parseBoolField(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    if (raw == "true") return true;
    if (raw == "false") return false;
    throw std::runtime_error(
        "RuntimeSafetyStateStore: field '" + key + "' is not a valid bool"
    );
}

economics::DefenseModeTrigger parseTrigger(const std::string& s) {
    if (s == "SUPPLY_DIVERGENCE")
        return economics::DefenseModeTrigger::SUPPLY_DIVERGENCE;
    if (s == "DOUBLE_SIGN_MASS_EVENT")
        return economics::DefenseModeTrigger::DOUBLE_SIGN_MASS_EVENT;
    if (s == "UNAUTHORIZED_TREASURY_SPEND_ATTEMPT")
        return economics::DefenseModeTrigger::UNAUTHORIZED_TREASURY_SPEND_ATTEMPT;
    if (s == "CHAIN_AUDIT_FAILURE")
        return economics::DefenseModeTrigger::CHAIN_AUDIT_FAILURE;
    if (s == "STORAGE_CORRUPTION")
        return economics::DefenseModeTrigger::STORAGE_CORRUPTION;
    if (s == "OPERATOR_DECLARED")
        return economics::DefenseModeTrigger::OPERATOR_DECLARED;
    if (s == "GOVERNANCE_VOTED")
        return economics::DefenseModeTrigger::GOVERNANCE_VOTED;
    throw std::runtime_error(
        "RuntimeSafetyStateStore: unknown activationTrigger: " + s
    );
}

} // namespace

RuntimeSafetyStateWriteResult RuntimeSafetyStateStore::write(
    const std::filesystem::path& path,
    const RuntimeSafetyState& state
) {
    if (!state.isValid()) {
        return RuntimeSafetyStateWriteResult::invalidState(
            "cannot write invalid RuntimeSafetyState: " + state.rejectionReason()
        );
    }

    try {
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
                {"governanceProposalId",
                 encodeOptional(state.governanceProposalId())},
                {"lastChainAuditHeight",
                 std::to_string(state.lastChainAuditHeight())},
                {"exitRequiresChainAudit",
                 state.exitRequiresChainAudit() ? "true" : "false"},
                {"updatedAt",
                 std::to_string(state.updatedAt())}
            }
        );
        storage::AtomicFile::writeTextFile(path, contents);
        return RuntimeSafetyStateWriteResult::written();
    } catch (const std::exception& e) {
        return RuntimeSafetyStateWriteResult::ioFailure(
            std::string("RuntimeSafetyStateStore: write failed: ") + e.what()
        );
    }
}

RuntimeSafetyStateReadResult RuntimeSafetyStateStore::read(
    const std::filesystem::path& path
) {
    if (!std::filesystem::exists(path)) {
        return RuntimeSafetyStateReadResult::missing();
    }

    std::string contents;
    try {
        contents = storage::AtomicFile::readTextFile(path);
    } catch (const std::exception& e) {
        return RuntimeSafetyStateReadResult::ioFailure(
            std::string("RuntimeSafetyStateStore: cannot read file: ") + e.what()
        );
    }

    serialization::KeyValueFileDocument doc;
    try {
        doc = serialization::KeyValueFileCodec::parse(contents, kSchemaId);
    } catch (const std::exception& e) {
        // Wrong schema version or malformed key-value structure.
        const std::string msg = e.what();
        if (msg.find("version") != std::string::npos ||
            msg.find("schema") != std::string::npos ||
            msg.find("Schema") != std::string::npos) {
            return RuntimeSafetyStateReadResult::schemaMismatch(
                std::string("RuntimeSafetyStateStore: schema mismatch: ") + msg
            );
        }
        return RuntimeSafetyStateReadResult::malformed(
            std::string("RuntimeSafetyStateStore: malformed file: ") + msg
        );
    }

    try {
        doc.requireOnlyFields(kAllowedFields);

        economics::DefenseModeState defenseMode;
        if (!economics::defenseModeStateFromString(
                doc.requireField("defenseMode"), defenseMode)) {
            return RuntimeSafetyStateReadResult::malformed(
                "RuntimeSafetyStateStore: unknown defenseMode value"
            );
        }

        const economics::DefenseModeTrigger trigger =
            parseTrigger(doc.requireField("activationTrigger"));

        const std::uint64_t activationHeight =
            parseUint64Field(doc, "activationHeight");
        const std::string activationReason =
            decodeOptional(doc.requireField("activationReason"));
        const std::string evidenceId =
            decodeOptional(doc.requireField("evidenceId"));
        const std::string governanceProposalId =
            decodeOptional(doc.requireField("governanceProposalId"));
        const std::uint64_t lastChainAuditHeight =
            parseUint64Field(doc, "lastChainAuditHeight");
        const bool exitRequiresChainAudit =
            parseBoolField(doc, "exitRequiresChainAudit");
        const std::int64_t updatedAt =
            parseInt64Field(doc, "updatedAt");

        RuntimeSafetyState state(
            defenseMode,
            trigger,
            activationHeight,
            activationReason,
            evidenceId,
            governanceProposalId,
            lastChainAuditHeight,
            exitRequiresChainAudit,
            updatedAt
        );

        if (!state.isValid()) {
            return RuntimeSafetyStateReadResult::invalid(
                "RuntimeSafetyStateStore: loaded state is invalid: " +
                state.rejectionReason()
            );
        }

        return RuntimeSafetyStateReadResult::loaded(std::move(state));

    } catch (const std::exception& e) {
        return RuntimeSafetyStateReadResult::malformed(
            std::string("RuntimeSafetyStateStore: field parse error: ") + e.what()
        );
    }
}

} // namespace nodo::node
