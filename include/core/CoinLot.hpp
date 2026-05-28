#ifndef NODO_CORE_COIN_LOT_HPP
#define NODO_CORE_COIN_LOT_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

/*
 * Estado de um lote de moedas.
 *
 * AVAILABLE:
 * Pode ser usado em transações.
 *
 * LOCKED_FOR_SECURITY:
 * Está travado ajudando a segurança econômica da rede.
 *
 * SLASHED:
 * Sofreu penalidade por comportamento inválido.
 */
enum class CoinLotStatus {
    AVAILABLE,
    LOCKED_FOR_SECURITY,
    SLASHED
};

std::string coinLotStatusToString(CoinLotStatus status);

/*
 * CoinLot representa um lote rastreável de moedas.
 *
 * PRINCÍPIO CENTRAL:
 * A moeda NODO não é apenas um número no saldo.
 * Ela tem origem, dono, estado e histórico.
 */
class CoinLot {
public:
    CoinLot(
        std::string id,
        std::string originMintRecordId,
        std::string ownerAddress,
        utils::Amount amount,
        CoinLotStatus status,
        std::uint64_t createdAtBlock,
        std::uint64_t lockedUntilBlock,
        std::int64_t timestamp
    );

    const std::string& id() const;
    const std::string& originMintRecordId() const;
    const std::string& ownerAddress() const;
    utils::Amount amount() const;
    CoinLotStatus status() const;
    std::uint64_t createdAtBlock() const;
    std::uint64_t lockedUntilBlock() const;
    std::int64_t timestamp() const;

    bool isAvailable() const;
    bool isLockedForSecurity() const;
    bool isValid() const;

    void lockForSecurity(std::uint64_t lockedUntilBlock);
    void unlockIfMature(std::uint64_t currentBlock);
    void markSlashed();

    std::string serialize() const;

private:
    std::string m_id;
    std::string m_originMintRecordId;
    std::string m_ownerAddress;
    utils::Amount m_amount;
    CoinLotStatus m_status;
    std::uint64_t m_createdAtBlock;
    std::uint64_t m_lockedUntilBlock;
    std::int64_t m_timestamp;
};

} // namespace nodo::core

#endif