#ifndef NODO_CORE_STATE_ROOT_CALCULATOR_HPP
#define NODO_CORE_STATE_ROOT_CALCULATOR_HPP

#include "core/AccountStateView.hpp"

#include <string>
#include <map>

namespace nodo::core {

class StateRootCalculator {
public:
    static std::string calculateAccountStateRoot(
        const AccountStateView& view
    );

    static std::string canonicalAccountStatePayload(
        const AccountStateView& view
    );

    static std::string calculateProtocolStateRoot(
        const AccountStateView& view,
        const std::map<std::string, std::string>& deterministicDomains
    );
};

} // namespace nodo::core

#endif
