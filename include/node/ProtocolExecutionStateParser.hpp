#ifndef NODO_NODE_PROTOCOL_EXECUTION_STATE_PARSER_HPP
#define NODO_NODE_PROTOCOL_EXECUTION_STATE_PARSER_HPP

#include "node/ProtocolTransactionDomainExecutor.hpp"
#include <map>
#include <string>

namespace nodo::node {

class ProtocolExecutionStateParser {
public:
  static ProtocolExecutionState
  parse(const std::map<std::string, std::string> &domains);
};

} // namespace nodo::node

#endif
