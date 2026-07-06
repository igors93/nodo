#ifndef NODO_NODE_FINALIZED_ARTIFACT_SCHEMA_HPP
#define NODO_NODE_FINALIZED_ARTIFACT_SCHEMA_HPP

#include <string>

namespace nodo::node {

class FinalizedArtifactSchema {
public:
  static const std::string &currentSchemaId();
  static bool isCurrentSchema(const std::string &schemaId);
  static bool hasVersionSuffix(const std::string &schemaId);
};

} // namespace nodo::node

#endif
