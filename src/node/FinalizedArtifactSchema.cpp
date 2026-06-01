#include "node/FinalizedArtifactSchema.hpp"

#include <cctype>

namespace nodo::node {

const std::string& FinalizedArtifactSchema::currentSchemaId() {
    static const std::string schemaId = "NODO_FINALIZED_BLOCK_ARTIFACT";
    return schemaId;
}

bool FinalizedArtifactSchema::isCurrentSchema(
    const std::string& schemaId
) {
    return schemaId == currentSchemaId();
}

bool FinalizedArtifactSchema::hasVersionSuffix(
    const std::string& schemaId
) {
    const std::size_t marker = schemaId.rfind("_V");
    if (marker == std::string::npos ||
        marker + 2 >= schemaId.size()) {
        return false;
    }

    for (std::size_t index = marker + 2; index < schemaId.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(schemaId[index]))) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::node
