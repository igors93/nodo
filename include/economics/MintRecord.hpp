#ifndef NODO_ECONOMICS_MINT_RECORD_HPP
#define NODO_ECONOMICS_MINT_RECORD_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * MintReason explica por que novas moedas foram criadas.
 *
 * REGRA DA NODO:
 * Nenhuma moeda pode ser criada sem um motivo explícito.
 */
enum class MintReason {
    GENESIS_ALLOCATION,
    NETWORK_DEFENSE_REWARD,
    LOCKED_RESERVE_REWARD,
    TREASURY_REWARD
};

std::string mintReasonToString(MintReason reason);

/*
 * MintRecord registra a origem de moedas novas.
 *
 * PRINCÍPIO CENTRAL:
 * Toda moeda NODO nasce de um MintRecord.
 *
 * Isso torna a emissão auditável.
 */
class MintRecord {
public:
    MintRecord(
        std::string id,
        std::string recipientAddress,
        utils::Amount amount,
        MintReason reason,
        std::uint64_t epoch,
        std::uint64_t sourceBlockIndex,
        std::string sourceBlockHash,
        std::int64_t timestamp
    );

    const std::string& id() const;
    const std::string& recipientAddress() const;
    utils::Amount amount() const;
    MintReason reason() const;
    std::uint64_t epoch() const;
    std::uint64_t sourceBlockIndex() const;
    const std::string& sourceBlockHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    /*
     * Serialização determinística.
     *
     * SEGURANÇA:
     * Dados que entram em hash devem sempre ser serializados
     * na mesma ordem e no mesmo formato.
     */
    std::string serialize() const;

private:
    std::string m_id;
    std::string m_recipientAddress;
    utils::Amount m_amount;
    MintReason m_reason;
    std::uint64_t m_epoch;
    std::uint64_t m_sourceBlockIndex;
    std::string m_sourceBlockHash;
    std::int64_t m_timestamp;
};

} // namespace nodo::economics

#endif