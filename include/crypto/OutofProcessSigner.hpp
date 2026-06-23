#ifndef NODO_CRYPTO_OUT_OF_PROCESS_SIGNER_HPP
#define NODO_CRYPTO_OUT_OF_PROCESS_SIGNER_HPP

#include "crypto/KeyEncryptionService.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

namespace nodo::crypto {

struct SignatureRequest {
    uint64_t height;
    uint32_t round;
    std::string proposalHash;
    std::string payloadToSign;
};

class OutofProcessSigner {
public:
    OutofProcessSigner(
        const std::string& keyId,
        const std::string& encryptedKeyEnvelope,
        const std::string& password,
        std::filesystem::path stateFile = {},
        bool inMemoryTestMode = false
    );

    bool signBlockProposal(const SignatureRequest& request, std::string& signatureOut);
    bool signVote(const SignatureRequest& request, std::string& signatureOut);

    const std::string& validatorAddress() const;
    const PublicKey& validatorPublicKey() const;

private:
    std::string m_keyId;
    std::string m_decryptedPrivateKey;
    std::string m_validatorAddress;
    PublicKey m_validatorPublicKey;
    std::filesystem::path m_stateFile;
    mutable std::mutex m_mutex;

    uint64_t m_lastSignedProposalHeight{0};
    uint32_t m_lastSignedProposalRound{0};
    std::string m_lastSignedProposalHash;

    uint64_t m_lastSignedVoteHeight{0};
    uint32_t m_lastSignedVoteRound{0};
    std::string m_lastSignedVoteHash;

    bool decryptKey(const std::string& envelope, const std::string& password);
    bool loadSigningState();
    bool persistSigningState() const;
};

} // namespace nodo::crypto

#endif
