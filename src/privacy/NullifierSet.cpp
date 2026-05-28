#include "privacy/NullifierSet.hpp"

#include <sstream>
#include <stdexcept>

namespace nodo::privacy {

NullifierSet::NullifierSet() = default;

std::size_t NullifierSet::size() const {
    return m_usedNullifierHashes.size();
}

bool NullifierSet::empty() const {
    return m_usedNullifierHashes.empty();
}

bool NullifierSet::containsNullifierHash(
    const std::string& nullifierHash
) const {
    if (nullifierHash.empty()) {
        return false;
    }

    for (const auto& usedHash : m_usedNullifierHashes) {
        if (usedHash == nullifierHash) {
            return true;
        }
    }

    return false;
}

bool NullifierSet::canRegisterNullifier(
    const PrivacyNullifier& nullifier
) const {
    if (!nullifier.isValid()) {
        return false;
    }

    return !containsNullifierHash(nullifier.nullifierHash());
}

void NullifierSet::registerNullifier(
    const PrivacyNullifier& nullifier
) {
    if (!nullifier.isValid()) {
        throw std::invalid_argument("Invalid privacy nullifier rejected.");
    }

    if (containsNullifierHash(nullifier.nullifierHash())) {
        throw std::logic_error("Duplicate privacy nullifier rejected.");
    }

    m_usedNullifierHashes.push_back(nullifier.nullifierHash());
}

std::string NullifierSet::serialize() const {
    std::ostringstream oss;

    oss << "NullifierSet{"
        << "size=" << m_usedNullifierHashes.size()
        << ";usedNullifierHashes=[";

    for (std::size_t i = 0; i < m_usedNullifierHashes.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << m_usedNullifierHashes[i];
    }

    oss << "]}";

    return oss.str();
}

} // namespace nodo::privacy