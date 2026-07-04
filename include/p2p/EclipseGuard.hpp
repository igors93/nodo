#ifndef NODO_P2P_ECLIPSE_GUARD_HPP
#define NODO_P2P_ECLIPSE_GUARD_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::p2p {

/*
 * PeerSubnetInfo identifies a peer's IP address and its /24 subnet prefix.
 * Eclipse attack mitigation operates at the /24 granularity to limit how many
 * peers an adversary can contribute from a single network block they control.
 */
struct PeerSubnetInfo {
  std::string peerId;
  std::string ipAddress;
  std::string subnetPrefix; // e.g., "192.168.1" for /24
  std::uint16_t port;

  static std::string extractSubnetPrefix(const std::string &ip);
  bool isValid() const;
  std::string serialize() const;
};

struct EclipseGuardConfig {
  std::size_t maxPeersPerSubnet;  // default: 2
  std::size_t maxTotalPeers;      // default: 50
  std::size_t minSubnetDiversity; // minimum distinct /24 subnets (default: 8)
  double
      maxSingleSubnetFraction; // max fraction from one subnet (default: 0.25)

  static EclipseGuardConfig defaults();
  bool isValid() const;
  std::string serialize() const;
};

enum class EclipseCheckOutcome {
  ALLOWED,
  REJECTED_SUBNET_SATURATED,
  REJECTED_DIVERSITY_INSUFFICIENT,
  REJECTED_CAPACITY_FULL
};

std::string eclipseCheckOutcomeToString(EclipseCheckOutcome outcome);

class EclipseCheckResult {
public:
  static EclipseCheckResult allowed();
  static EclipseCheckResult rejected(EclipseCheckOutcome reason,
                                     std::string detail);

  bool isAllowed() const;
  EclipseCheckOutcome outcome() const;
  const std::string &detail() const;
  std::string serialize() const;

private:
  EclipseCheckOutcome m_outcome;
  std::string m_detail;
  bool m_allowed;

  EclipseCheckResult(EclipseCheckOutcome outcome, std::string detail,
                     bool allowed);
};

/*
 * EclipseGuard prevents eclipse attacks by enforcing peer diversity rules.
 *
 * An eclipse attack fills all of a node's connection slots with peers
 * controlled by the attacker, isolating it from the honest network. Enforcing
 * /24 subnet diversity makes this geometrically expensive: the attacker must
 * control IPs across many distinct /24 blocks.
 *
 * Security principle:
 * No single /24 subnet may contribute more than maxPeersPerSubnet peers, and
 * no subnet may represent more than maxSingleSubnetFraction of total peers.
 */
class EclipseGuard {
public:
  explicit EclipseGuard(
      EclipseGuardConfig config = EclipseGuardConfig::defaults());

  // Check if a new peer can be admitted given the current peer set.
  EclipseCheckResult
  checkAdmission(const PeerSubnetInfo &candidate,
                 const std::vector<PeerSubnetInfo> &currentPeers) const;

  // Recommend peers to evict to make room for better diversity.
  // Returns peer IDs to evict (from over-represented subnets).
  std::vector<std::string>
  recommendEvictions(const std::vector<PeerSubnetInfo> &currentPeers,
                     std::size_t targetCount) const;

  // Score the current peer set's diversity (0.0 = no diversity, 1.0 = perfect).
  double diversityScore(const std::vector<PeerSubnetInfo> &peers) const;

  // Count peers per subnet.
  std::map<std::string, std::size_t>
  subnetCounts(const std::vector<PeerSubnetInfo> &peers) const;

  const EclipseGuardConfig &config() const;

private:
  EclipseGuardConfig m_config;
};

} // namespace nodo::p2p

#endif
