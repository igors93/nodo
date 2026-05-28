#ifndef NODO_PRIVACY_NULLIFIER_SET_HPP
#define NODO_PRIVACY_NULLIFIER_SET_HPP

#include "privacy/PrivacyNullifier.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::privacy {

/*
 * NullifierSet stores already-used privacy nullifiers.
 *
 * Security principle:
 * A private coin may hide its origin, owner and amount, but spending it must
 * produce a public nullifier that cannot be registered twice.
 */
class NullifierSet {
public:
    NullifierSet();

    std::size_t size() const;
    bool empty() const;

    bool containsNullifierHash(const std::string& nullifierHash) const;

    /*
     * Returns true only when the nullifier is structurally valid and has
     * not been registered before.
     */
    bool canRegisterNullifier(const PrivacyNullifier& nullifier) const;

    /*
     * Registers a nullifier.
     *
     * Security rule:
     * Registering the same nullifier twice must always fail.
     */
    void registerNullifier(const PrivacyNullifier& nullifier);

    std::string serialize() const;

private:
    std::vector<std::string> m_usedNullifierHashes;
};

} // namespace nodo::privacy

#endif