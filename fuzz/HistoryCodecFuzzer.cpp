#include "archive/ArchivalChallenge.hpp"
#include "archive/ArchivalProof.hpp"
#include "archive/ArchiveProvider.hpp"
#include "archive/ArchiveSegment.hpp"
#include "node/history/FinalizedStateCheckpoint.hpp"
#include "node/history/FullProtocolStateSnapshot.hpp"
#include "node/history/HistorySyncMessages.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>

// Every decoder that reads untrusted bounded-history input (ADR 0013). The
// first byte selects the decoder; malformed input must only ever throw.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 1 || size > 1 << 20) {
    return 0;
  }
  const std::vector<unsigned char> bytes(data + 1, data + size);
  try {
    switch (data[0] % 7) {
    case 0:
      (void)nodo::node::FinalizedStateCheckpoint::decode(bytes);
      break;
    case 1:
      (void)nodo::node::FullProtocolStateSnapshot::decode(bytes, 1 << 20);
      break;
    case 2:
      (void)nodo::archive::ArchiveSegmentCommitment::decode(bytes);
      break;
    case 3:
      (void)nodo::archive::ArchivalChallenge::decode(bytes);
      break;
    case 4:
      (void)nodo::archive::ArchivalProof::decode(bytes, 1 << 20);
      break;
    case 5:
      (void)nodo::archive::ArchiveProviderRegistration::decode(bytes);
      break;
    default:
      (void)nodo::node::HistorySyncCodec::decode(bytes);
      break;
    }
  } catch (const std::exception &) {
    // Expected for malformed input; crashes and memory errors are not.
  }
  return 0;
}
