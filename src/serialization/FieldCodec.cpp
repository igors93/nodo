#include "serialization/FieldCodec.hpp"

#include <stdexcept>

namespace nodo::serialization {

std::string FieldCodec::extractField(
    const std::string& serialized,
    const std::string& key
) {
    if (key.empty()) {
        throw std::invalid_argument("Field key cannot be empty.");
    }

    // A key is valid only when preceded by '{' (first field in an object) or
    // ';' (subsequent fields). Matching key= anywhere inside the string would
    // allow a value that contains the text ";otherKey=injected" to be returned
    // for a different field lookup — a key-confusion injection vulnerability.
    std::size_t keyPos = std::string::npos;

    const std::string withSemi  = ";" + key + "=";
    const std::string withBrace = "{" + key + "=";

    const std::size_t semiPos  = serialized.find(withSemi);
    const std::size_t bracePos = serialized.find(withBrace);

    if (semiPos != std::string::npos) {
        keyPos = semiPos + 1; // skip the ';'
    } else if (bracePos != std::string::npos) {
        keyPos = bracePos + 1; // skip the '{'
    }

    if (keyPos == std::string::npos) {
        throw std::invalid_argument("Missing serialized field: " + key);
    }

    const std::size_t valueStart = keyPos + key.size() + 1; // skip "key="
    std::size_t valueEnd = serialized.find(';', valueStart);

    if (valueEnd == std::string::npos) {
        valueEnd = serialized.find('}', valueStart);
    }

    if (valueEnd == std::string::npos || valueEnd < valueStart) {
        throw std::invalid_argument("Invalid serialized field: " + key);
    }

    return serialized.substr(valueStart, valueEnd - valueStart);
}

std::string FieldCodec::extractBetween(
    const std::string& serialized,
    const std::string& startToken,
    const std::string& endToken
) {
    if (startToken.empty() || endToken.empty()) {
        throw std::invalid_argument("Serialization section tokens cannot be empty.");
    }

    const std::size_t start = serialized.find(startToken);

    if (start == std::string::npos) {
        throw std::invalid_argument("Missing serialized section start.");
    }

    const std::size_t valueStart = start + startToken.size();
    const std::size_t end = serialized.find(endToken, valueStart);

    if (end == std::string::npos || end < valueStart) {
        throw std::invalid_argument("Missing serialized section end.");
    }

    return serialized.substr(valueStart, end - valueStart);
}

std::string FieldCodec::extractTrailingSection(
    const std::string& serialized,
    const std::string& startToken,
    const std::string& endToken
) {
    if (startToken.empty() || endToken.empty()) {
        throw std::invalid_argument("Serialization section tokens cannot be empty.");
    }

    const std::size_t start = serialized.find(startToken);

    if (start == std::string::npos) {
        throw std::invalid_argument("Missing trailing serialized section start.");
    }

    const std::size_t valueStart = start + startToken.size();
    const std::size_t valueEnd = serialized.rfind(endToken);

    if (valueEnd == std::string::npos || valueEnd < valueStart) {
        throw std::invalid_argument("Malformed trailing serialized section.");
    }

    return serialized.substr(valueStart, valueEnd - valueStart);
}

std::vector<std::string> FieldCodec::splitTopLevelObjects(
    const std::string& serializedList,
    const std::string& objectPrefix
) {
    if (objectPrefix.empty()) {
        throw std::invalid_argument("Object prefix cannot be empty.");
    }

    std::vector<std::string> objects;

    if (serializedList.empty()) {
        return objects;
    }

    std::size_t index = 0;

    while (index < serializedList.size()) {
        while (index < serializedList.size() &&
               (serializedList[index] == ',' || serializedList[index] == ' ')) {
            ++index;
        }

        if (index >= serializedList.size()) {
            break;
        }

        if (serializedList.compare(index, objectPrefix.size(), objectPrefix) != 0) {
            throw std::invalid_argument("Unexpected object type in serialized list.");
        }

        const std::size_t objectStart = index;
        int braceDepth = 0;
        bool started = false;
        bool completed = false;

        for (; index < serializedList.size(); ++index) {
            const char current = serializedList[index];

            if (current == '{') {
                ++braceDepth;
                started = true;
            } else if (current == '}') {
                --braceDepth;

                if (started && braceDepth == 0) {
                    ++index;
                    objects.push_back(
                        serializedList.substr(objectStart, index - objectStart)
                    );
                    completed = true;
                    break;
                }
            }

            if (braceDepth < 0) {
                throw std::invalid_argument("Malformed serialized object braces.");
            }
        }

        if (!completed) {
            throw std::invalid_argument("Unclosed serialized object.");
        }
    }

    return objects;
}

} // namespace nodo::serialization