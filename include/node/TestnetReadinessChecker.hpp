#ifndef NODO_NODE_TESTNET_READINESS_CHECKER_HPP
#define NODO_NODE_TESTNET_READINESS_CHECKER_HPP

#include "config/NetworkParameters.hpp"
#include "crypto/KeyStore.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

enum class ReadinessStatus {
    READY,
    NOT_READY
};

std::string readinessStatusToString(ReadinessStatus status);

class ReadinessDiagnostic {
public:
    ReadinessDiagnostic();
    ReadinessDiagnostic(std::string checkName, bool passed, std::string detail);

    const std::string& checkName() const;
    bool passed() const;
    const std::string& detail() const;

    std::string serialize() const;

private:
    std::string m_checkName;
    bool m_passed;
    std::string m_detail;
};

/*
 * TestnetReadinessChecker evaluates whether a node meets the minimum
 * requirements to start on an official network.
 *
 * Security principle:
 * A node must not silently join a testnet or mainnet if it is misconfigured.
 * This checker produces a human-readable list of checks with pass/fail
 * status so that an operator knows exactly what must be fixed before the
 * node can participate.
 */
class TestnetReadinessChecker {
public:
    static std::vector<ReadinessDiagnostic> check(
        const config::NetworkParameters& params,
        const crypto::StoredKeyMetadata& validatorKey,
        std::size_t connectedPeers,
        bool genesisVerified,
        std::uint64_t finalizedHeight
    );

    static ReadinessStatus summarize(
        const std::vector<ReadinessDiagnostic>& checks
    );
};

} // namespace nodo::node

#endif
