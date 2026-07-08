#include "../common/ConsensusPhaseTestFixtures.hpp"
#include "config/NetworkParameters.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/ConsensusEventLoop.hpp"
#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/SlashingEvidenceMessages.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <iostream>

using namespace nodo;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

int main() {
  std::cout << "OK" << std::endl;
  return 0;
}
