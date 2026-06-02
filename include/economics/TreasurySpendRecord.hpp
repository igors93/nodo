#ifndef NODO_ECONOMICS_TREASURY_SPEND_RECORD_HPP
#define NODO_ECONOMICS_TREASURY_SPEND_RECORD_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

class TreasurySpendRecord {
public:
    TreasurySpendRecord();

    TreasurySpendRecord(
        std::string spendId,
        std::string proposalId,
        std::string recipientAddress,
        utils::Amount amount,
        std::string purpose,
        std::uint64_t executedAtBlock,
        std::uint64_t epoch,
        utils::Amount treasuryBalanceBefore,
        utils::Amount treasuryBalanceAfter
    );

    const std::string& spendId() const;
    const std::string& proposalId() const;
    const std::string& recipientAddress() const;
    utils::Amount amount() const;
    const std::string& purpose() const;
    std::uint64_t executedAtBlock() const;
    std::uint64_t epoch() const;
    utils::Amount treasuryBalanceBefore() const;
    utils::Amount treasuryBalanceAfter() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_spendId;
    std::string m_proposalId;
    std::string m_recipientAddress;
    utils::Amount m_amount;
    std::string m_purpose;
    std::uint64_t m_executedAtBlock;
    std::uint64_t m_epoch;
    utils::Amount m_treasuryBalanceBefore;
    utils::Amount m_treasuryBalanceAfter;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
