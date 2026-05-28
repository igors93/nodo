#ifndef NODO_PRIVACY_PRIVATE_ACCOUNTING_RECORD_HPP
#define NODO_PRIVACY_PRIVATE_ACCOUNTING_RECORD_HPP

#include "privacy/PrivacyCommitment.hpp"
#include "privacy/PrivacyNullifier.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::privacy {

/*
 * PrivateAccountingRecordType describes the accounting meaning of a private
 * ledger operation.
 *
 * PRIVATE_MINT:
 * Public supply increases and private commitments are created.
 *
 * PRIVATE_TRANSFER:
 * Private nullifiers are consumed and private output commitments are created.
 * Public supply must remain unchanged.
 *
 * PRIVATE_BURN:
 * Private nullifiers are consumed and public supply decreases.
 */
enum class PrivateAccountingRecordType {
    PRIVATE_MINT,
    PRIVATE_TRANSFER,
    PRIVATE_BURN
};

std::string privateAccountingRecordTypeToString(
    PrivateAccountingRecordType type
);

/*
 * PublicSupplyEffect describes whether a private operation changes public
 * monetary supply.
 *
 * Important:
 * Private transfers should have NO_SUPPLY_CHANGE.
 */
enum class PublicSupplyEffect {
    NO_SUPPLY_CHANGE,
    SUPPLY_INCREASE,
    SUPPLY_DECREASE
};

std::string publicSupplyEffectToString(PublicSupplyEffect effect);

/*
 * PrivateAccountingRecord connects private commitments and nullifiers into an
 * auditable private accounting operation.
 *
 * Security principle:
 * Privacy must not mean unverifiable money creation.
 *
 * A private record should hide ownership and amounts in the future, but still
 * provide enough public proof metadata for nodes to verify that supply rules
 * were respected.
 *
 * Current status:
 * This is a development foundation. The proofHash is simulated. Future
 * versions must replace it with real zero-knowledge proof verification.
 */
class PrivateAccountingRecord {
public:
    static PrivateAccountingRecord createPrivateMintRecord(
        std::vector<PrivacyCommitment> outputCommitments,
        utils::Amount publicSupplyAmount,
        std::string auditReference,
        std::int64_t timestamp
    );

    static PrivateAccountingRecord createPrivateTransferRecord(
        std::vector<PrivacyNullifier> inputNullifiers,
        std::vector<PrivacyCommitment> outputCommitments,
        std::string auditReference,
        std::int64_t timestamp
    );

    static PrivateAccountingRecord createPrivateBurnRecord(
        std::vector<PrivacyNullifier> inputNullifiers,
        utils::Amount publicSupplyAmount,
        std::string auditReference,
        std::int64_t timestamp
    );

    PrivateAccountingRecord(
        std::string id,
        PrivateAccountingRecordType type,
        PublicSupplyEffect supplyEffect,
        utils::Amount publicSupplyAmount,
        std::vector<PrivacyNullifier> inputNullifiers,
        std::vector<PrivacyCommitment> outputCommitments,
        std::string auditReference,
        std::string proofHash,
        std::int64_t timestamp
    );

    const std::string& id() const;
    PrivateAccountingRecordType type() const;
    PublicSupplyEffect supplyEffect() const;
    utils::Amount publicSupplyAmount() const;
    const std::vector<PrivacyNullifier>& inputNullifiers() const;
    const std::vector<PrivacyCommitment>& outputCommitments() const;
    const std::string& auditReference() const;
    const std::string& proofHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    bool hasDuplicateInputNullifiers() const;
    bool hasDuplicateOutputCommitments() const;

    std::string serialize() const;

private:
    static PrivateAccountingRecord createRecord(
        PrivateAccountingRecordType type,
        PublicSupplyEffect supplyEffect,
        utils::Amount publicSupplyAmount,
        std::vector<PrivacyNullifier> inputNullifiers,
        std::vector<PrivacyCommitment> outputCommitments,
        std::string auditReference,
        std::int64_t timestamp
    );

    static std::string computeDevelopmentProofHash(
        PrivateAccountingRecordType type,
        PublicSupplyEffect supplyEffect,
        const utils::Amount& publicSupplyAmount,
        const std::vector<PrivacyNullifier>& inputNullifiers,
        const std::vector<PrivacyCommitment>& outputCommitments,
        const std::string& auditReference
    );

    static std::string computeRecordId(
        PrivateAccountingRecordType type,
        const std::string& proofHash,
        const std::string& auditReference,
        std::int64_t timestamp
    );

    static std::string hashString(const std::string& value);

    bool hasValidSupplyEffect() const;
    bool hasValidShapeForType() const;
    bool hasValidInputsAndOutputs() const;

    std::string m_id;
    PrivateAccountingRecordType m_type;
    PublicSupplyEffect m_supplyEffect;
    utils::Amount m_publicSupplyAmount;
    std::vector<PrivacyNullifier> m_inputNullifiers;
    std::vector<PrivacyCommitment> m_outputCommitments;
    std::string m_auditReference;
    std::string m_proofHash;
    std::int64_t m_timestamp;
};

} // namespace nodo::privacy

#endif