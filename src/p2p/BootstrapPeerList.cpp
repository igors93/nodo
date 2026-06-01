#include "p2p/BootstrapPeerList.hpp"

#include <fstream>
#include <sstream>

namespace nodo::p2p {

bool BootstrapPeerList::isValidPeer(const PeerEndpoint& endpoint) {
    return endpoint.isValid();
}

bool BootstrapPeerList::validateAll(
    const std::vector<PeerEndpoint>& peers,
    std::string& reason
) {
    if (peers.empty()) {
        reason = "Bootstrap peer list is empty.";
        return false;
    }
    for (const auto& peer : peers) {
        if (!isValidPeer(peer)) {
            reason = "Invalid bootstrap peer: host='" + peer.host() +
                     "' port=" + std::to_string(peer.port());
            return false;
        }
    }
    reason = "";
    return true;
}

std::vector<PeerEndpoint> BootstrapPeerList::parseFromLines(
    const std::vector<std::string>& lines
) {
    std::vector<PeerEndpoint> result;
    for (const auto& line : lines) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto colonPos = line.rfind(':');
        if (colonPos == std::string::npos) {
            continue;
        }
        const std::string host = line.substr(0, colonPos);
        const std::string portStr = line.substr(colonPos + 1);
        if (host.empty() || portStr.empty()) {
            continue;
        }
        try {
            const int port = std::stoi(portStr);
            if (port <= 0 || port > 65535) {
                continue;
            }
            result.emplace_back(host, static_cast<std::uint16_t>(port));
        } catch (...) {
            continue;
        }
    }
    return result;
}

std::vector<PeerEndpoint> BootstrapPeerList::loadFromFile(
    const std::filesystem::path& path,
    std::string& reason
) {
    if (!std::filesystem::exists(path)) {
        reason = "Bootstrap peer file not found: " + path.string();
        return {};
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        reason = "Cannot open bootstrap peer file: " + path.string();
        return {};
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    const auto peers = parseFromLines(lines);
    if (peers.empty()) {
        reason = "No valid peers found in bootstrap file: " + path.string();
    }
    return peers;
}

} // namespace nodo::p2p
