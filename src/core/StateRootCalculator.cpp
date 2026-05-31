#include "core/StateRootCalculator.hpp"

#include "crypto/hash.h"

#include <sstream>

namespace nodo::core {

std::string StateRootCalculator::calculateAccountStateRoot(
    const AccountStateView& view
) {
    if (!view.isValid()) {
        return "";
    }

    char output[NODO_HASH_BUFFER_SIZE] = {0};

    const std::string payload =
        canonicalAccountStatePayload(view);

    nodo_hash_string(
        payload.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output);
}

std::string StateRootCalculator::canonicalAccountStatePayload(
    const AccountStateView& view
) {
    std::ostringstream oss;

    oss << "NODO_ACCOUNT_STATE_ROOT_V1{"
        << "accountCount=" << view.accounts().size()
        << ";accounts=[";

    const std::vector<AccountState> accounts =
        view.accounts();

    for (std::size_t index = 0; index < accounts.size(); ++index) {
        if (index != 0) {
            oss << ",";
        }

        oss << accounts[index].serialize();
    }

    oss << "]}";

    return oss.str();
}

} // namespace nodo::core
