#ifndef NODO_CORE_TRANSACTION_TYPE_HPP
#define NODO_CORE_TRANSACTION_TYPE_HPP

namespace nodo::core {

/*
 * Tipos de transações planejadas para a Nodo.
 *
 * Nem todos serão implementados agora.
 * Mas já deixamos os nomes definidos para guiar a arquitetura.
 */
enum class TransactionType {
    TRANSFER,
    MINT_REWARD,
    LOCK_SECURITY,
    UNLOCK_SECURITY,
    VALIDATOR_REGISTER,
    VALIDATOR_VOTE,
    PENALTY,
    BURN
};

} // namespace nodo::core

#endif