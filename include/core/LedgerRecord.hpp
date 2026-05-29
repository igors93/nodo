#ifndef NODO_CORE_LEDGER_RECORD_HPP
#define NODO_CORE_LEDGER_RECORD_HPP

#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/MintRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "privacy/PrivateAccountingRecord.hpp"

#include <cstdint>
#include <string>

namespace nodo::serialization {
class LedgerRecordCodec;
}

namespace nodo::core {

/*
 * LedgerRecordType defines what kind of event was accepted into the ledger.
 *
 * A LedgerRecord is not the same as a Transaction.
 * A Transaction is a request.
 * A LedgerRecord is an accepted and auditable ledger entry.
 */
enum class LedgerRecordType {
    MINT,
    TRANSACTION,
    PRIVATE_ACCOUNTING,
    VALIDATION_WORK,
    VALIDATOR_SCORE,
    PROTECTION_EPOCH,
    GENESIS_REWARD
};

std::string ledgerRecordTypeToString(LedgerRecordType type);

/*
 * LedgerRecord represents an immutable record accepted by the Nodo ledger.
 *
 * Security principle:
 * Important events should not be applied silently.
 * They should become deterministic, hashable, auditable records.
 */
class LedgerRecord {
public:
    static LedgerRecord fromMintRecord(
        const economics::MintRecord& mintRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromTransaction(
        const Transaction& transaction,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        std::int64_t timestamp
    );

    /*
     * Converts a valid private accounting operation into an official ledger record.
     *
     * Security rule:
     * Private accounting must enter the chain through the same deterministic
     * record pipeline as public mints and public transactions.
     */
    static LedgerRecord fromPrivateAccountingRecord(
        const privacy::PrivateAccountingRecord& privateAccountingRecord,
        std::int64_t timestamp
    );

    /*
     * Protection economics records.
     *
     * These methods start integrating the new Proof of Protection model into
     * the same auditable LedgerRecord pipeline used by blocks.
     */
    static LedgerRecord fromValidationWorkRecord(
        const economics::ValidationWorkRecord& validationWorkRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromValidatorScoreRecord(
        const economics::ValidatorScoreRecord& validatorScoreRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromProtectionEpoch(
        const economics::ProtectionEpoch& protectionEpoch,
        std::int64_t timestamp
    );

    static LedgerRecord fromGenesisRewardRecord(
        const economics::GenesisRewardRecord& genesisRewardRecord,
        std::int64_t timestamp
    );

    const std::string& id() const;
    LedgerRecordType type() const;
    const std::string& sourceId() const;
    const std::string& payload() const;
    const std::string& payloadHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    /*
     * Deterministic serialization.
     *
     * This representation is used inside Block hashing.
     */
    std::string serialize() const;

private:
    /*
     * LedgerRecordCodec is the only serialization boundary allowed to rebuild
     * a LedgerRecord from trusted serialized fields.
     *
     * Security rule:
     * Any reconstructed record must still pass LedgerRecord::isValid().
     */
    friend class nodo::serialization::LedgerRecordCodec;

    LedgerRecord(
        std::string id,
        LedgerRecordType type,
        std::string sourceId,
        std::string payload,
        std::string payloadHash,
        std::int64_t timestamp
    );

    static std::string hashPayload(const std::string& payload);

    static std::string computeRecordId(
        LedgerRecordType type,
        const std::string& sourceId,
        const std::string& payloadHash,
        std::int64_t timestamp
    );

    static std::string computeDerivedSourceId(
        const std::string& sourcePrefix,
        const std::string& payloadHash
    );

    std::string m_id;
    LedgerRecordType m_type;
    std::string m_sourceId;
    std::string m_payload;
    std::string m_payloadHash;
    std::int64_t m_timestamp;
};

} // namespace nodo::core

#endif
