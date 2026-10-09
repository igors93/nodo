#include "consensus/V1LivenessAccountability.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace nodo::consensus {

namespace {

void appendU64(std::vector<unsigned char> &bytes, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    bytes.push_back(static_cast<unsigned char>(value >> shift));
  }
}

void appendU32(std::vector<unsigned char> &bytes, std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8) {
    bytes.push_back(static_cast<unsigned char>(value >> shift));
  }
}

} // namespace

V1LivenessAccountability::V1LivenessAccountability(
    std::uint64_t epochLengthBlocks, const Digest &root,
    const std::vector<Validator> &validators, std::uint64_t totalWeight)
    : m_epochLengthBlocks(epochLengthBlocks), m_totalWeight(totalWeight),
      m_setRoot(root), m_validators(validators), m_signed(validators.size(), 0) {}

bool V1LivenessAccountability::validSet(
    const std::vector<Validator> &validators, std::uint64_t &totalWeight) {
  if (validators.size() < 4 || validators.size() > kMaxValidators) {
    return false;
  }
  totalWeight = 0;
  for (std::size_t i = 0; i < validators.size(); ++i) {
    const Validator &entry = validators[i];
    if (entry.weight == 0 ||
        (i != 0 && !(validators[i - 1].id < entry.id)) ||
        entry.weight > std::numeric_limits<std::uint64_t>::max() - totalWeight) {
      return false;
    }
    totalWeight += entry.weight;
  }
  return true;
}

std::optional<std::vector<std::size_t>>
V1LivenessAccountability::verifiedSignerIndexes(
    const std::vector<Digest> &signers,
    const std::vector<Validator> &validators, std::uint64_t totalWeight) {
  std::vector<std::size_t> indexes;
  indexes.reserve(signers.size());
  std::size_t validatorIndex = 0;
  std::uint64_t signedWeight = 0;
  for (const Digest &signer : signers) {
    if (!indexes.empty() && !(validators[indexes.back()].id < signer)) {
      return std::nullopt;
    }
    while (validatorIndex < validators.size() &&
           validators[validatorIndex].id < signer) {
      ++validatorIndex;
    }
    if (validatorIndex == validators.size() ||
        validators[validatorIndex].id != signer ||
        validators[validatorIndex].weight >
            std::numeric_limits<std::uint64_t>::max() - signedWeight) {
      return std::nullopt;
    }
    signedWeight += validators[validatorIndex].weight;
    indexes.push_back(validatorIndex);
  }
  const std::uint64_t required = totalWeight - (totalWeight - 1) / 3;
  if (signedWeight < required) {
    return std::nullopt;
  }
  return indexes;
}

std::optional<V1LivenessAccountability> V1LivenessAccountability::create(
    std::uint64_t epochLengthBlocks, const Digest &genesisSetRoot,
    const std::vector<Validator> &genesisValidators) {
  std::uint64_t totalWeight = 0;
  // ADR 0007 implies L >= 288 because T <= 300 and L*T >= 86400.
  if (epochLengthBlocks < 288 || epochLengthBlocks > 604'800 ||
      !validSet(genesisValidators, totalWeight)) {
    return std::nullopt;
  }
  return V1LivenessAccountability(epochLengthBlocks, genesisSetRoot,
                                  genesisValidators, totalWeight);
}

