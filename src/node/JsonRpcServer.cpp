#include "node/JsonRpcServer.hpp"

#include <nlohmann/json.hpp>

#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

using Json = nlohmann::json;

// RPC requests are at most two or three levels deep; anything deeper is
// treated as hostile input rather than parsed.
constexpr int kMaxJsonNestingDepth = 32;

// Parses untrusted JSON, throwing std::exception on malformed input, nesting
// deeper than kMaxJsonNestingDepth, or a key repeated within one object.
Json parseStrictJson(const std::string& text) {
    std::vector<std::set<std::string>> keysPerOpenObject;

    const Json::parser_callback_t callback =
        [&keysPerOpenObject](int depth, Json::parse_event_t event, Json& parsed) {
            if (depth > kMaxJsonNestingDepth) {
                throw std::invalid_argument("JSON nesting is too deep.");
            }
            switch (event) {
                case Json::parse_event_t::object_start:
                    keysPerOpenObject.emplace_back();
                    break;
                case Json::parse_event_t::key:
                    if (!keysPerOpenObject.back()
                             .insert(parsed.get<std::string>())
                             .second) {
                        throw std::invalid_argument("Duplicate JSON object key.");
                    }
                    break;
                case Json::parse_event_t::object_end:
                    keysPerOpenObject.pop_back();
                    break;
                default:
                    break;
            }
            return true;
        };

    return Json::parse(text, callback);
}

