#include "consensus/V1WeightedProposerSchedule.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

namespace nodo::consensus {

namespace {

using Schedule = V1WeightedProposerSchedule;
using Priority = Schedule::Priority;

bool add(Priority left, Priority right, Priority &result) {
  return !__builtin_add_overflow(left, right, &result);
}

bool subtract(Priority left, Priority right, Priority &result) {
  return !__builtin_sub_overflow(left, right, &result);
}

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

void appendI128(std::vector<unsigned char> &bytes, Priority value) {
  const unsigned __int128 raw = static_cast<unsigned __int128>(value);
  for (int shift = 120; shift >= 0; shift -= 8) {
    bytes.push_back(static_cast<unsigned char>(raw >> shift));
  }
}

} // namespace

V1WeightedProposerSchedule::V1WeightedProposerSchedule(
    std::uint64_t epochLengthBlocks, const Digest &root,
    const std::vector<Validator> &validators, std::uint64_t totalWeight)
    : m_epochLengthBlocks(epochLengthBlocks), m_setRoot(root),
      m_totalWeight(totalWeight), m_validators(validators),
      m_priorities(validators.size(), 0) {}

bool V1WeightedProposerSchedule::validSet(
    const std::vector<Validator> &validators,
    std::uint64_t &totalWeight) {
  if (validators.empty() || validators.size() > kMaxValidators) {
    return false;
  }
  totalWeight = 0;
  for (std::size_t i = 0; i < validators.size(); ++i) {
    const Validator &validator = validators[i];
    if (validator.weight == 0 ||
        (i > 0 && !(validators[i - 1].id < validator.id)) ||
        validator.weight >
            std::numeric_limits<std::uint64_t>::max() - totalWeight) {
      return false;
    }
    totalWeight += validator.weight;
  }
  return totalWeight != 0;
}

std::optional<V1WeightedProposerSchedule>
V1WeightedProposerSchedule::create(
    std::uint64_t epochLengthBlocks, const Digest &genesisSetRoot,
    const std::vector<Validator> &genesisValidators) {
  std::uint64_t totalWeight = 0;
  if (epochLengthBlocks == 0 || genesisValidators.size() < 4 ||
      !validSet(genesisValidators, totalWeight)) {
    return std::nullopt;
  }
  return V1WeightedProposerSchedule(epochLengthBlocks, genesisSetRoot,
                                    genesisValidators, totalWeight);
}

std::optional<V1WeightedProposerSchedule::Ranking>
V1WeightedProposerSchedule::ranking() const {
  Ranking result;
  result.scores.reserve(m_validators.size());
  for (std::size_t i = 0; i < m_validators.size(); ++i) {
    Priority score = 0;
    if (!add(m_priorities[i], static_cast<Priority>(m_validators[i].weight),
             score)) {
      return std::nullopt;
    }
    result.scores.push_back(score);
  }
  result.order.resize(m_validators.size());
  std::iota(result.order.begin(), result.order.end(), 0);
  std::sort(result.order.begin(), result.order.end(),
            [&](std::size_t left, std::size_t right) {
              if (result.scores[left] != result.scores[right]) {
                return result.scores[left] > result.scores[right];
              }
              return m_validators[left].id < m_validators[right].id;
            });
  return result;
}

std::optional<V1WeightedProposerSchedule::Digest>
V1WeightedProposerSchedule::proposer(std::uint64_t height,
                                    std::uint64_t round) const {
  if (height != m_nextHeight) {
    return std::nullopt;
  }
  const auto ranked = ranking();
  if (!ranked) {
    return std::nullopt;
  }
  return m_validators[ranked->order[round % ranked->order.size()]].id;
}

bool V1WeightedProposerSchedule::rebase(
    const std::vector<Validator> &oldValidators,
    const std::vector<Priority> &oldPriorities,
    const std::vector<Validator> &newValidators, std::uint64_t newWeight,
    std::vector<Priority> &result) {
  result.resize(newValidators.size());
  std::vector<bool> carries(newValidators.size(), false);
  std::optional<Priority> lowest;
  for (std::size_t i = 0; i < newValidators.size(); ++i) {
    const auto it = std::lower_bound(
        oldValidators.begin(), oldValidators.end(), newValidators[i].id,
        [](const Validator &entry, const Digest &id) { return entry.id < id; });
    if (it == oldValidators.end() || it->id != newValidators[i].id) {
      continue;
    }
    const std::size_t oldIndex = static_cast<std::size_t>(
        std::distance(oldValidators.begin(), it));
    result[i] = oldPriorities[oldIndex];
    carries[i] = true;
    if (!lowest || result[i] < *lowest) {
      lowest = result[i];
    }
  }
  for (std::size_t i = 0; i < newValidators.size(); ++i) {
    if (!carries[i]) {
      if (lowest && !subtract(*lowest, static_cast<Priority>(newWeight),
                              result[i])) {
        return false;
      }
      if (!lowest) {
        result[i] = 0;
      }
    }
  }

  return normalize(result, newWeight);
}

bool V1WeightedProposerSchedule::normalize(
    std::vector<Priority> &priorities, std::uint64_t totalWeight) {
  const auto [minimum, maximum] =
      std::minmax_element(priorities.begin(), priorities.end());
  Priority range = 0;
  if (!subtract(*maximum, *minimum, range)) {
    return false;
  }
  const Priority threshold = static_cast<Priority>(totalWeight) * 2;
  if (range > threshold) {
    const Priority divisor = range / threshold + (range % threshold != 0);
    for (Priority &priority : priorities) {
      priority /= divisor; // Signed integer division truncates toward zero.
    }
  }

  Priority sum = 0;
  for (const Priority priority : priorities) {
    if (!add(sum, priority, sum)) {
      return false;
    }
  }
  const Priority average = sum / static_cast<Priority>(priorities.size());
  for (Priority &priority : priorities) {
    if (!subtract(priority, average, priority)) {
      return false;
    }
  }
  sum = 0;
  for (const Priority priority : priorities) {
    if (!add(sum, priority, sum)) {
      return false;
    }
  }
  const Priority direction = sum > 0 ? -1 : 1;
  const std::size_t remainder = static_cast<std::size_t>(sum > 0 ? sum : -sum);
  if (remainder >= priorities.size()) {
    return false;
  }
  for (std::size_t i = 0; i < remainder; ++i) {
    if (!add(priorities[i], direction, priorities[i])) {
      return false;
    }
  }
  sum = 0;
  for (const Priority priority : priorities) {
    if (!add(sum, priority, sum)) {
      return false;
    }
  }
  const auto [finalMinimum, finalMaximum] =
      std::minmax_element(priorities.begin(), priorities.end());
  return sum == 0 &&
         subtract(*finalMaximum, *finalMinimum, range) &&
         range <= threshold + 1;
}

bool V1WeightedProposerSchedule::finalize(
    std::uint64_t height, std::uint64_t round, const Digest &signedProposer,
    const std::optional<NextSet> &nextSet) {
  if (height != m_nextHeight ||
      height == std::numeric_limits<std::uint64_t>::max() ||
      (nextSet && height % m_epochLengthBlocks != 0)) {
    return false;
  }
  std::uint64_t nextWeight = 0;
  if (nextSet && !validSet(nextSet->validators, nextWeight)) {
    return false;
  }
  std::vector<Validator> nextValidators;
  if (nextSet) {
    nextValidators = nextSet->validators;
  }
  const auto ranked = ranking();
  if (!ranked || signedProposer !=
                     m_validators[ranked->order[round % ranked->order.size()]]
                         .id) {
    return false;
  }
  std::vector<Priority> nextPriorities = ranked->scores;
  const std::size_t primary = ranked->order.front();
  if (!subtract(nextPriorities[primary],
                static_cast<Priority>(m_totalWeight),
                nextPriorities[primary])) {
    return false;
  }
  if (nextSet) {
    if (nextSet->validators != m_validators) {
      std::vector<Priority> rebased;
      if (!rebase(m_validators, nextPriorities, nextSet->validators, nextWeight,
                  rebased)) {
        return false;
      }
      nextPriorities = std::move(rebased);
    } else if (!normalize(nextPriorities, nextWeight)) {
      return false;
    }
  } else if (!normalize(nextPriorities, m_totalWeight)) {
    return false;
  }
  m_priorities = std::move(nextPriorities);
  if (nextSet) {
    m_validators = std::move(nextValidators);
    m_setRoot = nextSet->root;
    m_totalWeight = nextWeight;
  }
  ++m_nextHeight;
  return true;
}

std::vector<unsigned char>
V1WeightedProposerSchedule::stateValueBytes() const {
  std::vector<unsigned char> bytes;
  bytes.reserve(32 + 8 + 8 + 4 + m_validators.size() * (4 + 32 + 16));
  bytes.insert(bytes.end(), m_setRoot.begin(), m_setRoot.end());
  appendU64(bytes, m_nextHeight - 1);
  appendU64(bytes, m_totalWeight);
  appendU32(bytes, static_cast<std::uint32_t>(m_validators.size()));
  for (std::size_t i = 0; i < m_validators.size(); ++i) {
    appendU32(bytes, 48);
    bytes.insert(bytes.end(), m_validators[i].id.begin(),
                 m_validators[i].id.end());
    appendI128(bytes, m_priorities[i]);
  }
  return bytes;
}

} // namespace nodo::consensus
