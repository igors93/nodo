#ifndef NODO_ECONOMICS_V1_RESOURCE_FEE_HPP
#define NODO_ECONOMICS_V1_RESOURCE_FEE_HPP

#include <cstdint>
#include <optional>
#include <span>

namespace nodo::economics {

// Checked reference for the future v1 consensus resource and fee rules.
// Inputs must come from a validated typed v1 codec and deterministic execution;
// this is not a compatibility adapter for the nodo/0.7 fee split.
class V1ResourceFee {
public:
  struct Parameters {
    std::uint32_t maxTxBytes;
    std::uint32_t maxBlockBytes;
    std::uint64_t maxTxUnits;
    std::uint64_t maxBlockUnits;
    std::uint64_t feeBase;
    std::uint64_t feePerUnitFloor;
  };

  struct TransactionWork {
    std::uint8_t type;
    std::uint32_t transactionBytes;
    std::uint32_t receiptBytes;
    std::uint32_t inputLots;
    std::uint32_t newLots;
    std::uint32_t effectIds;
  };

  struct SystemRecordWork {
    std::uint32_t nestedRecordBytes;
    std::uint32_t receiptBytes;
    std::uint32_t effectIds;
  };

  static constexpr std::uint32_t kMaxEvidencePerBlock = 32;
  static constexpr std::uint32_t kMaxSystemRecordsPerBlock = 4096;
  static constexpr std::uint64_t kMaxBlockUnits = 16'777'216;
  static constexpr std::uint64_t kMaxTxUnits = 4'194'304;

  static bool validParameters(const Parameters &parameters);
  static std::optional<std::uint64_t>
  transactionUnits(const Parameters &parameters, const TransactionWork &work);
  static std::optional<std::uint64_t> evidenceUnits(std::uint32_t evidenceBytes);
  static std::optional<std::uint64_t>
  systemRecordUnits(const SystemRecordWork &work);
  static std::optional<std::uint64_t> blockUnits(
      const Parameters &parameters, std::uint32_t bodyBytes,
      std::span<const std::uint64_t> transactions,
      std::span<const std::uint64_t> evidence,
      std::span<const std::uint64_t> systemRecords);
  static std::optional<std::uint64_t> nextBaseFee(
      std::uint64_t parentBaseFee, std::uint64_t parentUsedUnits,
      std::uint64_t parentMaxBlockUnits, std::uint64_t childFloor);
  static std::optional<std::uint64_t>
  minimumFee(const Parameters &parameters, std::uint64_t baseFeePerUnit,
             std::uint64_t transactionUnits);

private:
  static std::optional<std::uint64_t> typeSurcharge(std::uint8_t type);
};

} // namespace nodo::economics

#endif
