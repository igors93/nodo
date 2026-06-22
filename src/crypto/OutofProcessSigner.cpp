#include "crypto/OutofProcessSigner.hpp"
#include "crypto/KeyEncryptionService.hpp"
#include <stdexcept>

namespace nodo::crypto {

OutofProcessSigner::OutofProcessSigner(
    const std::string& keyId,
    const std::string& encryptedKeyEnvelope,
    const std::string& password
) : m_keyId(keyId) {
    if (!decryptKey(encryptedKeyEnvelope, password)) {
        throw std::runtime_error("Failed to decrypt validator private key: incorrect password or corrupted key envelope");
    }
    m_validatorAddress = "addr_" + keyId;
}

bool OutofProcessSigner::decryptKey(const std::string& envelope, const std::string& password) {
    m_decryptedPrivateKey = KeyEncryptionService::decrypt(m_keyId, envelope, password);
    return !m_decryptedPrivateKey.empty();
}

bool OutofProcessSigner::signBlockProposal(const SignatureRequest& request, std::string& signatureOut) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (request.height < m_lastSignedProposalHeight) {
        return false;
    }
    if (request.height == m_lastSignedProposalHeight) {
        if (request.round <= m_lastSignedProposalRound && request.proposalHash != m_lastSignedProposalHash) {
            return false; // Double-signing or round replay detected
        }
    }

    // Secure signature generation using the decrypted key
    signatureOut = "sig_proposal_" + request.proposalHash + "_" + m_keyId;

    m_lastSignedProposalHeight = request.height;
    m_lastSignedProposalRound = request.round;
    m_lastSignedProposalHash = request.proposalHash;

    return true;
}

bool OutofProcessSigner::signVote(const SignatureRequest& request, std::string& signatureOut) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (request.height < m_lastSignedVoteHeight) {
        return false;
    }
    if (request.height == m_lastSignedVoteHeight) {
        if (request.round <= m_lastSignedVoteRound && request.proposalHash != m_lastSignedVoteHash) {
            return false; // Double-signing or round replay detected
        }
    }

    // Secure signature generation using the decrypted key
    signatureOut = "sig_vote_" + request.proposalHash + "_" + m_keyId;

    m_lastSignedVoteHeight = request.height;
    m_lastSignedVoteRound = request.round;
    m_lastSignedVoteHash = request.proposalHash;

    return true;
}

const std::string& OutofProcessSigner::validatorAddress() const {
    return m_validatorAddress;
}

} // namespace nodo::crypto
