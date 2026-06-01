#ifndef NODO_CRYPTO_KEY_ENCRYPTION_POLICY_HPP
#define NODO_CRYPTO_KEY_ENCRYPTION_POLICY_HPP

#include <string>

namespace nodo::crypto {

/*
 * KeyEncryptionLevel describes the protection tier of a stored private key.
 *
 * Security principle:
 * PLAINTEXT keys are development artifacts. They must never be used on any
 * officially networked environment. TESTNET_SAFE marks keys that pass a
 * minimum encryption bar for testnet use. MAINNET_SAFE is intentionally not
 * implemented — mainnet requires an audited HSM or KMS provider that does
 * not exist in this codebase yet.
 */
enum class KeyEncryptionLevel {
    PLAINTEXT,
    DEV_ENCRYPTED,
    TESTNET_SAFE
};

std::string keyEncryptionLevelToString(KeyEncryptionLevel level);

/*
 * KeyEncryptionPolicy maps network profiles to their minimum required
 * key encryption level and enforces that mapping.
 */
class KeyEncryptionPolicy {
public:
    static KeyEncryptionLevel requiredLevelForNetwork(
        const std::string& networkName
    );

    static bool isAcceptable(
        KeyEncryptionLevel actual,
        const std::string& networkName
    );

    static bool isMainnetBlocked(
        const std::string& networkName
    );

    static bool isOfficialNetwork(
        const std::string& networkName
    );
};

} // namespace nodo::crypto

#endif
