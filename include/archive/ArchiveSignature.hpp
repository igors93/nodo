#ifndef NODO_ARCHIVE_ARCHIVE_SIGNATURE_HPP
#define NODO_ARCHIVE_ARCHIVE_SIGNATURE_HPP

#include "crypto/KeyPair.hpp"
#include "crypto/Signature.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nodo::archive {

/*
 * Ed25519 signatures for archive provider objects under their own signing
 * domain (NODO_ARCHIVAL_PROOF_V1), so a registration or proof signature can
 * never be replayed as a transaction, vote or handshake signature. The signed
 * message is a purpose-separated digest of the canonical body. Object ids
 * exclude the signature, so the unsigned signature timestamp cannot change
 * an object's identity.
 */
class ArchiveSignature {
public:
  static crypto::Signature sign(const std::vector<unsigned char> &body,
                                std::string_view purpose,
                                const crypto::KeyPair &key,
                                std::int64_t signedAt);

  static bool verify(const std::vector<unsigned char> &body,
                     std::string_view purpose,
                     const crypto::Signature &signature,
                     const std::string &expectedPublicKeyHex);

  static void write(serialization::CanonicalWriter &writer,
                    const crypto::Signature &signature);
  static crypto::Signature read(serialization::CanonicalReader &reader);
};

} // namespace nodo::archive

#endif
