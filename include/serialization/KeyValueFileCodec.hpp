#ifndef NODO_SERIALIZATION_KEY_VALUE_FILE_CODEC_HPP
#define NODO_SERIALIZATION_KEY_VALUE_FILE_CODEC_HPP

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nodo::serialization {

/*
 * KeyValueFileCodec centralizes strict parsing for Nodo's current versioned
 * text files: manifest, finalized blocks and persistent mempool entries.
 */
class KeyValueFileDocument {
public:
    KeyValueFileDocument();

    KeyValueFileDocument(
        std::string version,
        std::map<std::string, std::string> fields
    );

    const std::string& version() const;
    const std::map<std::string, std::string>& fields() const;

    bool hasField(
        const std::string& key
    ) const;

    std::string requireField(
        const std::string& key
    ) const;

    void requireOnlyFields(
        const std::set<std::string>& allowedFields
    ) const;

private:
    std::string m_version;
    std::map<std::string, std::string> m_fields;
};

class KeyValueFileCodec {
public:
    static KeyValueFileDocument parse(
        const std::string& contents,
        const std::string& expectedVersion
    );

    static std::string serialize(
        const std::string& version,
        const std::vector<std::pair<std::string, std::string>>& fields
    );
};

} // namespace nodo::serialization

#endif
