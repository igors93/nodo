#ifndef NODO_ARCHIVE_ARCHIVE_PROVIDER_HPP
#define NODO_ARCHIVE_ARCHIVE_PROVIDER_HPP

#include "config/HistoryParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signature.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nodo::archive {

struct ArchiveProviderRegistrationFields {
  std::string chainId;
  // Address derived from the Ed25519 public key; the only provider identity.
  std::string providerId;
  std::string publicKeyHex;
  // Economic identity that posts the bond (an owner or staking address).
  // Replication counts distinct operators, never distinct provider keys.
  std::string operatorId;
  std::uint64_t bondRawUnits = 0;
  std::uint64_t declaredCapacityBytes = 0;
  std::uint64_t registeredHeight = 0;
};

/*
 * A provider's signed intent to preserve history (ADR 0014). No personal
 * data: identity is cryptographic, and the bond, not a claim, bounds how many
 * segment slots the provider may hold.
 */
class ArchiveProviderRegistration {
public:
  static constexpr const char *SCHEMA = "NODO_ARCHIVE_PROVIDER_REGISTRATION_V1";
  static constexpr std::size_t kMaxEncodedBytes = 4096;

  ArchiveProviderRegistration();
  ArchiveProviderRegistration(ArchiveProviderRegistrationFields fields,
                              crypto::Signature signature);

  static ArchiveProviderRegistration
  sign(ArchiveProviderRegistrationFields fields, const crypto::KeyPair &key,
       std::int64_t signedAt);

  const ArchiveProviderRegistrationFields &fields() const;
  const crypto::Signature &signature() const;

  bool isStructurallyValid() const;
  bool verifySignature() const;
  std::uint64_t maxSlots(const config::HistoryParameters &parameters) const;

  std::vector<unsigned char> encodeBody() const;
  std::string registrationId() const;
  std::vector<unsigned char> encode() const;
  static ArchiveProviderRegistration decode(const std::vector<unsigned char> &bytes);

private:
  ArchiveProviderRegistrationFields m_fields;
  crypto::Signature m_signature;
};

enum class ArchiveProviderStatus { PENDING, ACTIVE, REMOVED };

std::string archiveProviderStatusToString(ArchiveProviderStatus status);

struct ArchiveProviderRecord {
  ArchiveProviderRegistration registration;
  ArchiveProviderStatus status = ArchiveProviderStatus::PENDING;
  std::uint64_t activationHeight = 0;
  // H(providerId, hash of the block finalized at activationHeight). Unknown
  // at registration time, so a provider cannot grind keys to land on a
  // chosen segment.
  std::string assignmentKey;
  std::uint32_t successfulEpochStreak = 0;
  std::uint32_t consecutiveFailedEpochs = 0;
  std::uint64_t removedAtHeight = 0;
  std::string removalReason;
};

enum class ArchiveRegistrationStatus {
  REGISTERED,
  MALFORMED,
  BAD_SIGNATURE,
  WRONG_CHAIN,
  INSUFFICIENT_BOND,
  DUPLICATE_PROVIDER
};

std::string
archiveRegistrationStatusToString(ArchiveRegistrationStatus status);

/*
 * Deterministic provider registry. Every node applying the same finalized
 * registrations, activations and removals in the same order derives the
 * same digest.
 */
class ArchiveProviderRegistry {
public:
  explicit ArchiveProviderRegistry(std::string chainId = "");

  ArchiveRegistrationStatus
  registerProvider(const ArchiveProviderRegistration &registration,
                   const config::HistoryParameters &parameters);

  // Activates every pending provider whose activation height is `height`,
  // keyed by the hash of the block finalized at that height.
  std::size_t activateAt(std::uint64_t height, const std::string &blockHash);

  bool remove(const std::string &providerId, std::uint64_t height,
              const std::string &reason);

  ArchiveProviderRecord *find(const std::string &providerId);
  const ArchiveProviderRecord *find(const std::string &providerId) const;
  std::vector<const ArchiveProviderRecord *> activeProviders() const;
  std::size_t size() const;
  const std::string &chainId() const;
  std::string digest() const;

private:
  std::string m_chainId;
  std::map<std::string, ArchiveProviderRecord> m_records;
};

} // namespace nodo::archive

#endif
