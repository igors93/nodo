#include "core/CoinLotRegistry.hpp"

#include <sstream>
#include <stdexcept>

namespace nodo::core {

CoinLotRegistry::CoinLotRegistry()
    : m_lots() {}

CoinLotRegistry CoinLotRegistry::fromCoinLots(
    const std::vector<CoinLot>& lots
) {
    CoinLotRegistry registry;

    for (const auto& lot : lots) {
        registry.addLot(lot);
    }

    return registry;
}

void CoinLotRegistry::addLot(
    const CoinLot& lot
) {
    assertLotCanBeAdded(lot);

    const auto inserted =
        m_lots.emplace(
            lot.id(),
            lot
        );

    if (!inserted.second) {
        throw std::logic_error("CoinLot id already exists in registry.");
    }
}

bool CoinLotRegistry::hasLot(
    const std::string& lotId
) const {
    return m_lots.find(lotId) != m_lots.end();
}

const CoinLot& CoinLotRegistry::lot(
    const std::string& lotId
) const {
    const auto it = m_lots.find(lotId);

    if (it == m_lots.end()) {
        throw std::out_of_range("CoinLot was not found in registry.");
    }

    return it->second;
}

CoinLotVerificationResult CoinLotRegistry::verifyExists(
    const std::string& lotId
) const {
    if (lotId.empty()) {
        return CoinLotVerificationResult::invalid(
            "CoinLot id is empty."
        );
    }

    if (!hasLot(lotId)) {
        return CoinLotVerificationResult::invalid(
            "CoinLot does not exist."
        );
    }

    if (!lot(lotId).isValid()) {
        return CoinLotVerificationResult::invalid(
            "CoinLot exists but is invalid."
        );
    }

    return CoinLotVerificationResult::valid();
}

CoinLotVerificationResult CoinLotRegistry::verifySpendable(
    const std::string& lotId,
    const std::string& expectedOwner
) const {
    const CoinLotVerificationResult exists =
        verifyExists(lotId);

    if (!exists.success()) {
        return exists;
    }

    if (expectedOwner.empty()) {
        return CoinLotVerificationResult::invalid(
            "Expected owner is empty."
        );
    }

    const CoinLot& existingLot =
        lot(lotId);

    if (existingLot.ownerAddress() != expectedOwner) {
        return CoinLotVerificationResult::invalid(
            "CoinLot owner mismatch."
        );
    }

    if (!existingLot.isSpendable()) {
        return CoinLotVerificationResult::invalid(
            "CoinLot is not spendable."
        );
    }

    return CoinLotVerificationResult::valid();
}

CoinLotVerificationResult CoinLotRegistry::verifyExactSpend(
    const std::string& lotId,
    const std::string& expectedOwner,
    utils::Amount expectedAmount
) const {
    const CoinLotVerificationResult spendable =
        verifySpendable(
            lotId,
            expectedOwner
        );

    if (!spendable.success()) {
        return spendable;
    }

    if (!expectedAmount.isPositive()) {
        return CoinLotVerificationResult::invalid(
            "Expected spend amount must be positive."
        );
    }

    const CoinLot& existingLot =
        lot(lotId);

    if (existingLot.amount() != expectedAmount) {
        return CoinLotVerificationResult::invalid(
            "CoinLot amount mismatch."
        );
    }

    return CoinLotVerificationResult::valid();
}

void CoinLotRegistry::consumeLotAndCreateOutputs(
    const std::string& inputLotId,
    const std::string& expectedOwner,
    const std::vector<CoinLot>& outputLots
) {
    if (outputLots.empty()) {
        throw std::invalid_argument("CoinLot spend must create at least one output lot.");
    }

    const CoinLotVerificationResult spendable =
        verifySpendable(
            inputLotId,
            expectedOwner
        );

    if (!spendable.success()) {
        throw std::invalid_argument("Input CoinLot is not spendable: " + spendable.reason());
    }

    for (const auto& outputLot : outputLots) {
        assertLotCanBeAdded(outputLot);

        if (!outputLot.isAvailable()) {
            throw std::invalid_argument("Output CoinLot must be available.");
        }
    }

    const utils::Amount inputAmount =
        lot(inputLotId).amount();

    const utils::Amount outputAmount =
        sumLots(outputLots);

    if (inputAmount != outputAmount) {
        throw std::invalid_argument("CoinLot spend outputs must exactly equal input amount.");
    }

    auto it = m_lots.find(inputLotId);

    if (it == m_lots.end()) {
        throw std::out_of_range("Input CoinLot disappeared from registry.");
    }

    it->second.markSpent();

    for (const auto& outputLot : outputLots) {
        addLot(outputLot);
    }
}

void CoinLotRegistry::markSpent(
    const std::string& lotId,
    const std::string& expectedOwner
) {
    const CoinLotVerificationResult spendable =
        verifySpendable(
            lotId,
            expectedOwner
        );

    if (!spendable.success()) {
        throw std::invalid_argument("CoinLot cannot be marked spent: " + spendable.reason());
    }

    auto it = m_lots.find(lotId);

    if (it == m_lots.end()) {
        throw std::out_of_range("CoinLot was not found in registry.");
    }

    it->second.markSpent();
}

void CoinLotRegistry::lockForSecurity(
    const std::string& lotId,
    const std::string& expectedOwner,
    std::uint64_t lockedUntilBlock
) {
    const CoinLotVerificationResult spendable =
        verifySpendable(
            lotId,
            expectedOwner
        );

    if (!spendable.success()) {
        throw std::invalid_argument("CoinLot cannot be locked: " + spendable.reason());
    }

    auto it = m_lots.find(lotId);

    if (it == m_lots.end()) {
        throw std::out_of_range("CoinLot was not found in registry.");
    }

    it->second.lockForSecurity(lockedUntilBlock);
}

void CoinLotRegistry::unlockIfMature(
    const std::string& lotId,
    std::uint64_t currentBlock
) {
    auto it = m_lots.find(lotId);

    if (it == m_lots.end()) {
        throw std::out_of_range("CoinLot was not found in registry.");
    }

    it->second.unlockIfMature(currentBlock);
}

void CoinLotRegistry::markSlashed(
    const std::string& lotId
) {
    auto it = m_lots.find(lotId);

    if (it == m_lots.end()) {
        throw std::out_of_range("CoinLot was not found in registry.");
    }

    it->second.markSlashed();
}

utils::Amount CoinLotRegistry::availableBalanceForOwner(
    const std::string& ownerAddress
) const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : m_lots) {
        const CoinLot& current = entry.second;

        if (current.ownerAddress() == ownerAddress &&
            current.isAvailable()) {
            total = total + current.amount();
        }
    }

    return total;
}

