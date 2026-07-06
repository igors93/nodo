#ifndef NODO_NODE_EPOCH_TREASURY_REPORT_VERIFIER_HPP
#define NODO_NODE_EPOCH_TREASURY_REPORT_VERIFIER_HPP

#include "economics/EpochTreasuryReport.hpp"

#include <string>

namespace nodo::node {

enum class EpochTreasuryVerificationStatus {
  MATCH,
  FIELD_MISMATCH,
  PERSISTED_INVALID,
  REBUILT_INVALID
};

std::string
epochTreasuryVerificationStatusToString(EpochTreasuryVerificationStatus status);

class EpochTreasuryVerificationResult {
public:
  EpochTreasuryVerificationResult();

  static EpochTreasuryVerificationResult match();
  static EpochTreasuryVerificationResult fieldMismatch(std::string reason);
  static EpochTreasuryVerificationResult persistedInvalid(std::string reason);
  static EpochTreasuryVerificationResult rebuiltInvalid(std::string reason);

  bool matched() const;
  EpochTreasuryVerificationStatus status() const;
  const std::string &reason() const;

private:
  EpochTreasuryVerificationStatus m_status;
  std::string m_reason;
};

/*
 * EpochTreasuryReportVerifier compares a persisted EpochTreasuryReport against
 * a report rebuilt from the canonical TreasurySpendRecord sequence.
 *
 * Security principle:
 * A persisted treasury report is not authoritative. It must agree with the
 * rebuilt report on every field. Any discrepancy fails the audit.
 */
class EpochTreasuryReportVerifier {
public:
  static EpochTreasuryVerificationResult
  verify(const economics::EpochTreasuryReport &persisted,
         const economics::EpochTreasuryReport &rebuilt);
};

} // namespace nodo::node

#endif
