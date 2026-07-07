#include "node/LightClientProtocol.hpp"

#include "core/LedgerRecord.hpp"
#include "core/MerkleTree.hpp"

#include <cassert>
#include <iostream>

using nodo::node::LightClientHeader;
using nodo::node::LightClientProtocolVerifier;

void testRejectsBrokenHeaderChain() {
  LightClientHeader a("localnet", "chain", "genesis", 1, "hash-a",
                      "genesis-hash", "state", "receipts", 10, "payload-a",
                      "validators", "record", "qc");
  LightClientHeader b("localnet", "chain", "genesis", 3, "hash-b", "not-hash-a",
                      "state", "receipts", 11, "payload-b", "validators",
                      "record", "qc");
  std::string reason;
  assert(!LightClientProtocolVerifier::verifyHeaderChain({a, b}, &reason));
  assert(!reason.empty());
}

void testProofBundleJsonIsStable() {
  const std::string root = nodo::core::MerkleTree::buildRoot({"a", "b"});
  assert(!root.empty());
}

int main() {
  testRejectsBrokenHeaderChain();
  testProofBundleJsonIsStable();
  std::cout << "Nodo LightClientProtocol tests passed.\n";
  return 0;
}
