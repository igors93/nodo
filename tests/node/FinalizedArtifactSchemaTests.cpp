#include "node/FinalizedArtifactSchema.hpp"

#include <cassert>
#include <string>

namespace {

void testCurrentSchemaIsVersionless() {
    const std::string& schemaId =
        nodo::node::FinalizedArtifactSchema::currentSchemaId();

    assert(schemaId == "NODO_FINALIZED_BLOCK_ARTIFACT");
    assert(!nodo::node::FinalizedArtifactSchema::hasVersionSuffix(schemaId));
    assert(nodo::node::FinalizedArtifactSchema::isCurrentSchema(schemaId));
}

void testLegacyVersionedSchemasAreRejected() {
    assert(!nodo::node::FinalizedArtifactSchema::isCurrentSchema("NODO_FINALIZED_BLOCK_V19"));
    assert(!nodo::node::FinalizedArtifactSchema::isCurrentSchema("NODO_FINALIZED_BLOCK_V20"));
    assert(nodo::node::FinalizedArtifactSchema::hasVersionSuffix("NODO_FINALIZED_BLOCK_V19"));
    assert(nodo::node::FinalizedArtifactSchema::hasVersionSuffix("NODO_FINALIZED_BLOCK_V20"));
}

} // namespace

int main() {
    testCurrentSchemaIsVersionless();
    testLegacyVersionedSchemasAreRejected();
    return 0;
}
