#ifndef NODO_ECONOMICS_PROTECTION_ECONOMICS_REBUILDER_HPP
#define NODO_ECONOMICS_PROTECTION_ECONOMICS_REBUILDER_HPP

#include "core/Blockchain.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/ProtectionEconomicsState.hpp"

#include <string>
#include <vector>

namespace nodo::economics {

/*
 * ProtectionEconomicsRebuilder rebuilds protection economy state from chain
 * history.
 *
 * In simple terms:
 * It reads blocks and answers:
 * - who did useful work?
 * - what is each validator's latest score?
 * - how much reward was emitted?
 * - which reward coin lots were born?
 */
class ProtectionEconomicsRebuilder {
public:
    static ProtectionEconomicsState rebuildFromBlockchain(
        const core::Blockchain& blockchain
    );

    static ProtectionEconomicsState rebuildFromBlocks(
        const std::vector<core::Block>& blocks
    );

    static void applyLedgerRecord(
        ProtectionEconomicsState& state,
        const core::LedgerRecord& record,
        std::uint64_t blockIndex
    );

private:
    static bool isProtectionRecord(
        core::LedgerRecordType type
    );

    static std::int64_t parseInt64Field(
        const std::string& payload,
        const std::string& fieldName
    );

    static std::uint64_t parseUInt64Field(
        const std::string& payload,
        const std::string& fieldName
    );

    static std::int32_t parseInt32Field(
        const std::string& payload,
        const std::string& fieldName
    );

    static std::string parseStringField(
        const std::string& payload,
        const std::string& fieldName
    );

    static void applyValidationWorkRecord(
        ProtectionEconomicsState& state,
        const core::LedgerRecord& record
    );

    static void applyValidatorScoreRecord(
        ProtectionEconomicsState& state,
        const core::LedgerRecord& record
    );

    static void applyProtectionEpochRecord(
        ProtectionEconomicsState& state,
        const core::LedgerRecord& record
    );

    static void applyGenesisRewardRecord(
        ProtectionEconomicsState& state,
        const core::LedgerRecord& record,
        std::uint64_t blockIndex
    );
};

} // namespace nodo::economics

#endif
