#include "node/LightClientProtocol.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <utility>

namespace nodo::node {
namespace {

std::string hashString(const std::string &input) {
  char output[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_bytes(reinterpret_cast<const unsigned char *>(input.data()),
                  input.size(), output, sizeof(output));
  return std::string(output, NODO_HASH_HEX_SIZE);
}

std::string jsonString(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  out.push_back('"');
  return out;
}

std::string merkleProofJson(const core::MerkleProof &proof) {
  std::ostringstream oss;
  oss << "{\"leafHash\":" << jsonString(proof.leafHash())
      << ",\"reconstructedRoot\":" << jsonString(proof.reconstructRoot())
      << ",\"steps\":[";
  const auto &steps = proof.steps();
  for (std::size_t i = 0; i < steps.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << "{\"siblingHash\":" << jsonString(steps[i].siblingHash)
        << ",\"siblingIsLeft\":" << (steps[i].siblingIsLeft ? "true" : "false")
        << "}";
  }
  oss << "]}";
  return oss.str();
}

} // namespace

LightClientHeader::LightClientHeader() : m_height(0), m_timestamp(0) {}

LightClientHeader::LightClientHeader(
    std::string networkName, std::string chainId, std::string genesisConfigId,
    std::uint64_t height, std::string blockHash, std::string previousHash,
    std::string stateRoot, std::string receiptsRoot, std::int64_t timestamp,
    std::string headerPayload, std::string validatorSetRoot,
    std::string finalizedRecord, std::string quorumCertificate)
    : m_networkName(std::move(networkName)), m_chainId(std::move(chainId)),
      m_genesisConfigId(std::move(genesisConfigId)), m_height(height),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_stateRoot(std::move(stateRoot)),
      m_receiptsRoot(std::move(receiptsRoot)), m_timestamp(timestamp),
      m_headerPayload(std::move(headerPayload)),
      m_validatorSetRoot(std::move(validatorSetRoot)),
      m_finalizedRecord(std::move(finalizedRecord)),
      m_quorumCertificate(std::move(quorumCertificate)) {}

const std::string &LightClientHeader::networkName() const {
  return m_networkName;
}
const std::string &LightClientHeader::chainId() const { return m_chainId; }
const std::string &LightClientHeader::genesisConfigId() const {
  return m_genesisConfigId;
}
std::uint64_t LightClientHeader::height() const { return m_height; }
const std::string &LightClientHeader::blockHash() const { return m_blockHash; }
const std::string &LightClientHeader::previousHash() const {
  return m_previousHash;
}
const std::string &LightClientHeader::stateRoot() const { return m_stateRoot; }
const std::string &LightClientHeader::receiptsRoot() const {
  return m_receiptsRoot;
}
std::int64_t LightClientHeader::timestamp() const { return m_timestamp; }
const std::string &LightClientHeader::headerPayload() const {
  return m_headerPayload;
}
const std::string &LightClientHeader::validatorSetRoot() const {
  return m_validatorSetRoot;
}
const std::string &LightClientHeader::finalizedRecord() const {
  return m_finalizedRecord;
}
const std::string &LightClientHeader::quorumCertificate() const {
  return m_quorumCertificate;
}

bool LightClientHeader::isValid() const {
  return !m_networkName.empty() && !m_chainId.empty() &&
         !m_genesisConfigId.empty() && !m_blockHash.empty() &&
         !m_previousHash.empty() && !m_headerPayload.empty() &&
         !m_validatorSetRoot.empty() && !m_finalizedRecord.empty() &&
         !m_quorumCertificate.empty() && m_timestamp > 0;
}

bool LightClientHeader::headerHashMatches() const {
  return !m_headerPayload.empty() && hashString(m_headerPayload) == m_blockHash;
}

std::string LightClientHeader::serializeJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"networkName\":" << jsonString(m_networkName)
      << ",\"chainId\":" << jsonString(m_chainId)
      << ",\"genesisConfigId\":" << jsonString(m_genesisConfigId)
      << ",\"height\":" << m_height
      << ",\"blockHash\":" << jsonString(m_blockHash)
      << ",\"previousHash\":" << jsonString(m_previousHash)
      << ",\"stateRoot\":" << jsonString(m_stateRoot)
      << ",\"receiptsRoot\":" << jsonString(m_receiptsRoot)
      << ",\"timestamp\":" << m_timestamp
      << ",\"validatorSetRoot\":" << jsonString(m_validatorSetRoot)
      << ",\"headerPayload\":" << jsonString(m_headerPayload)
      << ",\"quorumCertificate\":" << jsonString(m_quorumCertificate)
      << ",\"finalizedRecord\":" << jsonString(m_finalizedRecord)
      << ",\"headerHashVerified\":" << (headerHashMatches() ? "true" : "false")
      << "}";
  return oss.str();
}

LightClientAccountProof::LightClientAccountProof() = default;

LightClientAccountProof::LightClientAccountProof(LightClientHeader header,
                                                 std::string address,
                                                 std::string accountJson,
                                                 std::string accountRoot,
                                                 core::MerkleProof proof)
    : m_header(std::move(header)), m_address(std::move(address)),
      m_accountJson(std::move(accountJson)),
      m_accountRoot(std::move(accountRoot)), m_proof(std::move(proof)) {}

const LightClientHeader &LightClientAccountProof::header() const {
  return m_header;
}
const std::string &LightClientAccountProof::address() const {
  return m_address;
}
const std::string &LightClientAccountProof::accountJson() const {
  return m_accountJson;
}
const std::string &LightClientAccountProof::accountRoot() const {
  return m_accountRoot;
}
const core::MerkleProof &LightClientAccountProof::proof() const {
  return m_proof;
}

bool LightClientAccountProof::isValid() const {
  return m_header.isValid() && !m_address.empty() && !m_accountJson.empty() &&
         !m_accountRoot.empty() && m_proof.isValid();
}

bool LightClientAccountProof::verifies() const {
  return isValid() && m_header.headerHashMatches() &&
         m_proof.verify(m_accountRoot);
}

std::string LightClientAccountProof::serializeJson() const {
  std::ostringstream oss;
  oss << "{\"header\":" << m_header.serializeJson()
      << ",\"address\":" << jsonString(m_address)
      << ",\"account\":" << m_accountJson
      << ",\"accountRoot\":" << jsonString(m_accountRoot)
      << ",\"proof\":" << merkleProofJson(m_proof)
      << ",\"verified\":" << (verifies() ? "true" : "false") << "}";
  return oss.str();
}

LightClientTransactionProof::LightClientTransactionProof() = default;

LightClientTransactionProof::LightClientTransactionProof(
    LightClientHeader header, std::string transactionId, std::string recordJson,
    std::string recordsRoot, core::MerkleProof proof)
    : m_header(std::move(header)), m_transactionId(std::move(transactionId)),
      m_recordJson(std::move(recordJson)),
      m_recordsRoot(std::move(recordsRoot)), m_proof(std::move(proof)) {}

const LightClientHeader &LightClientTransactionProof::header() const {
  return m_header;
}
const std::string &LightClientTransactionProof::transactionId() const {
  return m_transactionId;
}
const std::string &LightClientTransactionProof::recordJson() const {
  return m_recordJson;
}
const std::string &LightClientTransactionProof::recordsRoot() const {
  return m_recordsRoot;
}
const core::MerkleProof &LightClientTransactionProof::proof() const {
  return m_proof;
}

bool LightClientTransactionProof::isValid() const {
  return m_header.isValid() && !m_transactionId.empty() &&
         !m_recordJson.empty() && !m_recordsRoot.empty() && m_proof.isValid();
}

bool LightClientTransactionProof::verifies() const {
  return isValid() && m_header.headerHashMatches() &&
         m_proof.verify(m_recordsRoot);
}

std::string LightClientTransactionProof::serializeJson() const {
  std::ostringstream oss;
  oss << "{\"header\":" << m_header.serializeJson()
      << ",\"transactionId\":" << jsonString(m_transactionId)
      << ",\"record\":" << m_recordJson
      << ",\"recordsRoot\":" << jsonString(m_recordsRoot)
      << ",\"proof\":" << merkleProofJson(m_proof)
      << ",\"verified\":" << (verifies() ? "true" : "false") << "}";
  return oss.str();
}

bool LightClientProtocolVerifier::verifyHeaderChain(
    const std::vector<LightClientHeader> &headers, std::string *reason) {
  if (headers.empty()) {
    if (reason != nullptr)
      *reason = "header range is empty";
    return false;
  }
  for (std::size_t i = 0; i < headers.size(); ++i) {
    const LightClientHeader &header = headers[i];
    if (!header.isValid()) {
      if (reason != nullptr)
        *reason = "invalid light-client header at index " + std::to_string(i);
      return false;
    }
    if (!header.headerHashMatches()) {
      if (reason != nullptr)
        *reason = "block hash does not match header payload at height " +
                  std::to_string(header.height());
      return false;
    }
    if (i > 0) {
      const LightClientHeader &previous = headers[i - 1];
      if (header.height() != previous.height() + 1) {
        if (reason != nullptr)
          *reason = "non-contiguous light-client header range";
        return false;
      }
      if (header.previousHash() != previous.blockHash()) {
        if (reason != nullptr)
          *reason = "broken previous-hash link at height " +
                    std::to_string(header.height());
        return false;
      }
      if (header.chainId() != previous.chainId() ||
          header.genesisConfigId() != previous.genesisConfigId()) {
        if (reason != nullptr)
          *reason = "header range crosses network or genesis boundary";
        return false;
      }
    }
  }
  if (reason != nullptr)
    *reason = "verified";
  return true;
}

} // namespace nodo::node
