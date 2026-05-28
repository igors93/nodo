#include "privacy/PrivateAccountingLedger.hpp"

#include <sstream>
#include <stdexcept>

namespace nodo::privacy {

PrivateAccountingLedger::PrivateAccountingLedger()
    : m_privateMintedSupply(utils::Amount::fromRawUnits(0)),
      m_privateBurnedSupply(utils::Amount::fromRawUnits(0)) {}

std::size_t PrivateAccountingLedger::size() const {
    return m_records.size();
}

bool PrivateAccountingLedger::empty() const {
    return m_records.empty();
}

const std::vector<PrivateAccountingRecord>&
PrivateAccountingLedger::records() const {
    return m_records;
}

const NullifierSet& PrivateAccountingLedger::nullifierSet() const {
    return m_nullifierSet;
}

utils::Amount PrivateAccountingLedger::privateMintedSupply() const {
    return m_privateMintedSupply;
}

utils::Amount PrivateAccountingLedger::privateBurnedSupply() const {
    return m_privateBurnedSupply;
}

utils::Amount PrivateAccountingLedger::outstandingPrivateSupply() const {
    if (m_privateBurnedSupply > m_privateMintedSupply) {
        throw std::logic_error("Private burned supply exceeds private minted supply.");
    }

    return m_privateMintedSupply - m_privateBurnedSupply;
}

std::size_t PrivateAccountingLedger::registeredCommitmentCount() const {
    return m_registeredCommitmentIds.size();
}

bool PrivateAccountingLedger::containsRecordId(
    const std::string& recordId
) const {
    if (recordId.empty()) {
        return false;
    }

    for (const auto& record : m_records) {
        if (record.id() == recordId) {
            return true;
        }
    }

    return false;
}

bool PrivateAccountingLedger::containsCommitmentId(
    const std::string& commitmentId
) const {
    if (commitmentId.empty()) {
        return false;
    }

    for (const auto& registeredCommitmentId : m_registeredCommitmentIds) {
        if (registeredCommitmentId == commitmentId) {
            return true;
        }
    }

    return false;
}

bool PrivateAccountingLedger::canAppendRecord(
    const PrivateAccountingRecord& record
) const {
    if (!record.isValid()) {
        return false;
    }

    if (containsRecordId(record.id())) {
        return false;
    }

    if (!recordInputNullifiersAreNew(record)) {
        return false;
    }

    if (!recordOutputCommitmentsAreNew(record)) {
        return false;
    }

    if (record.supplyEffect() == PublicSupplyEffect::SUPPLY_DECREASE) {
        const utils::Amount burnedAfterRecord =
            m_privateBurnedSupply + record.publicSupplyAmount();

        if (burnedAfterRecord > m_privateMintedSupply) {
            return false;
        }
    }

    return true;
}

void PrivateAccountingLedger::addRecord(
    const PrivateAccountingRecord& record
) {
    if (!canAppendRecord(record)) {
        throw std::logic_error("PrivateAccountingRecord rejected by PrivateAccountingLedger.");
    }

    registerRecordInputsAndOutputs(record);
    applySupplyEffect(record);

    m_records.push_back(record);
}

bool PrivateAccountingLedger::isValid() const {
    if (hasDuplicateRecordIds()) {
        return false;
    }

    if (hasDuplicateCommitmentIds()) {
        return false;
    }

    if (m_privateBurnedSupply > m_privateMintedSupply) {
        return false;
    }

    NullifierSet reconstructedNullifierSet;
    std::vector<std::string> reconstructedCommitmentIds;
    utils::Amount reconstructedMintedSupply = utils::Amount::fromRawUnits(0);
    utils::Amount reconstructedBurnedSupply = utils::Amount::fromRawUnits(0);

    for (const auto& record : m_records) {
        if (!record.isValid()) {
            return false;
        }

        for (const auto& nullifier : record.inputNullifiers()) {
            if (!reconstructedNullifierSet.canRegisterNullifier(nullifier)) {
                return false;
            }

            reconstructedNullifierSet.registerNullifier(nullifier);
        }

        for (const auto& commitment : record.outputCommitments()) {
            if (!commitment.isValid()) {
                return false;
            }

            for (const auto& existingCommitmentId : reconstructedCommitmentIds) {
                if (existingCommitmentId == commitment.id()) {
                    return false;
                }
            }

            reconstructedCommitmentIds.push_back(commitment.id());
        }

        if (record.supplyEffect() == PublicSupplyEffect::SUPPLY_INCREASE) {
            reconstructedMintedSupply =
                reconstructedMintedSupply + record.publicSupplyAmount();
        } else if (record.supplyEffect() == PublicSupplyEffect::SUPPLY_DECREASE) {
            reconstructedBurnedSupply =
                reconstructedBurnedSupply + record.publicSupplyAmount();

            if (reconstructedBurnedSupply > reconstructedMintedSupply) {
                return false;
            }
        }
    }

    if (reconstructedNullifierSet.size() != m_nullifierSet.size()) {
        return false;
    }

    if (reconstructedCommitmentIds.size() != m_registeredCommitmentIds.size()) {
        return false;
    }

    if (reconstructedMintedSupply != m_privateMintedSupply) {
        return false;
    }

    if (reconstructedBurnedSupply != m_privateBurnedSupply) {
        return false;
    }

    return true;
}

std::string PrivateAccountingLedger::serialize() const {
    std::ostringstream oss;

    oss << "PrivateAccountingLedger{"
        << "recordCount=" << m_records.size()
        << ";registeredCommitmentCount=" << m_registeredCommitmentIds.size()
        << ";nullifierCount=" << m_nullifierSet.size()
        << ";privateMintedSupplyRaw=" << m_privateMintedSupply.rawUnits()
        << ";privateBurnedSupplyRaw=" << m_privateBurnedSupply.rawUnits()
        << ";outstandingPrivateSupplyRaw=" << outstandingPrivateSupply().rawUnits()
        << ";records=[";

    for (std::size_t i = 0; i < m_records.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << m_records[i].serialize();
    }

    oss << "]}";

    return oss.str();
}

bool PrivateAccountingLedger::hasDuplicateRecordIds() const {
    for (std::size_t i = 0; i < m_records.size(); ++i) {
        for (std::size_t j = i + 1; j < m_records.size(); ++j) {
            if (m_records[i].id() == m_records[j].id()) {
                return true;
            }
        }
    }

    return false;
}

bool PrivateAccountingLedger::hasDuplicateCommitmentIds() const {
    for (std::size_t i = 0; i < m_registeredCommitmentIds.size(); ++i) {
        for (std::size_t j = i + 1; j < m_registeredCommitmentIds.size(); ++j) {
            if (m_registeredCommitmentIds[i] == m_registeredCommitmentIds[j]) {
                return true;
            }
        }
    }

    return false;
}

bool PrivateAccountingLedger::recordOutputCommitmentsAreNew(
    const PrivateAccountingRecord& record
) const {
    for (const auto& commitment : record.outputCommitments()) {
        if (!commitment.isValid()) {
            return false;
        }

        if (containsCommitmentId(commitment.id())) {
            return false;
        }
    }

    return true;
}

bool PrivateAccountingLedger::recordInputNullifiersAreNew(
    const PrivateAccountingRecord& record
) const {
    for (const auto& nullifier : record.inputNullifiers()) {
        if (!nullifier.isValid()) {
            return false;
        }

        if (!m_nullifierSet.canRegisterNullifier(nullifier)) {
            return false;
        }
    }

    return true;
}

void PrivateAccountingLedger::registerRecordInputsAndOutputs(
    const PrivateAccountingRecord& record
) {
    for (const auto& nullifier : record.inputNullifiers()) {
        m_nullifierSet.registerNullifier(nullifier);
    }

    for (const auto& commitment : record.outputCommitments()) {
        if (containsCommitmentId(commitment.id())) {
            throw std::logic_error("Duplicate private commitment rejected.");
        }

        m_registeredCommitmentIds.push_back(commitment.id());
    }
}

void PrivateAccountingLedger::applySupplyEffect(
    const PrivateAccountingRecord& record
) {
    if (record.supplyEffect() == PublicSupplyEffect::SUPPLY_INCREASE) {
        m_privateMintedSupply =
            m_privateMintedSupply + record.publicSupplyAmount();
        return;
    }

    if (record.supplyEffect() == PublicSupplyEffect::SUPPLY_DECREASE) {
        const utils::Amount burnedAfterRecord =
            m_privateBurnedSupply + record.publicSupplyAmount();

        if (burnedAfterRecord > m_privateMintedSupply) {
            throw std::logic_error("Private burn exceeds private minted supply.");
        }

        m_privateBurnedSupply = burnedAfterRecord;
        return;
    }

    if (record.supplyEffect() == PublicSupplyEffect::NO_SUPPLY_CHANGE) {
        return;
    }

    throw std::logic_error("Unknown private supply effect.");
}

} // namespace nodo::privacy