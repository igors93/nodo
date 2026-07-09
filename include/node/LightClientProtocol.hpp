#ifndef NODO_NODE_LIGHT_CLIENT_PROTOCOL_HPP
#define NODO_NODE_LIGHT_CLIENT_PROTOCOL_HPP

#include "consensus/BlockFinalizer.hpp"
#include "core/MerkleTree.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

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
  // Structural verification only: field presence, header-payload hash,
  // height contiguity, previousHash linkage, and network/genesis identity.
  // Does not look inside quorumCertificate/finalizedRecord/validatorSetRoot
  // at all — a header with a garbage or mismatched QC still passes this on
  // its own. Kept for cheap pre-filtering and reused internally by
  // verifyFinalizedHeaderChain below.
  static bool verifyHeaderChain(const std::vector<LightClientHeader> &headers,
                                std::string *reason = nullptr);

  // Full verification of one finalized header: deserializes its embedded
  // FinalizedBlockRecord/QuorumCertificate (rejecting malformed data rather
  // than throwing), cross-checks that the header's standalone
  // quorumCertificate/validatorSetRoot fields agree with what is embedded in
  // finalizedRecord, that the record identifies this exact header
  // (height/blockHash/previousHash), and then cryptographically verifies
  // validator signatures, voting weight and validator_set_root against
  // validatorRegistryAtHeight — which must be the validator set that was
  // actually active at header.height(), never a "current" or unrelated
  // registry.
  static bool verifyFinalizedHeader(const LightClientHeader &header,
                                    const core::ValidatorRegistry
                                        &validatorRegistryAtHeight,
                                    const crypto::CryptoPolicy &policy,
                                    const crypto::SignatureProvider &provider,
                                    std::string *reason = nullptr);

  // verifyHeaderChain (hash-chain) plus verifyFinalizedHeader for every
  // header, each checked against the validator set recorded for ITS OWN
  // height in validatorSetHistory. This is what makes a validator-set
  // change partway through the range safe: every header is verified against
  // the set that actually produced its quorum certificate, not one shared
  // registry that would silently mis-verify headers on either side of a
  // transition.
  static bool
  verifyFinalizedHeaderChain(const std::vector<LightClientHeader> &headers,
                             const core::ValidatorSetHistory
                                 &validatorSetHistory,
                             const crypto::CryptoPolicy &policy,
                             const crypto::SignatureProvider &provider,
                             std::string *reason = nullptr);
};

} // namespace nodo::node

#endif