// Compact JSON text; invalid UTF-8 is replaced instead of throwing, since
// error messages may echo client-supplied bytes.
std::string dumpJson(const Json& value) {
    return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

// Build a JSON error object: {"code": N, "message": "..."}
std::string buildErrorObject(int code, const std::string& message) {
    return dumpJson(Json{{"code", code}, {"message", message}});
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
    return wellFormedJson && invalidReason.empty() && jsonrpc == "2.0" &&
           !method.empty();
}

JsonRpcRequest JsonRpcRequest::parse(const std::string& rawJson) {
    JsonRpcRequest req;

    Json root;
    try {
        root = parseStrictJson(rawJson);
    } catch (const std::exception&) {
        return req;  // wellFormedJson stays false
    }
    req.wellFormedJson = true;

    if (root.is_array()) {
        req.invalidReason = "Batch requests are not supported.";
        return req;
    }
    if (!root.is_object()) {
        req.invalidReason = "Request must be a JSON object.";
        return req;
    }

    // Read the id first so every later rejection can still be correlated.
    const auto id = root.find("id");
    if (id != root.end()) {
        if (id->is_string()) {
            req.id = id->get<std::string>();
            req.idJson = dumpJson(*id);
        } else if (id->is_number_integer()) {
            req.id = id->dump();
            req.idJson = req.id;
        } else if (!id->is_null()) {
            req.invalidReason = "id must be a string, an integer, or null.";
            return req;
        }
    }

    for (const auto& member : root.items()) {
        if (member.key() != "jsonrpc" && member.key() != "method" &&
            member.key() != "params" && member.key() != "id") {
            req.invalidReason = "Unknown request member '" + member.key() + "'.";
            return req;
        }
    }

    const auto version = root.find("jsonrpc");
    if (version == root.end() || !version->is_string() ||
        version->get<std::string>() != "2.0") {
        req.invalidReason = "jsonrpc must be \"2.0\".";
        return req;
    }
    req.jsonrpc = "2.0";

    const auto method = root.find("method");
    if (method == root.end() || !method->is_string() ||
        method->get<std::string>().empty()) {
        req.invalidReason = "method must be a non-empty string.";
        return req;
    }
    req.method = method->get<std::string>();

    const auto params = root.find("params");
    if (params == root.end()) {
        req.params = "{}";
    } else if (params->is_object() || params->is_array()) {
        req.params = dumpJson(*params);
    } else {
        req.invalidReason = "params must be an object or an array.";
        return req;
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
    // Responses built by dispatch() echo the request id with its JSON type.
    // A response built directly with no id refers to no request: null.
    const std::string idText = !idJson.empty() ? idJson
                               : id.empty()    ? "null"
                                               : dumpJson(Json(id));

    // result and error are JSON documents produced by handlers and
    // buildErrorObject(); they are embedded as-is.
    return "{\"jsonrpc\":" + dumpJson(Json(jsonrpc)) + ",\"id\":" + idText +
           (error.empty() ? ",\"result\":" + result : ",\"error\":" + error) +
           "}";
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
    const JsonRpcRequest req = JsonRpcRequest::parse(rawJson);

    const auto respond = [&req](JsonRpcResponse response) {
        response.idJson = req.idJson;
        return response;
    };

    if (!req.wellFormedJson) {
        return respond(JsonRpcResponse::makeError(
            "", JsonRpcError::PARSE_ERROR, "Parse error"));
    }

    if (!req.isValid()) {
        return respond(JsonRpcResponse::makeError(
            req.id,
            JsonRpcError::INVALID_REQUEST,
            req.invalidReason.empty() ? "Invalid Request"
                                      : "Invalid Request: " + req.invalidReason));
    }

    const auto it = m_handlers.find(req.method);
    if (it == m_handlers.end()) {
        return respond(JsonRpcResponse::makeError(
            req.id,
            JsonRpcError::METHOD_NOT_FOUND,
            "Method not found: " + req.method
        ));
    }

    try {
        return respond(it->second(req));
    } catch (...) {
        return respond(JsonRpcResponse::makeError(
            req.id,
            JsonRpcError::INTERNAL_ERROR,
            "RPC handler failed"
        ));
    }
}

std::string JsonRpcDispatcher::extractParam(
    const std::string& paramsJson,
    const std::string& key
) {
    Json params;
    try {
        params = parseStrictJson(paramsJson);
    } catch (const std::exception&) {
        return "";
    }

    if (!params.is_object()) {
        return "";
    }

    // Only a top-level member counts: a same-named key nested inside another
    // value must never be mistaken for this parameter.
    const auto value = params.find(key);
    if (value == params.end()) {
        return "";
    }
    if (value->is_string()) {
        return value->get<std::string>();
    }
    if (value->is_number() || value->is_boolean()) {
        return value->dump();
    }
    return "";
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

void JsonRpcDispatcher::registerStakingMethods(
    std::function<std::string(const std::string&)> stakeStatus,
    std::function<std::string(const std::string&)> stakePositions,
    std::function<std::string(const std::string&)> stakeGetPosition,
    std::function<std::string(const std::string&)> stakeSubmitSignedTransaction,
    std::function<std::string(const std::string&)> stakePendingUnbonding,
    std::function<std::string(const std::string&)> stakeValidatorStake,
    std::function<std::string()> stakeAuditStatus
) {
    auto validatorHandler = [](auto fn, const char* missing) {
        return [fn = std::move(fn), missing](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string validator = extractParam(req.params, "validator");
            if (validator.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, missing
                );
            }
            const std::string result = fn(validator);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        };
    };

    registerHandler(
        "stake_status",
        validatorHandler(std::move(stakeStatus), "Missing param: validator")
    );

    registerHandler("stake_positions",
        [fn = std::move(stakePositions)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string address = extractParam(req.params, "address");
            const std::string result = fn(address);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    registerHandler("stake_getPosition",
        [fn = std::move(stakeGetPosition)](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string positionId = extractParam(req.params, "positionId");
            if (positionId.empty()) {
                return JsonRpcResponse::makeError(
                    req.id, JsonRpcError::INVALID_PARAMS, "Missing param: positionId"
                );
            }
            const std::string result = fn(positionId);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        }
    );

    auto signedStakeTxHandler = [](auto fn, const char* methodName) {
        return [fn = std::move(fn), methodName](const JsonRpcRequest& req) -> JsonRpcResponse {
            const std::string transaction = extractParam(req.params, "transaction");
            if (transaction.empty()) {
                return JsonRpcResponse::makeError(
                    req.id,
                    JsonRpcError::INVALID_PARAMS,
                    std::string(methodName) + " requires a serialized signed transaction"
                );
            }
            const std::string result = fn(transaction);
            return JsonRpcResponse::success(req.id, result.empty() ? "null" : result);
        };
    };

    registerHandler("stake_deposit",
        signedStakeTxHandler(stakeSubmitSignedTransaction, "stake_deposit"));
    registerHandler("stake_topUp",
        signedStakeTxHandler(stakeSubmitSignedTransaction, "stake_topUp"));
    registerHandler("stake_unlock",
        signedStakeTxHandler(stakeSubmitSignedTransaction, "stake_unlock"));
    registerHandler("stake_withdraw",
        signedStakeTxHandler(std::move(stakeSubmitSignedTransaction), "stake_withdraw"));

    registerHandler(
        "stake_pendingUnbonding",
        validatorHandler(std::move(stakePendingUnbonding), "Missing param: validator")
    );
    registerHandler(
        "stake_validatorStake",
        validatorHandler(std::move(stakeValidatorStake), "Missing param: validator")
    );
    registerHandler("stake_auditStatus",
        [fn = std::move(stakeAuditStatus)](const JsonRpcRequest& req) -> JsonRpcResponse {
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
