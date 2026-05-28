#ifndef NODO_STAKING_SECURITY_WEIGHT_HPP
#define NODO_STAKING_SECURITY_WEIGHT_HPP

#include "core/CoinLot.hpp"

#include <cstdint>

namespace nodo::staking {

/*
 * SecurityWeight representa a força econômica gerada por moedas travadas.
 *
 * Ideia da Nodo:
 * - moeda livre = valor disponível;
 * - moeda travada = valor protegendo a rede.
 */
class SecurityWeight {
public:
    /*
     * Calcula o peso de segurança de um lote de moedas.
     *
     * Nesta primeira versão:
     * peso = quantidade de NODO travado * multiplicador por tempo.
     *
     * IMPORTANTE:
     * Isso é uma regra econômica do protocolo.
     * Deve ser determinística.
     */
    static std::uint64_t calculateForCoinLot(
        const core::CoinLot& coinLot,
        std::uint64_t currentBlock
    );

private:
    static std::uint64_t lockDurationMultiplier(
        std::uint64_t currentBlock,
        std::uint64_t lockedUntilBlock
    );
};

} // namespace nodo::staking

#endif