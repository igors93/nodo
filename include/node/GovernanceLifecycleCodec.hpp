#ifndef NODO_NODE_GOVERNANCE_LIFECYCLE_CODEC_HPP
#define NODO_NODE_GOVERNANCE_LIFECYCLE_CODEC_HPP

#include "economics/GovernanceLifecycleRecord.hpp"
#include "serialization/KeyValueFileCodec.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

class GovernanceLifecycleCodec {
public:
    using FieldList = std::vector<std::pair<std::string, std::string>>;

    static const std::string& schemaId();

    static std::string encode(
        const economics::GovernanceLifecycleRecord& lifecycle
    );

    static economics::GovernanceLifecycleRecord decode(
        const std::string& contents
    );

    static void appendFields(
        const economics::GovernanceLifecycleRecord& lifecycle,
        const std::string& prefix,
        FieldList& fields
    );

    static void addAllowedFields(
        const serialization::KeyValueFileDocument& doc,
        const std::string& prefix,
        std::set<std::string>& allowed
    );

    static economics::GovernanceLifecycleRecord decodeFromDocument(
        const serialization::KeyValueFileDocument& doc,
        const std::string& prefix
    );
};

} // namespace nodo::node

#endif
