#ifndef NODO_SERIALIZATION_FIELD_CODEC_HPP
#define NODO_SERIALIZATION_FIELD_CODEC_HPP

#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * FieldCodec provides shared helpers for deterministic text serialization.
 *
 * Security principle:
 * Parsing rules must not be duplicated across critical ledger modules.
 *
 * Current status:
 * This is still a development text codec. It creates a safer boundary while
 * Nodo evolves toward a stricter binary or canonical encoding format.
 */
class FieldCodec {
public:
    static std::string extractField(
        const std::string& serialized,
        const std::string& key
    );

    static std::string extractBetween(
        const std::string& serialized,
        const std::string& startToken,
        const std::string& endToken
    );

    static std::string extractTrailingSection(
        const std::string& serialized,
        const std::string& startToken,
        const std::string& endToken
    );

    static std::vector<std::string> splitTopLevelObjects(
        const std::string& serializedList,
        const std::string& objectPrefix
    );
};

} // namespace nodo::serialization

#endif