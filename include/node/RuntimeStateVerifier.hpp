#ifndef NODO_NODE_RUNTIME_STATE_VERIFIER_HPP
#define NODO_NODE_RUNTIME_STATE_VERIFIER_HPP

#include "config/NetworkParameters.hpp"
#include "core/Blockchain.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"

#include <string>

namespace nodo::node {

class RuntimeStateVerificationResult {
public:
    RuntimeStateVerificationResult();

    static RuntimeStateVerificationResult passed(
        std::string latestStateRoot
    );

    static RuntimeStateVerificationResult failed(
        std::string reason
    );

    bool verified() const;
    const std::string& reason() const;
    const std::string& latestStateRoot() const;

private:
    bool m_verified;
    std::string m_reason;
    std::string m_latestStateRoot;
};

class RuntimeStateVerifier {
public:
    static RuntimeStateVerificationResult verifyManifestMatchesRuntime(
        const NodeRuntimeManifest& manifest,
        const NodeRuntime& runtime
    );

    static RuntimeStateVerificationResult verifyLatestStateRoot(
        const config::GenesisConfig& genesisConfig,
        const core::Blockchain& blockchain,
        const std::string& expectedLatestStateRoot
    );

    static std::string calculateLatestStateRoot(
        const config::GenesisConfig& genesisConfig,
        const core::Blockchain& blockchain
    );
};

} // namespace nodo::node

#endif
