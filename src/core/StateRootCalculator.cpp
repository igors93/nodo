#include "core/StateRootCalculator.hpp"

#include "core/MerkleTree.hpp"
#include "core/SparseMerkleTree.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/hash.h"
#include "serialization/CanonicalWriter.hpp"

#include <sstream>
#include <vector>

namespace nodo::core {

namespace {

std::string accountSmtKey(const std::string &address) {
  char addressHash[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_string(address.c_str(), addressHash, sizeof(addressHash));
  return std::string(addressHash);
}

std::string accountLeafPayload(const AccountState &account) {
  std::ostringstream oss;
  oss << "NODO_ACCOUNT_LEAF_V2{"
      << "address=" << account.address()
      << ";balance=" << account.balance().rawUnits()
      << ";nonce=" << account.nonce() << "}";
  return oss.str();
}

SparseMerkleTree buildAccountStateTree(const AccountStateView &view) {
  SparseMerkleTree smt;
  for (const auto &account : view.accounts()) {
    smt.update(accountSmtKey(account.address()),
               MerkleTree::hashLeaf(accountLeafPayload(account)));
  }
  return smt;
}

std::string validatorSmtKey(const std::string &address) {
  char addressHash[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_string(address.c_str(), addressHash, sizeof(addressHash));
  return std::string(addressHash);
}

std::string validatorLeafPayload(const ValidatorRegistryEntry &entry) {
  std::ostringstream oss;
  oss << "NODO_VALIDATOR_STAKE_LEAF_V1{"
      << "address=" << entry.registrationRecord().validatorAddress()
      << ";weight=" << entry.consensusWeight() << "}";
  return oss.str();
}

SparseMerkleTree buildValidatorStateTree(const ValidatorRegistry &registry) {
  SparseMerkleTree smt;
  for (const auto &address : registry.validatorAddresses()) {
    const auto *entry = registry.entryForAddress(address);
    smt.update(validatorSmtKey(address),
               MerkleTree::hashLeaf(validatorLeafPayload(*entry)));
  }
  return smt;
}

} // namespace

std::string
StateRootCalculator::calculateAccountStateRoot(const AccountStateView &view) {
  if (!view.isValid()) {
    return "";
  }

  return buildAccountStateTree(view).root();
}

std::string StateRootCalculator::canonicalAccountStatePayload(
    const AccountStateView &view) {
  if (!view.isValid()) {
    return "";
  }

  std::ostringstream oss;
  oss << "NODO_ACCOUNT_STATE_ROOT_V2{";

  const std::vector<AccountState> accounts = view.accounts();
  oss << "accountCount=" << accounts.size()
      << ";merkleRoot=" << calculateAccountStateRoot(view) << "}";

  return oss.str();
}

std::string StateRootCalculator::calculateValidatorStateRoot(
    const ValidatorRegistry &registry) {
  if (!registry.isValid()) {
    return "";
  }

  return buildValidatorStateTree(registry).root();
}

std::string StateRootCalculator::calculateProtocolStateRoot(
    const AccountStateView &view,
    const std::map<std::string, std::string> &deterministicDomains) {
  const std::string accountRoot = calculateAccountStateRoot(view);
  if (accountRoot.empty()) {
    return "";
  }
  if (deterministicDomains.empty()) {
    return accountRoot;
  }

  std::vector<std::string> leaves;
  serialization::CanonicalWriter accountLeaf;
  accountLeaf.writeString("NODO_STATE_DOMAIN_V2");
  accountLeaf.writeString("accounts");
  accountLeaf.writeString(accountRoot);
  leaves.push_back(accountLeaf.byteString());

  for (const auto &[domain, payload] : deterministicDomains) {
    if (domain.empty() || domain == "accounts" || payload.empty()) {
      return "";
    }

    serialization::CanonicalWriter domainLeaf;
    domainLeaf.writeString("NODO_STATE_DOMAIN_V2");
    domainLeaf.writeString(domain);
    domainLeaf.writeString(payload);
    leaves.push_back(domainLeaf.byteString());
  }
  return MerkleTree::buildRoot(leaves);
}

MerkleProof
StateRootCalculator::accountInclusionProof(const AccountStateView &view,
                                           const std::string &address) {
  if (!view.isValid() || !view.hasAccount(address)) {
    return MerkleProof();
  }

  return buildAccountStateTree(view).generateProof(accountSmtKey(address));
}

} // namespace nodo::core
