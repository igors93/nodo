#include "core/State.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Time.hpp"

#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/hash.h"

#include <iostream>
#include <string>

int main() {
    using nodo::core::State;

    using nodo::economics::MintRecord;
    using nodo::economics::MintReason;

    using nodo::utils::Amount;
    using nodo::utils::currentUnixTimestamp;

    using nodo::crypto::CryptoAlgorithm;
    using nodo::crypto::CryptoPolicy;
    using nodo::crypto::PrivateKey;
    using nodo::crypto::PublicKey;
    using nodo::crypto::SecurityContext;
    using nodo::crypto::SignatureBundle;

    std::cout << "Nodo Blockchain - Initial Security Core\n";
    std::cout << "---------------------------------------\n\n";

    State state;

    /*
     * Nesta fase inicial, ainda usamos chaves de desenvolvimento.
     *
     * IMPORTANTE:
     * Isto não é seguro para produção.
     * Serve apenas para a Nodo nascer com a arquitetura correta:
     * chave pública, chave privada, assinatura e política criptográfica.
     */
    PublicKey igorPublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-development-public-key"
    );

    PrivateKey igorPrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-development-private-key"
    );

    /*
     * Bloco gênesis simplificado:
     * Criamos moedas iniciais usando MintRecord.
     *
     * REGRA DA NODO:
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

    /*
     * Criamos uma assinatura de desenvolvimento para o MintRecord.
     *
     * Ideia:
     * No futuro, ações como criação de moeda, validação e staking
     * deverão carregar assinaturas reais.
     */
    SignatureBundle genesisSignatureBundle =
        SignatureBundle::createDevelopmentSignature(
            genesisMint.serialize(),
            igorPublicKey,
            igorPrivateKey,
            currentUnixTimestamp()
        );

    CryptoPolicy cryptoPolicy = CryptoPolicy::developmentPolicy();

    const bool signaturePolicyValid =
        genesisSignatureBundle.isValidForPolicy(
            cryptoPolicy,
            SecurityContext::DEVELOPMENT_ONLY
        );

    std::cout << "Genesis signature policy check: "
              << (signaturePolicyValid ? "VALID" : "INVALID")
              << "\n";

    if (!signaturePolicyValid) {
        std::cerr << "Fatal: genesis signature rejected by crypto policy.\n";
        return 1;
    }

    state.applyMintRecord(genesisMint);

    std::cout << "\nGenesis mint applied.\n";
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

    std::cout << "\nGenesis SignatureBundle preview:\n";
    std::cout << genesisSignatureBundle.serialize() << "\n";

    std::cout << "\nNodo initial crypto-agile base executed successfully.\n";

    return 0;
}