#ifndef NODO_CRYPTO_OUT_OF_PROCESS_SIGNER_HPP
#define NODO_CRYPTO_OUT_OF_PROCESS_SIGNER_HPP

#include "crypto/KeyEncryptionService.hpp"
#include <string>
#include <map>
#include <mutex>
#include <cstdint>

namespace nodo::crypto {

struct SignatureRequest {
    uint64_t height;
    uint32_t round;
    std::string proposalHash;
    std::string payloadToSign;
};

class OutofProcessSigner {
public:
    OutofProcessSigner(const std::string& keyId, const std::string& encryptedKeyEnvelope, const std::string& password);

    bool signBlockProposal(const SignatureRequest& request, std::string& signatureOut);
    bool signVote(const SignatureRequest& request, std::string& signatureOut);

    const std::string& validatorAddress() const;

private:
    std::string m_keyId;
    std::string m_decryptedPrivateKey;
    std::string m_validatorAddress;
    mutable std::mutex m_mutex;

    uint64_t m_lastSignedProposalHeight{0};
    uint32_t m_lastSignedProposalRound{0};
    std::string m_lastSignedProposalHash;

    uint64_t m_lastSignedVoteHeight{0};
    uint32_t m_lastSignedVoteRound{0};
    std::string m_lastSignedVoteHash;

    bool decryptKey(const std::string& envelope, const std::string& password);
};

} // namespace nodo::crypto

#endif
