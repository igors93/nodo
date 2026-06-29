#include "node/JsonRpcServer.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

// ---------------------------------------------------------------------------
// Minimal hand-rolled JSON helpers
// (No external JSON library. Supports flat {"key": "value"} objects only.)
// ---------------------------------------------------------------------------

namespace {

// Trim leading/trailing whitespace
std::string trim(const std::string& s) {
    const std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Extract the raw string value for a JSON key from a flat JSON object string.
// Handles both quoted string values and numeric/boolean values.
// Returns empty string if not found.
std::string jsonGetValue(const std::string& json, const std::string& key) {
    const std::string quotedKey = "\"" + key + "\"";
    std::size_t pos = json.find(quotedKey);
    if (pos == std::string::npos) {
        return "";
    }

    // Advance past the key and the colon
    pos += quotedKey.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) {
        ++pos;
    }

    if (pos >= json.size()) {
        return "";
    }

    if (json[pos] == '"') {
        // Quoted string value — find closing quote (no escape handling needed for our use)
        ++pos;
        const std::size_t endPos = json.find('"', pos);
        if (endPos == std::string::npos) {
            return "";
        }
        return json.substr(pos, endPos - pos);
    }

    // Numeric or boolean value — read until delimiter
    const std::size_t endPos = json.find_first_of(",}\t\r\n ", pos);
    if (endPos == std::string::npos) {
        return trim(json.substr(pos));
    }
    return trim(json.substr(pos, endPos - pos));
}

// Escape a string for embedding in a JSON string value.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Build a JSON error object: {"code": N, "message": "..."}
std::string buildErrorObject(int code, const std::string& message) {
    std::ostringstream oss;
    oss << "{\"code\":" << code
        << ",\"message\":\"" << jsonEscape(message) << "\"}";
    return oss.str();
}

bool parseUint64Strict(const std::string& value, std::uint64_t& parsedValue) {
    if (value.empty()) {
        return false;
    }
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    try {
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed =
            std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size() ||
            parsed > std::numeric_limits<std::uint64_t>::max() ||
            std::to_string(parsed) != value) {
            return false;
        }
        parsedValue = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// JsonRpcRequest
// ---------------------------------------------------------------------------

bool JsonRpcRequest::isValid() const {
    return jsonrpc == "2.0" && !method.empty();
}

JsonRpcRequest JsonRpcRequest::parse(const std::string& rawJson) {
    JsonRpcRequest req;

    const std::string trimmed = trim(rawJson);
    if (trimmed.empty() || trimmed.front() != '{') {
        return req;  // will fail isValid()
    }

    req.jsonrpc = jsonGetValue(trimmed, "jsonrpc");
    req.method  = jsonGetValue(trimmed, "method");
    req.id      = jsonGetValue(trimmed, "id");

    // Extract params: find the "params" value which may be an object
    const std::string paramsKey = "\"params\"";
    const std::size_t paramsPos = trimmed.find(paramsKey);
    if (paramsPos != std::string::npos) {
        std::size_t colonPos = trimmed.find(':', paramsPos + paramsKey.size());
        if (colonPos != std::string::npos) {
            // Skip whitespace after colon
            std::size_t valueStart = colonPos + 1;
            while (valueStart < trimmed.size() &&
                   (trimmed[valueStart] == ' ' || trimmed[valueStart] == '\t')) {
                ++valueStart;
            }

            if (valueStart < trimmed.size()) {
                if (trimmed[valueStart] == '{') {
                    // Object params — find matching closing brace
                    int depth = 0;
                    std::size_t end = valueStart;
                    for (; end < trimmed.size(); ++end) {
                        if (trimmed[end] == '{') {
                            ++depth;
                        } else if (trimmed[end] == '}') {
                            --depth;
                            if (depth == 0) {
                                break;
                            }
                        }
                    }
                    req.params = trimmed.substr(valueStart, end - valueStart + 1);
                } else if (trimmed[valueStart] == '[') {
                    // Array params — find matching closing bracket
                    std::size_t end = trimmed.find(']', valueStart);
                    if (end != std::string::npos) {
                        req.params = trimmed.substr(valueStart, end - valueStart + 1);
                    }
                } else {
                    // Primitive param
                    std::size_t end = trimmed.find_first_of(",}", valueStart);
                    if (end == std::string::npos) {
                        end = trimmed.size();
                    }
                    req.params = trim(trimmed.substr(valueStart, end - valueStart));
                }
            }
        }
    }

    if (req.params.empty()) {
        req.params = "{}";
    }

    return req;
}

// ---------------------------------------------------------------------------
// JsonRpcResponse
// ---------------------------------------------------------------------------

bool JsonRpcResponse::isSuccess() const {
    return error.empty() && !result.empty();
}

std::string JsonRpcResponse::serialize() const {
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"" << jsonEscape(jsonrpc) << "\",\"id\":\"" << jsonEscape(id) << "\"";

    if (!error.empty()) {
        oss << ",\"error\":" << error;
    } else {
        oss << ",\"result\":" << result;
    }

    oss << "}";
    return oss.str();
}

JsonRpcResponse JsonRpcResponse::success(
    const std::string& id,
    const std::string& result
) {
    JsonRpcResponse resp;
    resp.id     = id;
    resp.result = result;
    return resp;
}

JsonRpcResponse JsonRpcResponse::makeError(
    const std::string& id,
    int code,
    const std::string& message
) {
    JsonRpcResponse resp;
    resp.id    = id;
    resp.error = buildErrorObject(code, message);
    return resp;
}

// ---------------------------------------------------------------------------
// JsonRpcDispatcher
// ---------------------------------------------------------------------------

JsonRpcDispatcher::JsonRpcDispatcher() = default;

void JsonRpcDispatcher::registerHandler(const std::string& method, Handler handler) {
    m_handlers[method] = std::move(handler);
}

JsonRpcResponse JsonRpcDispatcher::dispatch(const std::string& rawJson) const {
    // Attempt to parse the raw JSON
    const JsonRpcRequest req = JsonRpcRequest::parse(rawJson);

    if (req.jsonrpc.empty() && req.method.empty()) {
        // Completely failed to parse
        return JsonRpcResponse::makeError("", JsonRpcError::PARSE_ERROR, "Parse error");
    }

    if (!req.isValid()) {
        return JsonRpcResponse::makeError(req.id, JsonRpcError::INVALID_REQUEST, "Invalid Request");
    }

    const auto it = m_handlers.find(req.method);
    if (it == m_handlers.end()) {
        return JsonRpcResponse::makeError(
            req.id,
            JsonRpcError::METHOD_NOT_FOUND,
            "Method not found: " + req.method
        );
    }

    try {
        return it->second(req);
    } catch (const std::exception&) {
        return JsonRpcResponse::makeError(
            req.id,
            JsonRpcError::INTERNAL_ERROR,
            "RPC handler failed"
        );
    } catch (...) {
        return JsonRpcResponse::makeError(
            req.id,
            JsonRpcError::INTERNAL_ERROR,
            "RPC handler failed"
        );
    }
}

std::string JsonRpcDispatcher::extractParam(
    const std::string& paramsJson,
    const std::string& key
) {
    return jsonGetValue(paramsJson, key);
}

void JsonRpcDispatcher::registerStandardMethods(
    std::function<std::string(std::uint64_t)>        getBlockByHeight,
    std::function<std::string(const std::string&)>   getBlockByHash,
    std::function<std::string(const std::string&)>   getTransactionById,
    std::function<std::string(const std::string&)>   getAccountState,
    std::function<std::string(const std::string&)>   sendTransaction,
    std::function<std::string()>                     getMempoolStats,
    std::function<std::string(const std::string&)>   estimateFee,
    std::function<std::string()>                     getChainInfo,
    std::function<std::string()>                     getValidators
) {
    registerHandler("nodo_getBlockByHeight",
        [fn = std::move(getBlockByHeight)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string heightStr = extractParam(req.params, "height");
            if (heightStr.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: height"
                );
            }
            std::uint64_t height = 0;
            if (!parseUint64Strict(heightStr, height)) {
                return JsonRpcResponse::makeError(
                    req.id,
                    JsonRpcError::INVALID_PARAMS,
                    "Invalid param: height"
                );
            }
            const std::string result = fn(height);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_getBlockByHash",
        [fn = std::move(getBlockByHash)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string hash = extractParam(req.params, "hash");
            if (hash.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: hash"
                );
            }
            const std::string result = fn(hash);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_getTransactionById",
        [fn = std::move(getTransactionById)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string id = extractParam(req.params, "id");
            if (id.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: id"
                );
            }
            const std::string result = fn(id);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_getAccountState",
        [fn = std::move(getAccountState)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string address = extractParam(req.params, "address");
            if (address.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: address"
                );
            }
            const std::string result = fn(address);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_sendTransaction",
        [fn = std::move(sendTransaction)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string tx = extractParam(req.params, "tx");
            if (tx.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: tx"
                );
            }
            const std::string result = fn(tx);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_getMempoolStats",
        [fn = std::move(getMempoolStats)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string result = fn();
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_estimateFee",
        [fn = std::move(estimateFee)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string urgency = extractParam(req.params, "urgency");
            if (urgency.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: urgency"
                );
            }
            const std::string result = fn(urgency);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_getChainInfo",
        [fn = std::move(getChainInfo)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string result = fn();
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("nodo_getValidators",
        [fn = std::move(getValidators)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string result = fn();
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );
}

void JsonRpcDispatcher::registerGovernanceMethods(
    std::function<std::string()> governanceProposals,
    std::function<std::string(const std::string&)> governanceGetProposal,
    std::function<std::string(const std::string&)> governanceGetVotes,
    std::function<std::string(const std::string&)> governanceGetTally,
    std::function<std::string(const std::string&)> governanceGetDecision,
    std::function<std::string(const std::string&)> governanceGetExecution,
    std::function<std::string(const std::string&)> governanceSubmitProposal,
    std::function<std::string(const std::string&)> governanceSubmitVote,
    std::function<std::string()> governanceStatus
) {
    registerHandler("governance_proposals",
        [fn = std::move(governanceProposals)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string result = fn();
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    auto proposalIdHandler = [](auto fn, const char* missing) {
        return [fn = std::move(fn), missing](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string proposalId = extractParam(req.params, "proposalId");
            if (proposalId.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, missing
                );
            }
            const std::string result = fn(proposalId);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        };
    };

    registerHandler("governance_getProposal",
        proposalIdHandler(std::move(governanceGetProposal), "Missing param: proposalId"));
    registerHandler("governance_getVotes",
        proposalIdHandler(std::move(governanceGetVotes), "Missing param: proposalId"));
    registerHandler("governance_getTally",
        proposalIdHandler(std::move(governanceGetTally), "Missing param: proposalId"));
    registerHandler("governance_getDecision",
        proposalIdHandler(std::move(governanceGetDecision), "Missing param: proposalId"));
    registerHandler("governance_getExecution",
        proposalIdHandler(std::move(governanceGetExecution), "Missing param: proposalId"));

    registerHandler("governance_submitProposal",
        [fn = std::move(governanceSubmitProposal)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string tx = extractParam(req.params, "tx");
            if (tx.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: tx"
                );
            }
            const std::string result = fn(tx);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("governance_submitVote",
        [fn = std::move(governanceSubmitVote)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string tx = extractParam(req.params, "tx");
            if (tx.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: tx"
                );
            }
            const std::string result = fn(tx);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("governance_status",
        [fn = std::move(governanceStatus)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string result = fn();
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );
}

std::vector<std::string> JsonRpcDispatcher::registeredMethods() const {
    std::vector<std::string> methods;
    methods.reserve(m_handlers.size());
    for (const auto& [method, _handler] : m_handlers) {
        methods.push_back(method);
    }
    return methods;
}

} // namespace nodo::node
