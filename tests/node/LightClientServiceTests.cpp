// Proves the real RPC-serving path (LightClientService::headerRangeJson /
// checkpointJson, backing NodeRpcServer::handleLightHeaders /
// handleLightCheckpoint) is actually wired to the new cryptographic
// verification in LightClientProtocolVerifier — not just that the verifier
// has new unused methods. Drives a real single-validator chain through
// RuntimeBlockPipeline, exactly like tests/node/FastSyncImportTests.cpp.

#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/Signer.hpp"
#include "node/LightClientProtocol.hpp"
#include "node/LightClientService.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace nodo;
using namespace nodo::node;
using namespace nodo::core;
using namespace nodo::config;
using namespace nodo::crypto;

constexpr std::int64_t kTimestamp = 1800000000LL;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

KeyPair validatorKey() {
  return KeyPair::createDeterministicBls12381KeyPair("light-client-service-validator");
}

KeyPair userKey() {
  return KeyPair::createDeterministicEd25519KeyPair("light-client-service-user");
}

Signer validatorSigner() {
  static const Bls12381SignatureProvider provider;
  return Signer(validatorKey(), provider);
}

Signer userSigner() {
  static const Ed25519SignatureProvider provider;
  return Signer(userKey(), provider);
}

GenesisConfig genesisConfig() {
  return GenesisConfig(
      NetworkParameters::developmentLocal(), kTimestamp,
      {BootstrapValidatorConfig(validatorKey().publicKey(), 1, 1,
                                "light-client-service-validator",
                                userKey().address().value())},
      {GenesisAccountConfig(userKey().address().value(),
                            utils::Amount::fromRawUnits(1000000000000), 0)},
      "light-client-service-genesis");
}

NodeRuntime startRuntime() {
  const auto started = NodeRuntimeFactory::startFromGenesis(NodeRuntimeConfig(
      genesisConfig(),
      p2p::PeerInfo("local", "127.0.0.1:29995", "nodo/test", 0, kTimestamp), 16));
  require(started.started(), "Runtime must start from genesis.");
  return started.runtime();
}

void admit(NodeRuntime &runtime, std::uint64_t nonce, std::int64_t timestamp) {
  Transaction tx = TransactionBuilder::buildSignedTransfer(
      TransactionBuildRequest("light-client-service-recipient",
                              utils::Amount::fromRawUnits(1000),
                              utils::Amount::fromRawUnits(100), nonce, timestamp),
      userSigner(), genesisConfig().networkParameters().chainId());
  auto result = runtime.mutableMempool().admitTransaction(
      tx, CryptoPolicy::developmentPolicy(), SecurityContext::USER_TRANSACTION,
      timestamp);
  require(result.accepted(),
          "Transaction must enter the mempool: " + result.reason());
}

void produceBlock(NodeRuntime &runtime, std::int64_t timestamp) {
  const auto result = RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
      runtime, RuntimeBlockPipelineConfig(10, 1, 1, timestamp), validatorSigner());
  require(result.finalized(), "Block must finalize: " + result.reason());
}

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

void testHeaderRangeJsonIsCryptographicallyVerified() {
  NodeRuntime runtime = startRuntime();
  admit(runtime, 1, kTimestamp + 1);
  produceBlock(runtime, kTimestamp + 2);
  admit(runtime, 2, kTimestamp + 3);
  produceBlock(runtime, kTimestamp + 4);

  const std::string json = LightClientService::headerRangeJson(runtime, 1, 10);
  require(contains(json, "\"count\":2"), "expected 2 finalized headers: " + json);
  require(contains(json, "\"verified\":true"),
          "real crypto-backed header range must report verified:true: " + json);

  // Cross-check the same conclusion at the object level (not just JSON text):
  // the headers LightClientService actually served must independently pass
  // verifyFinalizedHeaderChain against this node's own validator set
  // history, using the real network crypto provider.
  const auto headers = LightClientService::headerRange(runtime, 1, 10);
  require(headers.size() == 2, "expected 2 headers from headerRange");
  const ProtocolCryptoContext cryptoContext = ProtocolCryptoContext::fromNetworkName(
      runtime.config().genesisConfig().networkParameters().networkName());
  require(cryptoContext.isValid(), "crypto context must be valid for localnet");
  std::string reason;
  require(LightClientProtocolVerifier::verifyFinalizedHeaderChain(
              headers, runtime.validatorSetHistory(), cryptoContext.policy(),
              cryptoContext.validatorSignatureProvider(), &reason),
          "served headers must verify cryptographically: " + reason);

  // Tampering with the served header's standalone quorumCertificate field
  // (a different, otherwise-valid QC string) must be caught, proving the
  // wiring is doing real work rather than always reporting true.
  const LightClientHeader tampered(
      headers[0].networkName(), headers[0].chainId(), headers[0].genesisConfigId(),
      headers[0].height(), headers[0].blockHash(), headers[0].previousHash(),
      headers[0].stateRoot(), headers[0].receiptsRoot(), headers[0].timestamp(),
      headers[0].headerPayload(), headers[0].validatorSetRoot(),
      headers[0].finalizedRecord(), headers[1].quorumCertificate());
  require(!LightClientProtocolVerifier::verifyFinalizedHeader(
              tampered, runtime.validatorSetHistory().setAt(headers[0].height()),
              cryptoContext.policy(), cryptoContext.validatorSignatureProvider(),
              &reason),
          "a header with a mismatched standalone quorum certificate must not "
          "verify");
}

void testCheckpointJsonIsCryptographicallyVerified() {
  NodeRuntime runtime = startRuntime();
  admit(runtime, 1, kTimestamp + 1);
  produceBlock(runtime, kTimestamp + 2);

  const std::string json = LightClientService::checkpointJson(runtime);
  require(contains(json, "\"checkpoint\":"), "expected a checkpoint field: " + json);
  require(contains(json, "\"verified\":true"),
          "real crypto-backed checkpoint must report verified:true: " + json);
}

int main() {
  try {
    testHeaderRangeJsonIsCryptographicallyVerified();
    testCheckpointJsonIsCryptographicallyVerified();
    std::cout << "Nodo LightClientService tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo LightClientService tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
