#include "consensus/BlockFinalizer.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::consensus {

namespace {

bool isSafeScalar(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character == ';' ||
            character == '{' ||
            character == '}' ||
            character == '[' ||
            character == ']' ||
            character == '\n' ||
            character == '\r' ||
            character == '\t') {
            return false;
        }
    }

    return true;
}

std::vector<std::string> splitTopLevel(
    const std::string& value,
    char separator
) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    int braceDepth = 0;
    int bracketDepth = 0;

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current =
            value[index];

        if (current == '{') {
            ++braceDepth;
        } else if (current == '}') {
            --braceDepth;
        } else if (current == '[') {
            ++bracketDepth;
        } else if (current == ']') {
            --bracketDepth;
        }

        if (braceDepth < 0 || bracketDepth < 0) {
            throw std::invalid_argument("Malformed nested serialized value.");
        }

        if (current == separator &&
            braceDepth == 0 &&
            bracketDepth == 0) {
            if (index == start) {
                throw std::invalid_argument("Empty serialized field.");
            }

            parts.push_back(value.substr(start, index - start));
            start = index + 1;
        }
    }

    if (braceDepth != 0 || bracketDepth != 0) {
        throw std::invalid_argument("Unbalanced nested serialized value.");
    }

    if (start >= value.size()) {
        throw std::invalid_argument("Serialized value has a trailing separator.");
    }

    parts.push_back(value.substr(start));
    return parts;
}

std::map<std::string, std::string> parseObjectFields(
    const std::string& serialized,
    const std::string& typeName
) {
    const std::string prefix =
        typeName + "{";

    if (serialized.rfind(prefix, 0) != 0 ||
        serialized.size() <= prefix.size() ||
        serialized.back() != '}') {
        throw std::invalid_argument("Serialized data is not a " + typeName + ".");
    }

    const std::string body =
        serialized.substr(
            prefix.size(),
            serialized.size() - prefix.size() - 1
        );

    if (body.empty()) {
        throw std::invalid_argument("Serialized " + typeName + " is empty.");
    }

    std::map<std::string, std::string> fields;

    for (const std::string& part : splitTopLevel(body, ';')) {
        const std::size_t separator =
            part.find('=');

        if (separator == std::string::npos ||
            separator == 0 ||
            separator + 1 >= part.size()) {
            throw std::invalid_argument("Malformed serialized " + typeName + " field.");
        }

        const std::string key =
            part.substr(0, separator);

        const std::string value =
            part.substr(separator + 1);

        if (!fields.emplace(key, value).second) {
            throw std::invalid_argument("Duplicate serialized " + typeName + " field: " + key);
        }
    }

    return fields;
}

void requireExactFields(
    const std::map<std::string, std::string>& fields,
    const std::set<std::string>& expected,
    const std::string& typeName
) {
    for (const std::string& key : expected) {
        if (fields.find(key) == fields.end()) {
            throw std::invalid_argument("Missing serialized " + typeName + " field: " + key);
        }
    }

    for (const auto& [key, ignored] : fields) {
        (void)ignored;

        if (expected.find(key) == expected.end()) {
            throw std::invalid_argument("Unknown serialized " + typeName + " field: " + key);
        }
    }
}

std::string requireField(
    const std::map<std::string, std::string>& fields,
    const std::string& key,
    const std::string& typeName
) {
    const auto found =
        fields.find(key);

    if (found == fields.end()) {
        throw std::invalid_argument("Missing serialized " + typeName + " field: " + key);
    }

    return found->second;
}

