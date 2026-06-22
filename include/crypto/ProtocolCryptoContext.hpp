#ifndef NODO_CRYPTO_PROTOCOL_CRYPTO_CONTEXT_HPP
#define NODO_CRYPTO_PROTOCOL_CRYPTO_CONTEXT_HPP

#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/SignatureProvider.hpp"

#include <string>

namespace nodo::crypto {

/*
 * ProtocolNetworkProfile classifies the network profile that is asking for
 * protocol cryptography.
 *
 * The protocol path must stay the same for localnet, testnet and mainnet.
 * What changes is the safety requirement enforced by this context.
 */
enum class ProtocolNetworkProfile {
    LOCALNET,
    TESTNET,
    MAINNET,
    UNKNOWN
};

std::string protocolNetworkProfileToString(
    ProtocolNetworkProfile profile
);

/*
 * ProtocolCryptoContext defines the cryptographic boundary used by protocol
 * execution.
 *
 * Runtime code should depend on this context instead of choosing a crypto
 * policy or signature provider inline. This keeps localnet, testnet and mainnet
 * on one protocol path while allowing each network profile to enforce different
 * cryptographic requirements.
 */
class ProtocolCryptoContext {
public:
    /*
     * Current runnable profile.
     *
     * localnet uses deterministic test keys, but protocol signatures are real
     * Ed25519 and BLS12-381 signatures.
     */
    static ProtocolCryptoContext localnet();

    /*
     * Future public testing profile.
     *
     * This currently refuses to run because there is not yet a production-safe
     * signature provider wired into Nodo.
     */
    static ProtocolCryptoContext testnet();

    /*
     * Future production profile.
     *
     * This must refuse temporary cryptography. Mainnet should only become valid
     * after a production-safe provider is available.
     */
    static ProtocolCryptoContext mainnet();

    /*
     * Builds a crypto context from the network name stored in NetworkParameters.
     * Accepted names: localnet, testnet, testnet-candidate, mainnet.
     */
    static ProtocolCryptoContext fromNetworkName(
        const std::string& networkName
    );

    ProtocolNetworkProfile profile() const;

    const std::string& networkProfile() const;

    const CryptoPolicy& policy() const;

    const SignatureProvider& signatureProvider() const;

    const SignatureProvider& userSignatureProvider() const;

    const SignatureProvider& validatorSignatureProvider() const;

    bool temporaryProviderAllowed() const;

    bool requiresProductionProvider() const;

    bool productionSafe() const;

    bool hasTemporaryProvider() const;

    bool isValid() const;

    const std::string& rejectionReason() const;

private:
    ProtocolCryptoContext(
        ProtocolNetworkProfile profile,
        std::string networkProfile,
        CryptoPolicy policy,
        bool temporaryProviderAllowed,
        bool requiresProductionProvider,
        std::string rejectionReason
    );

    ProtocolNetworkProfile m_profile;
    std::string m_networkProfile;
    CryptoPolicy m_policy;
    bool m_temporaryProviderAllowed;
    bool m_requiresProductionProvider;
    std::string m_rejectionReason;
    Ed25519SignatureProvider m_userProvider;
    Bls12381SignatureProvider m_validatorProvider;
};

} // namespace nodo::crypto

#endif
