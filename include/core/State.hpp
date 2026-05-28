#ifndef NODO_CORE_STATE_HPP
#define NODO_CORE_STATE_HPP

#include "core/CoinLot.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * State representa o estado atual da rede Nodo.
 *
 * Ele guarda:
 * - moedas criadas;
 * - lotes de moedas;
 * - supply total;
 * - altura atual da blockchain.
 *
 * FUTURO:
 * Depois o State também terá contas, validadores,
 * mempool, snapshots e regras de aplicação de transações.
 */
class State {
public:
    State();

    std::uint64_t currentBlockIndex() const;
    utils::Amount totalSupply() const;

    const std::vector<economics::MintRecord>& mintRecords() const;
    const std::vector<CoinLot>& coinLots() const;

    void advanceBlock();

    /*
     * Cria moedas a partir de um MintRecord.
     *
     * REGRA DE SEGURANÇA:
     * A função rejeita MintRecord inválido.
     * Toda moeda criada gera um CoinLot rastreável.
     */
    void applyMintRecord(const economics::MintRecord& mintRecord);

    /*
     * Trava um CoinLot para segurança.
     */
    void lockCoinLotForSecurity(
        const std::string& coinLotId,
        std::uint64_t lockedUntilBlock
    );

    utils::Amount balanceOf(const std::string& ownerAddress) const;

    std::uint64_t totalSecurityWeight() const;

    bool isSupplyAuditable() const;

private:
    std::uint64_t m_currentBlockIndex;
    utils::Amount m_totalSupply;
    std::vector<economics::MintRecord> m_mintRecords;
    std::vector<CoinLot> m_coinLots;

    std::string createCoinLotIdFromMint(const economics::MintRecord& mintRecord) const;
};

} // namespace nodo::core

#endif