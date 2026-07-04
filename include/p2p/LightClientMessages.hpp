#ifndef NODO_P2P_LIGHT_CLIENT_MESSAGES_HPP
#define NODO_P2P_LIGHT_CLIENT_MESSAGES_HPP

#include <cstdint>
#include <string>

namespace nodo::p2p {

/*
 * LightClientProofRequest is sent by a light client to a full node asking for
 * either a transaction inclusion proof or an account state proof at a given
 * block height.
 */
struct LightClientProofRequest {
  std::string requestId;
  std::string transactionId; // filled for tx inclusion request
  std::string address;       // filled for account state request
  std::uint64_t blockHeight;
  bool isTransactionRequest; // true = tx inclusion, false = account state

  bool isValid() const;
  std::string serialize() const;
  static LightClientProofRequest deserialize(const std::string &s);
};

/*
 * LightClientProofResponse carries the proof payload back to the light client.
 * On success, proofPayload holds the serialized TransactionInclusionProof or
 * AccountStateProof. On failure, reason explains why the proof is unavailable.
 */
struct LightClientProofResponse {
  std::string requestId;
  bool found;
  std::string proofPayload; // serialized proof, or empty if !found
  std::string reason;       // error reason if !found

  bool isValid() const;
  std::string serialize() const;
  static LightClientProofResponse deserialize(const std::string &s);
};

} // namespace nodo::p2p

#endif
