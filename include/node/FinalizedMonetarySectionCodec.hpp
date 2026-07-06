#ifndef NODO_NODE_FINALIZED_MONETARY_SECTION_CODEC_HPP
#define NODO_NODE_FINALIZED_MONETARY_SECTION_CODEC_HPP

#include "economics/SupplyDelta.hpp"
#include "serialization/KeyValueFileCodec.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

class FinalizedMonetarySectionCodec {
public:
  using FieldList = std::vector<std::pair<std::string, std::string>>;

  static std::size_t
  mintRecordCount(const serialization::KeyValueFileDocument &document);

  static std::size_t
  burnRecordCount(const serialization::KeyValueFileDocument &document);

  static void addAllowedFields(std::set<std::string> &allowedFields,
                               std::size_t mintRecordCount,
                               std::size_t burnRecordCount);

  static economics::SupplyDelta
  decodeSupplyDelta(const serialization::KeyValueFileDocument &document,
                    std::uint64_t expectedBlockHeight,
                    const std::string &expectedBlockHash);

  static void appendSupplyDeltaFields(const economics::SupplyDelta &supplyDelta,
                                      FieldList &fields);
};

} // namespace nodo::node

#endif
