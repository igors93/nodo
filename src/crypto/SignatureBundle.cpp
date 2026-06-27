#include "crypto/SignatureBundle.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/SigningDomain.hpp"

#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace nodo::crypto {

namespace {

std::vector<std::string> splitAtDepthZero(const std::string& body, char sep) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    int depth = 0;
    for (std::size_t i = 0; i < body.size(); ++i) {
        const char c = body[i];
        if (c == '{') ++depth;
        else if (c == '}') --depth;
        if (depth < 0) throw std::invalid_argument("Unbalanced braces in SignatureBundle.");
        if (depth == 0 && c == sep) {
            if (i == start) throw std::invalid_argument("Empty field in SignatureBundle.");
            parts.push_back(body.substr(start, i - start));
            start = i + 1;
        }
    }
    if (depth != 0) throw std::invalid_argument("Unbalanced braces in SignatureBundle body.");
    if (start < body.size()) parts.push_back(body.substr(start));
    return parts;
}

std::string extractBody(const std::string& serialized, const char* typeName) {
    const std::string prefix = std::string(typeName) + "{";
    if (serialized.size() <= prefix.size() ||
        serialized.compare(0, prefix.size(), prefix) != 0 ||
        serialized.back() != '}') {
        throw std::invalid_argument(std::string("Serialized data is not a ") + typeName + ".");
    }
    return serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);
}

std::map<std::string, std::string> parseFields(const std::string& body) {
    std::map<std::string, std::string> result;
    const std::vector<std::string> tokens = splitAtDepthZero(body, ';');
    for (const auto& token : tokens) {
        if (token.empty()) continue;
        const auto eq = token.find('=');
        if (eq == std::string::npos || eq == 0)
            throw std::invalid_argument("Malformed field: " + token);
        const std::string key = token.substr(0, eq);
        const std::string val = token.substr(eq + 1);
        if (!result.emplace(key, val).second)
            throw std::invalid_argument("Duplicate field: " + key);
    }
    return result;
}

std::string requireField(const std::map<std::string, std::string>& fields, const std::string& key) {
    const auto it = fields.find(key);
    if (it == fields.end()) throw std::invalid_argument("Missing field: " + key);
    return it->second;
}

std::int64_t parseI64(const std::string& value) {
    if (value.empty()) throw std::invalid_argument("Empty integer field.");
    std::size_t n = 0;
    const long long parsed = std::stoll(value, &n);
    if (n != value.size()) throw std::invalid_argument("Non-numeric integer field: " + value);
    return static_cast<std::int64_t>(parsed);
}

Signature parseSignatureChunk(const std::string& chunk, const PublicKey& expectedKey) {
    const std::string body = extractBody(chunk, "Signature");
    const std::map<std::string, std::string> fields = parseFields(body);

    const CryptoAlgorithm algorithm =
        cryptoAlgorithmFromString(requireField(fields, "algorithm"));
    const CryptoSuiteId suite =
        cryptoSuiteIdFromString(requireField(fields, "suite"));
    const SigningDomain domain =
        signingDomainFromString(requireField(fields, "domain"));

    if (domain == SigningDomain::UNKNOWN)
        throw std::invalid_argument("Unknown SigningDomain in Signature.");
    if (algorithm != expectedKey.algorithm())
        throw std::invalid_argument("Signature algorithm does not match expected public key.");
    if (requireField(fields, "publicKeyFingerprint") != expectedKey.fingerprint())
        throw std::invalid_argument("Signature public key fingerprint does not match expected public key.");

    Signature sig(
        suite,
        domain,
        algorithm,
        expectedKey,
        requireField(fields, "signatureHex"),
        parseI64(requireField(fields, "createdAt"))
    );

    if (!sig.isValid() || sig.serialize() != chunk)
        throw std::invalid_argument("Signature is invalid or non-canonical.");

    return sig;
}

} // namespace

SignatureBundle::SignatureBundle() = default;

void SignatureBundle::addSignature(const Signature& signature) {
    if (!signature.isValid()) {
        throw std::invalid_argument("Invalid signature rejected by SignatureBundle.");
    }

    /*
     * Basic safety:
     * Do not allow two signatures with the same algorithm in the same bundle.
     * This can be revisited later for multisig.
     */
    for (const auto& existing : m_signatures) {
        if (existing.algorithm() == signature.algorithm()) {
            throw std::logic_error("Duplicated signature algorithm rejected.");
        }
    }

    m_signatures.push_back(signature);
}

const std::vector<Signature>& SignatureBundle::signatures() const {
    return m_signatures;
}

bool SignatureBundle::empty() const {
    return m_signatures.empty();
}

bool SignatureBundle::hasAlgorithm(CryptoAlgorithm algorithm) const {
    for (const auto& signature : m_signatures) {
        if (signature.algorithm() == algorithm) {
            return true;
        }
    }

    return false;
}

bool SignatureBundle::isValidForPolicy(
    const CryptoPolicy& policy,
    SecurityContext context
) const {
    if (m_signatures.empty()) {
        return false;
    }

    for (const auto& signature : m_signatures) {
        if (!signature.isValid()) {
            return false;
        }

        if (!policy.isAlgorithmAllowed(signature.algorithm(), context)) {
            return false;
        }

        if (!isSigningDomainAllowedForContext(signature.domain(), context)) {
            return false;
        }
    }

    return true;
}

bool SignatureBundle::verifyForPolicy(
    const std::string& message,
    const CryptoPolicy& policy,
    SecurityContext context,
    const SignatureProvider& provider
) const {
    if (!isValidForPolicy(policy, context)) {
        return false;
    }

    if (message.empty()) {
        return false;
    }

    for (const auto& signature : m_signatures) {
        if (signature.algorithm() != provider.algorithm()) {
            return false;
        }

        const SignatureVerificationResult result =
            provider.verify(
                message,
                signature
            );

        if (!result.success()) {
            return false;
        }
    }

    return true;
}

std::string SignatureBundle::serialize() const {
    std::ostringstream oss;

    oss << "SignatureBundle{";

    for (std::size_t i = 0; i < m_signatures.size(); ++i) {
        if (i > 0) {
            oss << ";";
        }

        oss << m_signatures[i].serialize();
    }

    oss << "}";

    return oss.str();
}

// static
SignatureBundle SignatureBundle::deserialize(
    const std::string& serialized,
    const PublicKey& expectedPublicKey
) {
    if (!expectedPublicKey.isValid())
        throw std::invalid_argument("SignatureBundle::deserialize requires a valid expected public key.");

    const std::string body = extractBody(serialized, "SignatureBundle");
    if (body.empty())
        throw std::invalid_argument("SignatureBundle body is empty.");

    SignatureBundle bundle;
    for (const std::string& chunk : splitAtDepthZero(body, ';')) {
        bundle.addSignature(parseSignatureChunk(chunk, expectedPublicKey));
    }

    if (bundle.serialize() != serialized)
        throw std::invalid_argument("SignatureBundle is non-canonical after deserialization.");

    return bundle;
}

SignatureBundle SignatureBundle::createSignature(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp,
    const SignatureProvider& provider,
    SigningDomain domain
) {
    SignatureBundle bundle;

    bundle.addSignature(
        provider.sign(
            message,
            publicKey,
            privateKey,
            timestamp,
            domain
        )
    );

    return bundle;
}

} // namespace nodo::crypto
