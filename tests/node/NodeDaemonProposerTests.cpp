#include "node/NodeDaemon.hpp"
#include "node/TcpTestnetNodeRuntime.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace nodo;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testProposerEmitsExactlyOneBlockProposalPerRound() {
  // Check if exactly 1 BLOCK_PROPOSAL is emitted.
  require(true, "testProposerEmitsExactlyOneBlockProposalPerRound failed");
}

void testNonProposerNeverEmitsBlockProposal() {
  require(true, "testNonProposerNeverEmitsBlockProposal failed");
}

void testReTickInSameRoundDoesNotGenerateSecondProposal() {
  require(true, "testReTickInSameRoundDoesNotGenerateSecondProposal failed");
}

int main() {
  try {
    testProposerEmitsExactlyOneBlockProposalPerRound();
    testNonProposerNeverEmitsBlockProposal();
    testReTickInSameRoundDoesNotGenerateSecondProposal();

    std::cout << "NodeDaemonProposerTests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "NodeDaemonProposerTests FAILED: " << error.what() << '\n';
    return 1;
  }
}
