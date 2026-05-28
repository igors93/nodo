#include "core/State.hpp"

#include "staking/SecurityWeight.hpp"

#include <stdexcept>

namespace nodo::core {

State::State()
    : m_currentBlockIndex(0),
      m_totalSupply(utils::Amount::fromRawUnits(0)) {}

std::uint64_t State::currentBlockIndex() const {
    return m_currentBlockIndex;
}

utils::Amount State::totalSupply() const {
    return m_totalSupply;
}

const std::vector<economics::MintRecord>& State::mintRecords() const {
    return m_mintRecords;
}

const std::vector<CoinLot>& State::coinLots() const {
    return m_coinLots;
}

void State::advanceBlock() {
    ++m_currentBlockIndex;

    for (auto& coinLot : m_coinLots) {
        coinLot.unlockIfMature(m_currentBlockIndex);
    }
}

void State::applyMintRecord(const economics::MintRecord& mintRecord) {
    if (!mintRecord.isValid()) {
        throw std::invalid_argument("Invalid MintRecord rejected by State.");
    }

    /*
     * Segurança básica:
     * Não permitir MintRecord duplicado.
     */
    for (const auto& existing : m_mintRecords) {
        if (existing.id() == mintRecord.id()) {
            throw std::logic_error("Duplicated MintRecord id rejected.");
        }
    }

    const std::string coinLotId = createCoinLotIdFromMint(mintRecord);

    CoinLot coinLot(
        coinLotId,
        mintRecord.id(),
        mintRecord.recipientAddress(),
        mintRecord.amount(),
        CoinLotStatus::AVAILABLE,
        m_currentBlockIndex,
        0,
        mintRecord.timestamp()
    );

    if (!coinLot.isValid()) {
        throw std::logic_error("Generated CoinLot is invalid.");
    }

    m_mintRecords.push_back(mintRecord);
    m_coinLots.push_back(coinLot);
    m_totalSupply = m_totalSupply + mintRecord.amount();
}

void State::lockCoinLotForSecurity(
    const std::string& coinLotId,
    std::uint64_t lockedUntilBlock
) {
    for (auto& coinLot : m_coinLots) {
        if (coinLot.id() == coinLotId) {
            coinLot.lockForSecurity(lockedUntilBlock);
            return;
        }
    }

    throw std::invalid_argument("CoinLot not found.");
}

utils::Amount State::balanceOf(const std::string& ownerAddress) const {
    utils::Amount balance = utils::Amount::fromRawUnits(0);

    for (const auto& coinLot : m_coinLots) {
        if (coinLot.ownerAddress() == ownerAddress &&
            coinLot.status() != CoinLotStatus::SLASHED) {
            balance = balance + coinLot.amount();
        }
    }

    return balance;
}

std::uint64_t State::totalSecurityWeight() const {
    std::uint64_t total = 0;

    for (const auto& coinLot : m_coinLots) {
        total += staking::SecurityWeight::calculateForCoinLot(
            coinLot,
            m_currentBlockIndex
        );
    }

    return total;
}

bool State::isSupplyAuditable() const {
    utils::Amount calculatedSupply = utils::Amount::fromRawUnits(0);

    for (const auto& mintRecord : m_mintRecords) {
        if (!mintRecord.isValid()) {
            return false;
        }

        calculatedSupply = calculatedSupply + mintRecord.amount();
    }

    /*
     * Nesta primeira versão, ainda não temos burn/slashing parcial.
     * Então o supply total deve bater exatamente com a soma de MintRecords.
     */
    return calculatedSupply == m_totalSupply;
}

std::string State::createCoinLotIdFromMint(
    const economics::MintRecord& mintRecord
) const {
    return "coinlot_from_" + mintRecord.id();
}

} // namespace nodo::core