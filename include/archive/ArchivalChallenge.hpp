#ifndef NODO_ARCHIVE_ARCHIVAL_CHALLENGE_HPP
#define NODO_ARCHIVE_ARCHIVAL_CHALLENGE_HPP

#include "archive/ArchiveAssignment.hpp"
#include "archive/ArchiveSegment.hpp"
#include "config/HistoryParameters.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::archive {

struct ArchivalChallengeFields {
  std::string chainId;
  std::string providerId;
  std::uint64_t segmentIndex = 0;
  std::string segmentId;
  std::uint64_t pieceCount = 0;
  std::uint64_t round = 0;
  std::uint64_t seedHeight = 0;
  std::string seedBlockHash;
  std::uint64_t issueHeight = 0;
  std::uint64_t deadlineHeight = 0;
  std::vector<std::uint64_t> sampleIndices;
};

/*
 * An archival challenge is derived, never chosen (ADR 0014). Round r is
 * seeded by the hash of the block finalized at height r*I, which is unknown
 * when the provider's assignment was fixed:
 *
 *   seed = H(chainId, r, seedBlockHash, providerId, segmentIndex, segmentId)
 *   sample j = first distinct values of H(seed, j, attempt) reduced without
 *              modulo bias over the segment's pieces
 *
 * Any node recomputes the challenge from finalized data; an ordinary
 * challenger cannot pick easy pieces. The seed-block proposer can grind
 * candidate hashes, and a colluding provider may learn candidate samples.
 * The conditional G*f^k bound and its limits are stated in ADR 0014.
 */
class ArchivalChallenge {
public:
  static constexpr const char *SCHEMA = "NODO_ARCHIVAL_CHALLENGE_V1";
  static constexpr std::size_t kMaxEncodedBytes = 8192;

  ArchivalChallenge();
  explicit ArchivalChallenge(ArchivalChallengeFields fields);

  static ArchivalChallenge derive(const config::HistoryParameters &parameters,
                                  const std::string &providerId,
                                  const ArchiveSegmentCommitment &segment,
                                  std::uint64_t round,
                                  const std::string &seedBlockHash);

  // Every slot of the round, in slot order.
  static std::vector<ArchivalChallenge>
  forRound(const config::HistoryParameters &parameters,
           const std::vector<ArchiveSlot> &slots,
           const std::vector<ArchiveSegmentCommitment> &segments,
           std::uint64_t round, const std::string &seedBlockHash);

  static std::vector<std::uint64_t> sampleIndices(const std::string &seedHex,
                                                  std::uint64_t pieceCount,
                                                  std::uint32_t samples);

  const ArchivalChallengeFields &fields() const;
  bool isStructurallyValid() const;
  std::string seed() const;
  std::string challengeId() const;
  std::vector<unsigned char> encode() const;
  static ArchivalChallenge decode(const std::vector<unsigned char> &bytes);

private:
  ArchivalChallengeFields m_fields;
};

} // namespace nodo::archive

#endif