std::uint64_t parseU64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (const char current : value) {
        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::uint64_t parsed =
        static_cast<std::uint64_t>(
            std::stoull(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

std::int64_t parseI64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current =
            value[index];

        if (current == '-' && index == 0 && value.size() > 1) {
            continue;
        }

        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::int64_t parsed =
        static_cast<std::int64_t>(
            std::stoll(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

} // namespace

FinalizedBlockRecord::FinalizedBlockRecord()
    : m_blockIndex(0),
      m_blockHash(""),
      m_previousHash(""),
      m_round(0),
      m_finalizedAt(0),
      m_quorumCertificate() {}

FinalizedBlockRecord::FinalizedBlockRecord(
    std::uint64_t blockIndex,
    std::string blockHash,
    std::string previousHash,
    std::uint64_t round,
    std::int64_t finalizedAt,
    QuorumCertificate quorumCertificate
)
    : m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_round(round),
      m_finalizedAt(finalizedAt),
      m_quorumCertificate(std::move(quorumCertificate)) {}

std::uint64_t FinalizedBlockRecord::blockIndex() const {
    return m_blockIndex;
}

const std::string& FinalizedBlockRecord::blockHash() const {
    return m_blockHash;
}

const std::string& FinalizedBlockRecord::previousHash() const {
    return m_previousHash;
}

std::uint64_t FinalizedBlockRecord::round() const {
    return m_round;
}

std::int64_t FinalizedBlockRecord::finalizedAt() const {
    return m_finalizedAt;
}

const QuorumCertificate& FinalizedBlockRecord::quorumCertificate() const {
    return m_quorumCertificate;
}

bool FinalizedBlockRecord::matchesBlock(
    const core::Block& block
) const {
    return m_blockIndex == block.index() &&
           m_blockHash == block.hash() &&
           m_previousHash == block.previousHash();
}

bool FinalizedBlockRecord::isStructurallyValid() const {
    if (m_blockIndex == 0 ||
        m_round == 0 ||
        m_finalizedAt <= 0) {
        return false;
    }

    if (!isSafeScalar(m_blockHash) ||
        !isSafeScalar(m_previousHash)) {
        return false;
    }

    if (!m_quorumCertificate.isStructurallyValid()) {
        return false;
    }

    if (m_quorumCertificate.blockIndex() != m_blockIndex ||
        m_quorumCertificate.blockHash() != m_blockHash ||
        m_quorumCertificate.previousHash() != m_previousHash ||
        m_quorumCertificate.round() != m_round) {
        return false;
    }

    return true;
}

bool FinalizedBlockRecord::verify(
    const core::ValidatorRegistry& validatorRegistry,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!isStructurallyValid()) {
        return false;
    }

    return m_quorumCertificate.verify(
        validatorRegistry,
        policy,
        provider
    );
}

std::string FinalizedBlockRecord::serialize() const {
    std::ostringstream oss;

    oss << "FinalizedBlockRecord{"
        << "blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";previousHash=" << m_previousHash
        << ";round=" << m_round
        << ";finalizedAt=" << m_finalizedAt
        << ";quorumCertificate=" << m_quorumCertificate.serialize()
        << "}";

    return oss.str();
}

FinalizedBlockRecord FinalizedBlockRecord::deserialize(
    const std::string& serialized
) {
    const std::map<std::string, std::string> fields =
        parseObjectFields(
            serialized,
            "FinalizedBlockRecord"
        );

    requireExactFields(
        fields,
        {
            "blockIndex",
            "blockHash",
            "previousHash",
            "round",
            "finalizedAt",
            "quorumCertificate"
        },
        "FinalizedBlockRecord"
    );

    FinalizedBlockRecord record(
        parseU64Strict(
            requireField(fields, "blockIndex", "FinalizedBlockRecord"),
            "FinalizedBlockRecord.blockIndex"
        ),
        requireField(fields, "blockHash", "FinalizedBlockRecord"),
        requireField(fields, "previousHash", "FinalizedBlockRecord"),
        parseU64Strict(
            requireField(fields, "round", "FinalizedBlockRecord"),
            "FinalizedBlockRecord.round"
        ),
        parseI64Strict(
            requireField(fields, "finalizedAt", "FinalizedBlockRecord"),
            "FinalizedBlockRecord.finalizedAt"
        ),
        QuorumCertificate::deserialize(
            requireField(fields, "quorumCertificate", "FinalizedBlockRecord")
        )
    );

    if (!record.isStructurallyValid()) {
        throw std::invalid_argument("Serialized FinalizedBlockRecord is structurally invalid.");
    }

    if (record.serialize() != serialized) {
        throw std::invalid_argument("Serialized FinalizedBlockRecord is non-canonical.");
    }

    return record;
}

std::string blockFinalizationRegistryStatusToString(
    BlockFinalizationRegistryStatus status
) {
    switch (status) {
        case BlockFinalizationRegistryStatus::REGISTERED:
            return "REGISTERED";
        case BlockFinalizationRegistryStatus::DUPLICATE:
            return "DUPLICATE";
        case BlockFinalizationRegistryStatus::CONFLICTING_FINALIZATION:
            return "CONFLICTING_FINALIZATION";
        case BlockFinalizationRegistryStatus::INVALID_RECORD:
            return "INVALID_RECORD";
        case BlockFinalizationRegistryStatus::INVALID_REGISTRY:
            return "INVALID_REGISTRY";
        default:
            return "INVALID_REGISTRY";
    }
}

BlockFinalizationRegistryResult::BlockFinalizationRegistryResult()
    : m_status(BlockFinalizationRegistryStatus::INVALID_REGISTRY),
      m_reason("Uninitialized block finalization registry result."),
      m_record() {}

BlockFinalizationRegistryResult BlockFinalizationRegistryResult::registered(
    FinalizedBlockRecord record
) {
    BlockFinalizationRegistryResult result;
    result.m_status = BlockFinalizationRegistryStatus::REGISTERED;
    result.m_reason = "";
    result.m_record = std::move(record);
    return result;
}

BlockFinalizationRegistryResult BlockFinalizationRegistryResult::duplicate(
    FinalizedBlockRecord record
) {
    BlockFinalizationRegistryResult result;
    result.m_status = BlockFinalizationRegistryStatus::DUPLICATE;
    result.m_reason = "Finalized block already registered.";
    result.m_record = std::move(record);
    return result;
}

BlockFinalizationRegistryResult BlockFinalizationRegistryResult::rejected(
    BlockFinalizationRegistryStatus status,
    std::string reason
) {
    BlockFinalizationRegistryResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

BlockFinalizationRegistryStatus BlockFinalizationRegistryResult::status() const {
    return m_status;
}

const std::string& BlockFinalizationRegistryResult::reason() const {
    return m_reason;
}

const FinalizedBlockRecord& BlockFinalizationRegistryResult::record() const {
    return m_record;
}

bool BlockFinalizationRegistryResult::registered() const {
    return m_status == BlockFinalizationRegistryStatus::REGISTERED;
}

bool BlockFinalizationRegistryResult::duplicate() const {
    return m_status == BlockFinalizationRegistryStatus::DUPLICATE;
}

bool BlockFinalizationRegistryResult::success() const {
    return registered() || duplicate();
}

std::string BlockFinalizationRegistryResult::serialize() const {
    std::ostringstream oss;

    oss << "BlockFinalizationRegistryResult{"
        << "status=" << blockFinalizationRegistryStatusToString(m_status)
        << ";reason=" << m_reason
        << ";record="
        << (m_record.isStructurallyValid() ? m_record.serialize() : "NONE")
        << "}";

    return oss.str();
}

BlockFinalizationRegistry::BlockFinalizationRegistry()
    : m_recordsByHeight() {}

bool BlockFinalizationRegistry::canRegister(
    const FinalizedBlockRecord& record
) const {
    if (!record.isStructurallyValid()) {
        return false;
    }

    const auto existing =
        m_recordsByHeight.find(record.blockIndex());

    if (existing == m_recordsByHeight.end()) {
        return true;
    }

    return existing->second.blockHash() == record.blockHash();
}

BlockFinalizationRegistryResult BlockFinalizationRegistry::registerFinalizedBlock(
    const FinalizedBlockRecord& record
) {
    if (!record.isStructurallyValid()) {
        return BlockFinalizationRegistryResult::rejected(
            BlockFinalizationRegistryStatus::INVALID_RECORD,
            "Finalized block record is structurally invalid."
        );
    }

    const auto existing =
        m_recordsByHeight.find(record.blockIndex());

    if (existing != m_recordsByHeight.end()) {
        if (!existing->second.isStructurallyValid()) {
            return BlockFinalizationRegistryResult::rejected(
                BlockFinalizationRegistryStatus::INVALID_REGISTRY,
                "Existing finalized block record is invalid."
            );
        }

        if (existing->second.blockHash() == record.blockHash()) {
            return BlockFinalizationRegistryResult::duplicate(
                existing->second
            );
        }

        return BlockFinalizationRegistryResult::rejected(
            BlockFinalizationRegistryStatus::CONFLICTING_FINALIZATION,
            "A different block hash is already finalized at this height."
        );
    }

    m_recordsByHeight.emplace(
        record.blockIndex(),
        record
    );

    if (!isValid()) {
        m_recordsByHeight.erase(record.blockIndex());

        return BlockFinalizationRegistryResult::rejected(
            BlockFinalizationRegistryStatus::INVALID_REGISTRY,
            "Finalization registry failed post-registration audit."
        );
    }

    return BlockFinalizationRegistryResult::registered(record);
}

bool BlockFinalizationRegistry::hasFinalizedHeight(
    std::uint64_t blockIndex
) const {
    return m_recordsByHeight.find(blockIndex) != m_recordsByHeight.end();
}

bool BlockFinalizationRegistry::isFinalizedBlock(
    std::uint64_t blockIndex,
    const std::string& blockHash
) const {
    const auto existing =
        m_recordsByHeight.find(blockIndex);

    if (existing == m_recordsByHeight.end()) {
        return false;
    }

    return existing->second.blockHash() == blockHash;
}

const FinalizedBlockRecord* BlockFinalizationRegistry::recordForHeight(
    std::uint64_t blockIndex
) const {
    const auto existing =
        m_recordsByHeight.find(blockIndex);

    if (existing == m_recordsByHeight.end()) {
        return nullptr;
    }

    return &existing->second;
}

std::uint64_t BlockFinalizationRegistry::highestFinalizedHeight() const {
    if (m_recordsByHeight.empty()) {
        return 0;
    }

    return m_recordsByHeight.rbegin()->first;
}

std::size_t BlockFinalizationRegistry::size() const {
    return m_recordsByHeight.size();
}

bool BlockFinalizationRegistry::isValid() const {
    for (const auto& [height, record] : m_recordsByHeight) {
        if (height == 0 ||
            record.blockIndex() != height ||
            !record.isStructurallyValid()) {
            return false;
        }
    }

    return true;
}

std::string BlockFinalizationRegistry::serialize() const {
    std::ostringstream oss;

    oss << "BlockFinalizationRegistry{"
        << "size=" << m_recordsByHeight.size()
        << ";highestFinalizedHeight=" << highestFinalizedHeight()
        << ";records=[";

    bool first = true;

    for (const auto& [_, record] : m_recordsByHeight) {
        if (!first) {
            oss << ",";
        }

        oss << record.serialize();
        first = false;
    }

    oss << "]}";

    return oss.str();
}

std::string blockFinalizationStatusToString(
    BlockFinalizationStatus status
) {
    switch (status) {
        case BlockFinalizationStatus::FINALIZED:
            return "FINALIZED";
        case BlockFinalizationStatus::DUPLICATE_FINALIZATION:
            return "DUPLICATE_FINALIZATION";
        case BlockFinalizationStatus::INVALID_BLOCKCHAIN:
            return "INVALID_BLOCKCHAIN";
        case BlockFinalizationStatus::INVALID_BLOCK:
            return "INVALID_BLOCK";
        case BlockFinalizationStatus::INVALID_CERTIFICATE:
            return "INVALID_CERTIFICATE";
        case BlockFinalizationStatus::CERTIFICATE_BLOCK_MISMATCH:
            return "CERTIFICATE_BLOCK_MISMATCH";
        case BlockFinalizationStatus::INVALID_FINALIZATION_REGISTRY:
            return "INVALID_FINALIZATION_REGISTRY";
        case BlockFinalizationStatus::ALREADY_FINALIZED_CONFLICT:
            return "ALREADY_FINALIZED_CONFLICT";
        case BlockFinalizationStatus::APPEND_REJECTED:
            return "APPEND_REJECTED";
        case BlockFinalizationStatus::REGISTRATION_FAILED:
            return "REGISTRATION_FAILED";
        default:
            return "INVALID_BLOCK";
    }
}

BlockFinalizationResult::BlockFinalizationResult()
    : m_status(BlockFinalizationStatus::INVALID_BLOCK),
      m_reason("Uninitialized block finalization result."),
      m_record() {}

BlockFinalizationResult BlockFinalizationResult::finalized(
    FinalizedBlockRecord record
) {
    BlockFinalizationResult result;
    result.m_status = BlockFinalizationStatus::FINALIZED;
    result.m_reason = "";
    result.m_record = std::move(record);
    return result;
}

BlockFinalizationResult BlockFinalizationResult::duplicate(
    FinalizedBlockRecord record
) {
    BlockFinalizationResult result;
    result.m_status = BlockFinalizationStatus::DUPLICATE_FINALIZATION;
    result.m_reason = "Block already finalized.";
    result.m_record = std::move(record);
    return result;
}

BlockFinalizationResult BlockFinalizationResult::rejected(
    BlockFinalizationStatus status,
    std::string reason
) {
    BlockFinalizationResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

BlockFinalizationStatus BlockFinalizationResult::status() const {
    return m_status;
}

const std::string& BlockFinalizationResult::reason() const {
    return m_reason;
}

const FinalizedBlockRecord& BlockFinalizationResult::record() const {
    return m_record;
}

bool BlockFinalizationResult::finalized() const {
    return m_status == BlockFinalizationStatus::FINALIZED;
}

bool BlockFinalizationResult::duplicate() const {
    return m_status == BlockFinalizationStatus::DUPLICATE_FINALIZATION;
}

bool BlockFinalizationResult::success() const {
    return finalized() || duplicate();
}

std::string BlockFinalizationResult::serialize() const {
    std::ostringstream oss;

    oss << "BlockFinalizationResult{"
        << "status=" << blockFinalizationStatusToString(m_status)
        << ";reason=" << m_reason
        << ";record="
        << (m_record.isStructurallyValid() ? m_record.serialize() : "NONE")
        << "}";

    return oss.str();
}

BlockFinalizationResult BlockFinalizer::finalizeBlock(
    core::Blockchain& blockchain,
    const core::Block& block,
    const QuorumCertificate& certificate,
    const core::ValidatorRegistry& validatorRegistry,
    BlockFinalizationRegistry& finalizationRegistry,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    std::int64_t finalizedAt
) {
    if (blockchain.empty() || !blockchain.isValid(false)) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::INVALID_BLOCKCHAIN,
            "Blockchain is empty or invalid."
        );
    }

    if (!finalizationRegistry.isValid()) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::INVALID_FINALIZATION_REGISTRY,
            "Block finalization registry is invalid."
        );
    }

    if (!block.isValid(false) || block.isGenesisBlock()) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::INVALID_BLOCK,
            "Only valid non-genesis blocks can be finalized."
        );
    }

    if (finalizedAt <= 0) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::INVALID_BLOCK,
            "Finalization timestamp must be positive."
        );
    }

    if (!blockchain.canAppendBlock(block)) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::APPEND_REJECTED,
            "Block cannot be appended to current blockchain tip."
        );
    }

    if (!certificate.verify(
            validatorRegistry,
            policy,
            provider
        )) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::INVALID_CERTIFICATE,
            "Quorum certificate failed verification."
        );
    }

    if (!certificateMatchesBlock(
            block,
            certificate
        )) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::CERTIFICATE_BLOCK_MISMATCH,
            "Quorum certificate does not match block."
        );
    }

    FinalizedBlockRecord record(
        block.index(),
        block.hash(),
        block.previousHash(),
        certificate.round(),
        finalizedAt,
        certificate
    );

    if (!record.verify(
            validatorRegistry,
            policy,
            provider
        )) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::INVALID_CERTIFICATE,
            "Finalized block record failed verification."
        );
    }

    if (!finalizationRegistry.canRegister(record)) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::ALREADY_FINALIZED_CONFLICT,
            "A conflicting block is already finalized at this height."
        );
    }

    blockchain.addBlock(block);

    const BlockFinalizationRegistryResult registration =
        finalizationRegistry.registerFinalizedBlock(record);

    if (registration.duplicate()) {
        return BlockFinalizationResult::duplicate(
            registration.record()
        );
    }

    if (!registration.registered()) {
        return BlockFinalizationResult::rejected(
            BlockFinalizationStatus::REGISTRATION_FAILED,
            registration.reason()
        );
    }

    return BlockFinalizationResult::finalized(
        registration.record()
    );
}

bool BlockFinalizer::certificateMatchesBlock(
    const core::Block& block,
    const QuorumCertificate& certificate
) {
    return certificate.blockIndex() == block.index() &&
           certificate.blockHash() == block.hash() &&
           certificate.previousHash() == block.previousHash();
}

} // namespace nodo::consensus
