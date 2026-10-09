#include "economics/V1ResourceFee.hpp"

#include <algorithm>
#include <limits>

namespace nodo::economics {

namespace {

constexpr std::uint64_t kMinBlockUnits = 5'000'000;
constexpr std::uint64_t kMinTxUnits = 2'048;
constexpr std::uint32_t kMaxReceiptBytes = 65'536;
constexpr std::uint32_t kMaxEvidenceBytes = 262'144;
constexpr std::uint32_t kMaxInputLots = 128;
constexpr std::uint32_t kMaxNewLots = 2;
constexpr std::uint32_t kMaxEffectIds = 256;
constexpr std::uint32_t kMaxSystemEffectIds = 2045;
constexpr std::uint32_t kEmptyReceiptBytes = 92;
constexpr std::uint64_t kSignatureUnits = 1'024;
constexpr std::uint64_t kInputLotUnits = 128;
constexpr std::uint64_t kNewLotUnits = 256;
constexpr std::uint64_t kEffectUnits = 64;
constexpr unsigned __int128 kMaxU64 =
    std::numeric_limits<std::uint64_t>::max();

} // namespace

bool V1ResourceFee::validParameters(const Parameters &parameters) {
  if (parameters.maxTxBytes < 256 || parameters.maxTxBytes > 262'144 ||
      parameters.maxBlockBytes > 1'048'576 ||
      parameters.maxBlockBytes < parameters.maxTxBytes + 16 ||
      parameters.maxTxUnits < kMinTxUnits ||
      parameters.maxTxUnits > kMaxTxUnits ||
      parameters.maxTxUnits <
          static_cast<std::uint64_t>(parameters.maxTxBytes) + 2'048 ||
      parameters.maxBlockUnits < kMinBlockUnits ||
      parameters.maxBlockUnits > kMaxBlockUnits ||
      parameters.maxTxUnits > parameters.maxBlockUnits / 2 ||
      parameters.feePerUnitFloor == 0) {
    return false;
  }
  const unsigned __int128 maximumRequiredFee =
      static_cast<unsigned __int128>(parameters.feePerUnitFloor) *
          parameters.maxTxUnits +
      parameters.feeBase;
  return maximumRequiredFee <= kMaxU64;
}

std::optional<std::uint64_t>
V1ResourceFee::typeSurcharge(std::uint8_t type) {
  switch (type) {
  case 1: return 256;   // TRANSFER
  case 2: return 128;   // BURN
  case 3: return 512;   // STAKE_DEPOSIT
  case 4: return 512;   // STAKE_UNLOCK
  case 5: return 512;   // STAKE_WITHDRAW
  case 6: return 512;   // STAKE_TOP_UP
  case 7: return 1'024; // VALIDATOR_REGISTER
  case 8: return 384;   // VALIDATOR_EXIT_REQUEST
  case 9: return 384;   // VALIDATOR_UNJAIL_REQUEST
  case 10: return 768;  // VALIDATOR_KEY_ROTATE
  case 11: return 768;  // GOVERNANCE_PROPOSE
  case 12: return 512;  // GOVERNANCE_VOTE
  case 13: return 1'536; // GOVERNANCE_EXECUTE
  }
  return std::nullopt;
}

std::optional<std::uint64_t> V1ResourceFee::transactionUnits(
    const Parameters &parameters, const TransactionWork &work) {
  const auto surcharge = typeSurcharge(work.type);
  if (!validParameters(parameters) || !surcharge ||
      work.transactionBytes < 8 ||
      work.transactionBytes > parameters.maxTxBytes ||
      work.receiptBytes > kMaxReceiptBytes ||
      work.inputLots > kMaxInputLots || work.newLots > kMaxNewLots ||
      work.effectIds > kMaxEffectIds ||
      work.receiptBytes != kEmptyReceiptBytes + 32 * work.effectIds) {
    return std::nullopt;
  }
  const std::uint64_t units =
      static_cast<std::uint64_t>(work.transactionBytes) + work.receiptBytes +
      kSignatureUnits + kInputLotUnits * work.inputLots +
      kNewLotUnits * work.newLots + kEffectUnits * work.effectIds +
      *surcharge;
  if (units > parameters.maxTxUnits) {
    return std::nullopt;
  }
  return units;
}

std::optional<std::uint64_t>
V1ResourceFee::evidenceUnits(std::uint32_t evidenceBytes) {
  if (evidenceBytes < 8 || evidenceBytes > kMaxEvidenceBytes) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(evidenceBytes) + 2 * kSignatureUnits +
         512;
}

std::optional<std::uint64_t>
V1ResourceFee::systemRecordUnits(const SystemRecordWork &work) {
  if (work.nestedRecordBytes < 4 ||
      work.nestedRecordBytes > 1'048'576 ||
      work.receiptBytes > kMaxReceiptBytes ||
      work.effectIds > kMaxSystemEffectIds ||
      work.receiptBytes != kEmptyReceiptBytes + 32 * work.effectIds) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(work.nestedRecordBytes) +
         work.receiptBytes + kSignatureUnits +
         kEffectUnits * work.effectIds;
}

std::optional<std::uint64_t> V1ResourceFee::livenessRecordUnits(
    const SystemRecordWork &work, std::uint32_t accountedValidators) {
  if (accountedValidators < 4 || accountedValidators > 9'619 ||
      work.nestedRecordBytes != 105 || work.receiptBytes != 124 ||
      work.effectIds != 1) {
    return std::nullopt;
  }
  const auto base = systemRecordUnits(work);
  if (!base) {
    return std::nullopt;
  }
  return *base + 128ULL * accountedValidators;
}

std::optional<std::uint64_t> V1ResourceFee::blockUnits(
    const Parameters &parameters, std::uint32_t bodyBytes,
    std::span<const std::uint64_t> transactions,
    std::span<const std::uint64_t> evidence,
    std::span<const std::uint64_t> systemRecords) {
  if (!validParameters(parameters) || bodyBytes < 20 ||
      bodyBytes > parameters.maxBlockBytes ||
      transactions.size() > 4096 ||
      evidence.size() > kMaxEvidencePerBlock ||
      systemRecords.size() > kMaxSystemRecordsPerBlock) {
    return std::nullopt;
  }
  std::uint64_t userUnits = 0;
  for (const std::uint64_t units : transactions) {
    if (units == 0 || units > parameters.maxTxUnits ||
        units > parameters.maxBlockUnits - userUnits) {
      return std::nullopt;
    }
    userUnits += units;
  }
  if (userUnits > 3 * parameters.maxBlockUnits / 4) {
    return std::nullopt;
  }
  std::uint64_t total = userUnits;
  for (const auto group : {evidence, systemRecords}) {
    for (const std::uint64_t units : group) {
      if (units == 0 || units > parameters.maxBlockUnits - total) {
        return std::nullopt;
      }
      total += units;
    }
  }
  return total;
}

std::optional<std::uint64_t> V1ResourceFee::nextBaseFee(
    std::uint64_t parentBaseFee, std::uint64_t parentUsedUnits,
    std::uint64_t parentMaxBlockUnits, std::uint64_t childFloor) {
  if (parentBaseFee == 0 || childFloor == 0 ||
      parentMaxBlockUnits < kMinBlockUnits ||
      parentMaxBlockUnits > kMaxBlockUnits ||
      parentUsedUnits > parentMaxBlockUnits) {
    return std::nullopt;
  }
  const std::uint64_t target = parentMaxBlockUnits / 2;
  std::uint64_t next = parentBaseFee;
  if (parentUsedUnits > target) {
    const std::uint64_t excess = parentUsedUnits - target;
    const unsigned __int128 rawDelta =
        static_cast<unsigned __int128>(parentBaseFee) * excess /
        (static_cast<unsigned __int128>(target) * 8);
    const unsigned __int128 candidate =
        static_cast<unsigned __int128>(parentBaseFee) +
        std::max<unsigned __int128>(1, rawDelta);
    next = candidate > kMaxU64 ? std::numeric_limits<std::uint64_t>::max()
                               : static_cast<std::uint64_t>(candidate);
  } else if (parentUsedUnits < target) {
    const std::uint64_t deficit = target - parentUsedUnits;
    const std::uint64_t delta = static_cast<std::uint64_t>(
        static_cast<unsigned __int128>(parentBaseFee) * deficit /
        (static_cast<unsigned __int128>(target) * 8));
    next = parentBaseFee - delta;
  }
  return std::max(next, childFloor);
}

std::optional<std::uint64_t> V1ResourceFee::minimumFee(
    const Parameters &parameters, std::uint64_t baseFeePerUnit,
    std::uint64_t units) {
  if (!validParameters(parameters) ||
      baseFeePerUnit < parameters.feePerUnitFloor || units == 0 ||
      units > parameters.maxTxUnits) {
    return std::nullopt;
  }
  const unsigned __int128 required =
      static_cast<unsigned __int128>(baseFeePerUnit) * units +
      parameters.feeBase;
  if (required > kMaxU64) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(required);
}

} // namespace nodo::economics
