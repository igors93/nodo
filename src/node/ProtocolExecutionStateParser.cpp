#include "node/ProtocolExecutionStateParser.hpp"
#include "node/ProtocolDomainCodec.hpp"

namespace nodo::node {

ProtocolExecutionState ProtocolExecutionStateParser::parse(
    const std::map<std::string, std::string> &domains) {
  return ProtocolDomainCodec::decodeState(domains);
}

} // namespace nodo::node
