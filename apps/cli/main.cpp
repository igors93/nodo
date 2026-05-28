#include "core/State.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Time.hpp"
#include "crypto/hash.h"

#include <iostream>
#include <string>

int main() {
    using nodo::core::State;
    using nodo::economics::MintRecord;
    using nodo::economics::MintReason;
    using nodo::utils::Amount;
    using nodo::utils::currentUnixTimestamp;

    std::cout << "Nodo Blockchain - Initial Security Core\n";
    std::cout << "---------------------------------------\n\n";

    State state;

    /*
     * Bloco gênesis simplificado:
     * Criamos moedas iniciais usando MintRecord.
     *
     * REGRA:
     * Mesmo moedas do gênesis precisam de registro.
     */
    MintRecord genesisMint(
        "mint_genesis_igor_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        currentUnixTimestamp()
    );

    state.applyMintRecord(genesisMint);

    std::cout << "Genesis mint applied.\n";
    std::cout << "Total supply: " << state.totalSupply().toString() << "\n";
    std::cout << "Igor balance: " << state.balanceOf("igor").toString() << "\n";

    /*
     * Travamos o CoinLot criado no gênesis.
     *
     * Ideia central:
     * moedas travadas deixam de ser apenas valor disponível
     * e passam a gerar força de segurança.
     */
    state.lockCoinLotForSecurity(
        "coinlot_from_mint_genesis_igor_001",
        500
    );

    std::cout << "\nCoinLot locked for network security.\n";
    std::cout << "Current block: " << state.currentBlockIndex() << "\n";
    std::cout << "Total security weight: " << state.totalSecurityWeight() << "\n";

    /*
     * Auditoria simples do supply.
     */
    std::cout << "\nSupply audit: "
              << (state.isSupplyAuditable() ? "VALID" : "INVALID")
              << "\n";

    /*
     * Teste do módulo de hash.
     */
    char hashOutput[65] = {0};
    nodo_hash_string(genesisMint.serialize().c_str(), hashOutput, sizeof(hashOutput));

    std::cout << "\nGenesis MintRecord hash preview:\n";
    std::cout << hashOutput << "\n";

    std::cout << "\nNodo initial base executed successfully.\n";

    return 0;
}