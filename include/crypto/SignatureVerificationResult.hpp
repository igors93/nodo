#ifndef NODO_CRYPTO_SIGNATURE_VERIFICATION_RESULT_HPP
#define NODO_CRYPTO_SIGNATURE_VERIFICATION_RESULT_HPP

#include <string>
#include <utility>

namespace nodo::crypto {

/*
 * SignatureVerificationResult gives provider verification a clear outcome.
 *
 * In simple terms:
 * - success=true means the provider accepted the signature for the message;
 * - success=false means the provider rejected it and explains why.
 */
class SignatureVerificationResult {
public:
    static SignatureVerificationResult valid() {
        return SignatureVerificationResult(true, "");
    }

    static SignatureVerificationResult invalid(
        std::string reason
    ) {
        return SignatureVerificationResult(false, std::move(reason));
    }

    bool success() const {
        return m_success;
    }

    const std::string& failureReason() const {
        return m_failureReason;
    }

private:
    SignatureVerificationResult(
        bool success,
        std::string failureReason
    )
        : m_success(success),
          m_failureReason(std::move(failureReason)) {}

    bool m_success;
    std::string m_failureReason;
};

} // namespace nodo::crypto

#endif
