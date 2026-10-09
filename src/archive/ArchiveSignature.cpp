#include "archive/ArchiveSignature.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"

#include <stdexcept>

namespace nodo::archive {

namespace {

std::string signedMessage(const std::vector<unsigned char> &body,
                          std::string_view purpose) {
  return hashHex(purpose, body);
}

const crypto::Ed25519SignatureProvider &provider() {
  static const crypto::Ed25519SignatureProvider instance;
  return instance;
}

} // namespace

crypto::Signature ArchiveSignature::sign(const std::vector<unsigned char> &body,
                                         std::string_view purpose,
                                         const crypto::KeyPair &key,
                                         std::int64_t signedAt) {
  if (key.algorithm() != crypto::CryptoAlgorithm::CLASSIC_ED25519) {
    throw std::invalid_argument("Archive providers sign with Ed25519 keys.");
  }
  return provider().sign(signedMessage(body, purpose), key.publicKey(),
                         key.privateKeyForSigningOnly(), signedAt,
                         crypto::SigningDomain::ARCHIVAL_PROOF);
}

bool ArchiveSignature::verify(const std::vector<unsigned char> &body,
                              std::string_view purpose,
                              const crypto::Signature &signature,
                              const std::string &expectedPublicKeyHex) {
  try {
    if (signature.domain() != crypto::SigningDomain::ARCHIVAL_PROOF ||
        signature.algorithm() != crypto::CryptoAlgorithm::CLASSIC_ED25519 ||
        signature.publicKey().keyMaterial() != expectedPublicKeyHex) {
      return false;
    }
    return provider().verify(signedMessage(body, purpose), signature).success();
  } catch (const std::exception &) {
    return false;
  }
}

void ArchiveSignature::write(serialization::CanonicalWriter &writer,
                             const crypto::Signature &signature) {
  writer.writeString(signature.publicKey().keyMaterial());
  writer.writeString(signature.signatureHex());
  writer.writeInt64(signature.createdAt());
}

crypto::Signature ArchiveSignature::read(serialization::CanonicalReader &reader) {
  std::string keyMaterial = reader.readString();
  std::string signatureHex = reader.readString();
  const std::int64_t createdAt = reader.readInt64();
  return crypto::Signature(
      crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
      crypto::SigningDomain::ARCHIVAL_PROOF,
      crypto::CryptoAlgorithm::CLASSIC_ED25519,
      crypto::PublicKey(crypto::CryptoAlgorithm::CLASSIC_ED25519,
                        std::move(keyMaterial)),
      std::move(signatureHex), createdAt);
}

} // namespace nodo::archive
