#ifndef NODO_NODE_LOCAL_NODE_IDENTITY_HPP
#define NODO_NODE_LOCAL_NODE_IDENTITY_HPP

#include <filesystem>
#include <string>

namespace nodo::node {

/*
 * LocalNodeIdentity describes one local node's configuration for a local
 * testnet: its unique identifier, P2P endpoint, validator key seed,
 * data directory, and the genesis it belongs to.
 *
 * Security principle:
 * Each field is validated before the identity is accepted. Duplicate node
 * identifiers and duplicate endpoints are rejected by LocalPeerTopology.
 * An identity without a data directory is invalid.
 */
class LocalNodeIdentity {
public:
    LocalNodeIdentity();

    LocalNodeIdentity(
        std::string nodeId,
        std::string endpoint,
        std::string validatorKeySeed,
        std::filesystem::path dataDirectory,
        std::string genesisId
    );

    const std::string& nodeId() const;
    const std::string& endpoint() const;
    const std::string& validatorKeySeed() const;
    const std::filesystem::path& dataDirectory() const;
    const std::string& genesisId() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_nodeId;
    std::string m_endpoint;
    std::string m_validatorKeySeed;
    std::filesystem::path m_dataDirectory;
    std::string m_genesisId;

    mutable bool m_validated = false;
    mutable bool m_valid = false;
    mutable std::string m_rejectionReason;

    void validate() const;
};

} // namespace nodo::node

#endif