utils::Amount CoinLotRegistry::lockedBalanceForOwner(
    const std::string& ownerAddress
) const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : m_lots) {
        const CoinLot& current = entry.second;

        if (current.ownerAddress() == ownerAddress &&
            current.isLockedForSecurity()) {
            total = total + current.amount();
        }
    }

    return total;
}

utils::Amount CoinLotRegistry::totalTrackedAmount() const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : m_lots) {
        total = total + entry.second.amount();
    }

    return total;
}

utils::Amount CoinLotRegistry::totalAvailableAmount() const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : m_lots) {
        if (entry.second.isAvailable()) {
            total = total + entry.second.amount();
        }
    }

    return total;
}

utils::Amount CoinLotRegistry::totalLockedAmount() const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : m_lots) {
        if (entry.second.isLockedForSecurity()) {
            total = total + entry.second.amount();
        }
    }

    return total;
}

utils::Amount CoinLotRegistry::totalSpentAmount() const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : m_lots) {
        if (entry.second.isSpent()) {
            total = total + entry.second.amount();
        }
    }

    return total;
}

utils::Amount CoinLotRegistry::totalSlashedAmount() const {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& entry : m_lots) {
        if (entry.second.isSlashed()) {
            total = total + entry.second.amount();
        }
    }

    return total;
}

std::size_t CoinLotRegistry::size() const {
    return m_lots.size();
}

const std::map<std::string, CoinLot>& CoinLotRegistry::lots() const {
    return m_lots;
}

bool CoinLotRegistry::isValid() const {
    for (const auto& entry : m_lots) {
        if (entry.first.empty()) {
            return false;
        }

        if (entry.first != entry.second.id()) {
            return false;
        }

        if (!entry.second.isValid()) {
            return false;
        }
    }

    return true;
}

std::string CoinLotRegistry::serialize() const {
    std::ostringstream oss;

    oss << "CoinLotRegistry{"
        << "lots=" << m_lots.size()
        << ";totalTrackedRaw=" << totalTrackedAmount().rawUnits()
        << ";totalAvailableRaw=" << totalAvailableAmount().rawUnits()
        << ";totalLockedRaw=" << totalLockedAmount().rawUnits()
        << ";totalSpentRaw=" << totalSpentAmount().rawUnits()
        << ";totalSlashedRaw=" << totalSlashedAmount().rawUnits()
        << "}";

    return oss.str();
}

void CoinLotRegistry::assertLotCanBeAdded(
    const CoinLot& lot
) const {
    if (!lot.isValid()) {
        throw std::invalid_argument("Invalid CoinLot cannot be added to registry.");
    }

    if (lot.id().empty()) {
        throw std::invalid_argument("CoinLot id cannot be empty.");
    }

    if (hasLot(lot.id())) {
        throw std::logic_error("CoinLot id already exists in registry.");
    }
}

utils::Amount CoinLotRegistry::sumLots(
    const std::vector<CoinLot>& lots
) {
    utils::Amount total =
        utils::Amount::fromRawUnits(0);

    for (const auto& lot : lots) {
        total = total + lot.amount();
    }

    return total;
}

} // namespace nodo::core
