#ifndef NODO_ECONOMICS_MINT_AUTHORIZATION_HPP
#define NODO_ECONOMICS_MINT_AUTHORIZATION_HPP

#include "economics/MonetaryPolicy.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * MintAuthorization is the explicit permission grant for a mint operation.
 *
 * Security principle:
 * No NODO can be created without a MintAuthorization that is:
 *   - valid (all fields non-empty, amount positive, epochs consistent);
 *   - active at the epoch of the mint;
 *   - matched by authorizationId in the corresponding MintRecord.
 *
 * This class is not full governance. It is the minimal authorization object
 * that MonetaryFirewall requires. Future treasury/governance logic will produce
 * MintAuthorization records; for now they are constructed directly.
 */
class MintAuthorization {
public:
    MintAuthorization();

    MintAuthorization(
        std::string authorizationId,
        std::string policyVersion,
        std::uint64_t epoch,
        std::uint64_t expiresAtEpoch,
        utils::Amount maxMintAmount,
        std::string reason,
        std::string approvedBy
    );

    /*
     * Factory for bootstrap/localnet/genesis authorizations.
     *
     * Creates a minimal valid authorization bound to the policy's version and
     * epoch 0 (genesis). Use this instead of constructing authorization strings
     * manually in demo or test code.
     */
    static MintAuthorization createGenesisAuthorization(
        const MonetaryPolicy& policy,
        const std::string& authorizationId,
        utils::Amount maxMintAmount
    );

    const std::string& authorizationId() const;
    const std::string& policyVersion() const;
    std::uint64_t epoch() const;
    std::uint64_t expiresAtEpoch() const;
    utils::Amount maxMintAmount() const;
    const std::string& reason() const;
    const std::string& approvedBy() const;

    bool isValid() const;
    bool isActiveAtEpoch(std::uint64_t currentEpoch) const;
    std::string rejectionReason() const;
    std::string serialize() const;

private:
    std::string m_authorizationId;
    std::string m_policyVersion;
    std::uint64_t m_epoch;
    std::uint64_t m_expiresAtEpoch;
    utils::Amount m_maxMintAmount;
    std::string m_reason;
    std::string m_approvedBy;
};

} // namespace nodo::economics

#endif
