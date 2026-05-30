#include "serialization/KeyValueFileCodec.hpp"

#include <sstream>
#include <stdexcept>

namespace nodo::serialization {

namespace {

bool isSafeKey(
    const std::string& key
) {
    if (key.empty()) {
        return false;
    }

    for (const char current : key) {
        const bool allowed =
            (current >= 'a' && current <= 'z') ||
            (current >= 'A' && current <= 'Z') ||
            (current >= '0' && current <= '9') ||
            current == '_' ||
            current == '-' ||
            current == '.';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

void validateValue(
    const std::string& key,
    const std::string& value
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty value for key-value field: " + key);
    }

    if (value.find('\n') != std::string::npos ||
        value.find('\r') != std::string::npos) {
        throw std::invalid_argument("Multiline value rejected for key-value field: " + key);
    }
}

} // namespace

KeyValueFileDocument::KeyValueFileDocument()
    : m_version(""),
      m_fields() {}

KeyValueFileDocument::KeyValueFileDocument(
    std::string version,
    std::map<std::string, std::string> fields
)
    : m_version(std::move(version)),
      m_fields(std::move(fields)) {}

const std::string& KeyValueFileDocument::version() const {
    return m_version;
}

const std::map<std::string, std::string>& KeyValueFileDocument::fields() const {
    return m_fields;
}

bool KeyValueFileDocument::hasField(
    const std::string& key
) const {
    return m_fields.find(key) != m_fields.end();
}

std::string KeyValueFileDocument::requireField(
    const std::string& key
) const {
    const auto found =
        m_fields.find(key);

    if (found == m_fields.end()) {
        throw std::invalid_argument("Missing key-value field: " + key);
    }

    return found->second;
}

void KeyValueFileDocument::requireOnlyFields(
    const std::set<std::string>& allowedFields
) const {
    for (const auto& [key, ignored] : m_fields) {
        (void)ignored;

        if (allowedFields.find(key) == allowedFields.end()) {
            throw std::invalid_argument("Unknown key-value field: " + key);
        }
    }
}

KeyValueFileDocument KeyValueFileCodec::parse(
    const std::string& contents,
    const std::string& expectedVersion
) {
    if (expectedVersion.empty()) {
        throw std::invalid_argument("Expected key-value file version cannot be empty.");
    }

    std::istringstream input(contents);
    std::string line;
    std::map<std::string, std::string> fields;
    bool sawVersion = false;
    std::size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;

        if (!line.empty() &&
            line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            throw std::invalid_argument("Blank line in key-value file.");
        }

        if (!sawVersion) {
            if (line != expectedVersion) {
                throw std::invalid_argument("Unsupported key-value file version: " + line);
            }

            sawVersion = true;
            continue;
        }

        const std::size_t separator =
            line.find('=');

        if (separator == std::string::npos ||
            separator == 0 ||
            separator + 1 >= line.size()) {
            throw std::invalid_argument("Malformed key-value line: " + std::to_string(lineNumber));
        }

        const std::string key =
            line.substr(0, separator);

        const std::string value =
            line.substr(separator + 1);

        if (!isSafeKey(key)) {
            throw std::invalid_argument("Unsafe key-value field name: " + key);
        }

        validateValue(key, value);

        if (!fields.emplace(key, value).second) {
            throw std::invalid_argument("Duplicate key-value field: " + key);
        }
    }

    if (!sawVersion) {
        throw std::invalid_argument("Missing key-value file version.");
    }

    return KeyValueFileDocument(
        expectedVersion,
        std::move(fields)
    );
}

std::string KeyValueFileCodec::serialize(
    const std::string& version,
    const std::vector<std::pair<std::string, std::string>>& fields
) {
    if (version.empty() ||
        version.find('\n') != std::string::npos ||
        version.find('\r') != std::string::npos) {
        throw std::invalid_argument("Invalid key-value file version.");
    }

    std::set<std::string> seen;
    std::ostringstream output;

    output << version << "\n";

    for (const auto& [key, value] : fields) {
        if (!isSafeKey(key)) {
            throw std::invalid_argument("Unsafe key-value field name: " + key);
        }

        validateValue(key, value);

        if (!seen.insert(key).second) {
            throw std::invalid_argument("Duplicate key-value field: " + key);
        }

        output << key << "=" << value << "\n";
    }

    return output.str();
}

} // namespace nodo::serialization
