#ifndef NODO_NODE_LIGHT_CLIENT_PROTOCOL_HPP
#define NODO_NODE_LIGHT_CLIENT_PROTOCOL_HPP

#include "core/MerkleTree.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class LightClientHeader {
public:
  LightClientHeader();
  LightClientHeader(std::string networkName, std::string chainId,
                    std::string genesisConfigId, std::uint64_t height,
                    std::string blockHash, std::string previousHash,
                    std::string stateRoot, std::string receiptsRoot,
                    std::int64_t timestamp, std::string headerPayload,
                    std::string validatorSetRoot, std::string finalizedRecord,
                    std::string quorumCertificate);

  const std::string &networkName() const;
  const std::string &chainId() const;
  const std::string &genesisConfigId() const;
  std::uint64_t height() const;
  const std::string &blockHash() const;
  const std::string &previousHash() const;
  const std::string &stateRoot() const;
  const std::string &receiptsRoot() const;
  std::int64_t timestamp() const;
  const std::string &headerPayload() const;
  const std::string &validatorSetRoot() const;
  const std::string &finalizedRecord() const;
  const std::string &quorumCertificate() const;

  bool isValid() const;
  bool headerHashMatches() const;
  std::string serializeJson() const;

private:
  std::string m_networkName;
  std::string m_chainId;
  std::string m_genesisConfigId;
  std::uint64_t m_height;
  std::string m_blockHash;
  std::string m_previousHash;
  std::string m_stateRoot;
  std::string m_receiptsRoot;
  std::int64_t m_timestamp;
  std::string m_headerPayload;
  std::string m_validatorSetRoot;
  std::string m_finalizedRecord;
  std::string m_quorumCertificate;
};

class LightClientAccountProof {
public:
  LightClientAccountProof();
  LightClientAccountProof(LightClientHeader header, std::string address,
                          std::string accountJson, std::string accountRoot,
                          core::MerkleProof proof);

  const LightClientHeader &header() const;
  const std::string &address() const;
  const std::string &accountJson() const;
  const std::string &accountRoot() const;
  const core::MerkleProof &proof() const;

  bool isValid() const;
  bool verifies() const;
  std::string serializeJson() const;

private:
  LightClientHeader m_header;
  std::string m_address;
  std::string m_accountJson;
  std::string m_accountRoot;
  core::MerkleProof m_proof;
};

class LightClientTransactionProof {
public:
  LightClientTransactionProof();
  LightClientTransactionProof(LightClientHeader header,
                              std::string transactionId, std::string recordJson,
                              std::string recordsRoot, core::MerkleProof proof);

  const LightClientHeader &header() const;
  const std::string &transactionId() const;
  const std::string &recordJson() const;
  const std::string &recordsRoot() const;
  const core::MerkleProof &proof() const;

  bool isValid() const;
  bool verifies() const;
  std::string serializeJson() const;

private:
  LightClientHeader m_header;
  std::string m_transactionId;
  std::string m_recordJson;
  std::string m_recordsRoot;
  core::MerkleProof m_proof;
};

class LightClientProtocolVerifier {
public:
  static bool verifyHeaderChain(const std::vector<LightClientHeader> &headers,
                                std::string *reason = nullptr);
};

} // namespace nodo::node

#endif
