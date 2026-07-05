#ifndef NODO_CORE_STATE_ROOT_CALCULATOR_HPP
#define NODO_CORE_STATE_ROOT_CALCULATOR_HPP

#include "core/AccountStateView.hpp"
#include "core/MerkleTree.hpp"
#include "core/ValidatorRegistry.hpp"

#include <map>
#include <string>

namespace nodo::core {

class StateRootCalculator {
public:
  static std::string calculateAccountStateRoot(const AccountStateView &view);

  static std::string canonicalAccountStatePayload(const AccountStateView &view);

  static std::string
  calculateValidatorStateRoot(const ValidatorRegistry &registry);

  static std::string calculateProtocolStateRoot(
      const AccountStateView &view,
      const std::map<std::string, std::string> &deterministicDomains);

  /*
   * Builds a Merkle inclusion proof for a single address against the
   * account-state root returned by calculateAccountStateRoot (the
   * "accounts" domain root, not the full protocol state root). Returns an
   * invalid (empty leafHash) MerkleProof if the view is invalid or the
   * address is not present in it.
   */
  static MerkleProof accountInclusionProof(const AccountStateView &view,
                                           const std::string &address);
};

} // namespace nodo::core

#endif