std::optional<V1LivenessAccountability::Transition>
V1LivenessAccountability::advance(
    std::uint64_t height, const std::optional<ParentCommit> &parent,
    const std::optional<NextSet> &nextSet) {
  if (height != m_nextHeight ||
      height == std::numeric_limits<std::uint64_t>::max() ||
      (height == 1) == parent.has_value() ||
      (nextSet && height % m_epochLengthBlocks != 0)) {
    return std::nullopt;
  }
  if (parent && parent->height != height - 1) {
    return std::nullopt;
  }

  const bool firstOfEpoch = height > 1 &&
      (height - 1) % m_epochLengthBlocks == 0;
  std::vector<std::uint64_t> nextSigned = m_signed;
  std::uint64_t nextObserved = m_observed;
  if (firstOfEpoch) {
    if (!m_previousBoundaryRoot || parent->setRoot != *m_previousBoundaryRoot ||
        !verifiedSignerIndexes(parent->signers, m_previousBoundaryValidators,
                               m_previousBoundaryWeight)) {
      return std::nullopt;
    }
    // The old boundary's QC was not known when its header committed the
    // following set, so it cannot contribute to either epoch's decision.
  } else if (parent) {
    if (parent->setRoot != m_setRoot ||
        nextObserved >= m_epochLengthBlocks - 1) {
      return std::nullopt;
    }
    const auto signerIndexes =
        verifiedSignerIndexes(parent->signers, m_validators, m_totalWeight);
    if (!signerIndexes) {
      return std::nullopt;
    }
    for (std::size_t index : *signerIndexes) {
      if (nextSigned[index] ==
              std::numeric_limits<std::uint64_t>::max()) {
        return std::nullopt;
      }
      ++nextSigned[index];
    }
    ++nextObserved;
  }

  Transition result{height % m_epochLengthBlocks == 0, {}};
  std::uint64_t nextWeight = m_totalWeight;
  std::vector<Validator> nextValidators;
  std::vector<Validator> previousValidators;
  std::vector<std::uint64_t> resetSigned;
  if (result.boundary) {
    previousValidators = m_validators;
    if (nextObserved != m_epochLengthBlocks - 1) {
      return std::nullopt;
    }
    result.assessments.reserve(m_validators.size());
    for (std::size_t i = 0; i < m_validators.size(); ++i) {
      result.assessments.push_back(
          {m_validators[i].id, nextSigned[i], nextObserved,
           static_cast<unsigned __int128>(nextSigned[i]) * 4 <
               static_cast<unsigned __int128>(nextObserved) * 3});
    }
    if (nextSet) {
      if (!validSet(nextSet->validators, nextWeight)) {
        return std::nullopt;
      }
      nextValidators = nextSet->validators;
    }
    resetSigned.resize(nextSet ? nextValidators.size() : m_validators.size(), 0);
  }

  // All fallible validation and allocations finish before state is mutated.
  if (result.boundary) {
    m_previousBoundaryRoot = m_setRoot;
    m_previousBoundaryValidators = std::move(previousValidators);
    m_previousBoundaryWeight = m_totalWeight;
    if (nextSet) {
      m_setRoot = nextSet->root;
      m_validators = std::move(nextValidators);
      m_totalWeight = nextWeight;
    }
    m_signed = std::move(resetSigned);
    m_observed = 0;
    ++m_epoch;
  } else {
    m_signed = std::move(nextSigned);
    m_observed = nextObserved;
    if (firstOfEpoch) {
      m_previousBoundaryRoot.reset();
      m_previousBoundaryValidators.clear();
      m_previousBoundaryWeight = 0;
    }
  }
  ++m_nextHeight;
  return result;
}

std::vector<unsigned char> V1LivenessAccountability::stateValueBytes() const {
  std::vector<unsigned char> bytes;
  bytes.reserve(32 + 8 + 8 + 8 + 8 + 4 + m_validators.size() * 52);
  bytes.insert(bytes.end(), m_setRoot.begin(), m_setRoot.end());
  appendU64(bytes, m_nextHeight - 1);
  appendU64(bytes, m_epoch);
  appendU64(bytes, m_observed);
  appendU64(bytes, m_totalWeight);
  appendU32(bytes, static_cast<std::uint32_t>(m_validators.size()));
  for (std::size_t i = 0; i < m_validators.size(); ++i) {
    appendU32(bytes, 48);
    bytes.insert(bytes.end(), m_validators[i].id.begin(),
                 m_validators[i].id.end());
    appendU64(bytes, m_validators[i].weight);
    appendU64(bytes, m_signed[i]);
  }
  return bytes;
}

} // namespace nodo::consensus
