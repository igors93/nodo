#ifndef NODO_CORE_LEDGER_RECORD_DOMAIN_VALIDATOR_HPP
#define NODO_CORE_LEDGER_RECORD_DOMAIN_VALIDATOR_HPP

#include "core/LedgerRecord.hpp"

#include <string>

namespace nodo::core {

/*
 * LedgerRecordDomainValidator verifies that a non-transaction LedgerRecord
 * payload can be deserialized to a structurally valid domain object of the
 * declared type.
 *
 * This is a pre-vote gate: a proposer who inserts a record with a
 * correctly-hashed but unparseable payload would otherwise pass
 * LedgerRecord::isValid() and only fail at finalization time (too late).
 *
 * For types that expose a static deserialize() method, the payload is
 * round-trip verified: deserialize → isValid(). For types that only expose
 * serialize(), LedgerRecord::isValid() already guarantees hash consistency,
 * which is the binding protection for those types.
 */
class LedgerRecordDomainValidator {
public:
    struct Result {
        bool valid = false;
        std::string reason;

        static Result ok() { return {true, ""}; }
        static Result fail(std::string r) { return {false, std::move(r)}; }
    };

    static Result validate(const LedgerRecord& record);

private:
    static Result validateMint(const LedgerRecord& record);
    static Result validateValidationWork(const LedgerRecord& record);
    static Result validateValidatorScore(const LedgerRecord& record);
    static Result validateProtectionEpoch(const LedgerRecord& record);
    static Result validateGenesisReward(const LedgerRecord& record);
    static Result validateValidatorPenalty(const LedgerRecord& record);
};

} // namespace nodo::core

#endif
