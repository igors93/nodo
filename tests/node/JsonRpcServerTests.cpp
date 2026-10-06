#include "node/JsonRpcServer.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::node::JsonRpcDispatcher;
using nodo::node::JsonRpcError;
using nodo::node::JsonRpcRequest;
using nodo::node::JsonRpcResponse;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDispatchMethodNotFound() {
    JsonRpcDispatcher dispatcher;

    const auto response = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"nodo_nonexistent","params":{},"id":"1"})"
    );

    requireCondition(
        !response.isSuccess(),
        "dispatch should return error response for unknown method."
    );

    requireCondition(
        response.error.find("-32601") != std::string::npos,
        "Error response should contain METHOD_NOT_FOUND code -32601."
    );
}

void testDispatchParseErrorForMalformedJson() {
    JsonRpcDispatcher dispatcher;

    // Not valid JSON
    const auto response = dispatcher.dispatch("this is not json at all!!!");

    requireCondition(
        !response.isSuccess(),
        "dispatch should return error for malformed JSON."
    );

    requireCondition(
        response.error.find("-32700") != std::string::npos ||
        response.error.find("-32600") != std::string::npos,
        "Error response should contain PARSE_ERROR or INVALID_REQUEST code."
    );
}

void testRegisteredHandlerIsCalled() {
    JsonRpcDispatcher dispatcher;

    bool handlerCalled = false;

    dispatcher.registerHandler(
        "test_ping",
        [&handlerCalled](const JsonRpcRequest& req) -> JsonRpcResponse {
            handlerCalled = true;
            return JsonRpcResponse::success(req.id, "\"pong\"");
        }
    );

    const auto response = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping","params":{},"id":"42"})"
    );

    requireCondition(
        handlerCalled,
        "Registered handler should be called when method is dispatched."
    );

    requireCondition(
        response.isSuccess(),
        "Response should indicate success when handler returns success."
    );

    requireCondition(
        response.result == "\"pong\"",
        "Response result should match what the handler returned."
    );
}

void testSuccessResponseHasCorrectFields() {
    const auto response = JsonRpcResponse::success("req-id-99", "{\"height\":42}");

    requireCondition(
        response.jsonrpc == "2.0",
        "Response jsonrpc field should be 2.0."
    );

    requireCondition(
        response.id == "req-id-99",
        "Response id should match the request id."
    );

    requireCondition(
        response.isSuccess(),
        "Response should be marked as success."
    );

    const std::string serialized = response.serialize();
    requireCondition(
        serialized.find("\"jsonrpc\"") != std::string::npos,
        "Serialized response should contain jsonrpc field."
    );

    requireCondition(
        serialized.find("\"result\"") != std::string::npos,
        "Serialized success response should contain result field."
    );
}

void testErrorResponseHasCorrectCode() {
    const auto response = JsonRpcResponse::makeError(
        "req-id-7",
        JsonRpcError::INTERNAL_ERROR,
        "Something went wrong"
    );

    requireCondition(
        !response.isSuccess(),
        "Error response should not be marked as success."
    );

    requireCondition(
        response.error.find("-32603") != std::string::npos,
        "Error response should contain INTERNAL_ERROR code -32603."
    );

    requireCondition(
        response.id == "req-id-7",
        "Error response id should match the request id."
    );

    const std::string serialized = response.serialize();
    requireCondition(
        serialized.find("\"error\"") != std::string::npos,
        "Serialized error response should contain error field."
    );
}

void testGovernanceMethodsAreRegisteredAndDispatch() {
    JsonRpcDispatcher dispatcher;

    dispatcher.registerGovernanceMethods(
        []() { return R"({"proposals":["p1"]})"; },
        [](const std::string& id) { return "{\"proposalId\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"votes\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"tally\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"decision\":\"" + id + "\"}"; },
        [](const std::string& id) { return "{\"execution\":\"" + id + "\"}"; },
        [](const std::string& tx) { return "{\"proposalTx\":\"" + tx + "\"}"; },
        [](const std::string& tx) { return "{\"voteTx\":\"" + tx + "\"}"; },
        []() { return R"({"activeProposalCount":1})"; }
    );

    const auto proposal = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"governance_getProposal","params":{"proposalId":"p1"},"id":"7"})"
    );

    requireCondition(
        proposal.isSuccess() &&
        proposal.result.find("\"proposalId\":\"p1\"") != std::string::npos,
        "governance_getProposal should dispatch to the registered callback."
    );

    const auto missing = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"governance_getTally","params":{},"id":"8"})"
    );

    requireCondition(
        !missing.isSuccess() &&
        missing.error.find("-32602") != std::string::npos,
        "Governance proposal-id methods should reject missing proposalId."
    );

    const auto submitVote = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"governance_submitVote","params":{"tx":"signed-vote"},"id":"9"})"
    );

    requireCondition(
        submitVote.isSuccess() &&
        submitVote.result.find("\"voteTx\":\"signed-vote\"") != std::string::npos,
        "governance_submitVote should dispatch signed transaction payloads."
    );
}


void testEscapedJsonStringParameterIsDecoded() {
    const std::string raw =
        R"({"jsonrpc":"2.0","method":"nodo_sendTransaction","params":{"tx":"line1\nline2\"quoted\""},"id":"11"})";

    const JsonRpcRequest req = JsonRpcRequest::parse(raw);
    const std::string tx = JsonRpcDispatcher::extractParam(req.params, "tx");

    requireCondition(
        tx == "line1\nline2\"quoted\"",
        "JSON-RPC string parameters should decode common JSON escapes."
    );
}

void testStandardMethodsRegisterOfficialChainCalls() {
    JsonRpcDispatcher dispatcher;

    dispatcher.registerStandardMethods(
        [](std::uint64_t height) { return "{\"height\":" + std::to_string(height) + "}"; },
        [](const std::string& hash) { return "{\"hash\":\"" + hash + "\"}"; },
        [](const std::string& id) { return "{\"tx\":\"" + id + "\"}"; },
        [](const std::string& address) { return "{\"address\":\"" + address + "\"}"; },
        [](const std::string& tx) { return "{\"submitted\":\"" + tx + "\"}"; },
        []() { return R"({"size":0})"; },
        [](const std::string& urgency) { return "{\"urgency\":\"" + urgency + "\"}"; },
        []() { return R"({"chainId":"localnet"})"; },
        []() { return R"({"validators":[]})"; }
    );

    const auto chainInfo = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"nodo_getChainInfo","params":{},"id":"12"})"
    );

    requireCondition(
        chainInfo.isSuccess() && chainInfo.result.find("localnet") != std::string::npos,
        "nodo_getChainInfo should be part of the standard JSON-RPC method set."
    );

    const auto block = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"nodo_getBlockByHeight","params":{"height":7},"id":"13"})"
    );

    requireCondition(
        block.isSuccess() && block.result.find("\"height\":7") != std::string::npos,
        "nodo_getBlockByHeight should parse numeric height and dispatch."
    );
}

JsonRpcDispatcher pingDispatcher(std::string& calledMethod) {
    JsonRpcDispatcher dispatcher;
    for (const char* method : {"test_ping", "evil"}) {
        dispatcher.registerHandler(
            method,
            [&calledMethod](const JsonRpcRequest& req) -> JsonRpcResponse {
                calledMethod = req.method;
                return JsonRpcResponse::success(req.id, "\"pong\"");
            }
        );
    }
    return dispatcher;
}

bool hasCode(const JsonRpcResponse& response, int code) {
    return !response.isSuccess() &&
           response.error.find(std::to_string(code)) != std::string::npos;
}

// The old parser took the first "method" substring anywhere in the body, so a
// key nested in params could choose which handler ran.
void testNestedKeysCannotImpersonateRequestMembers() {
    std::string calledMethod;
    const JsonRpcDispatcher dispatcher = pingDispatcher(calledMethod);

    const auto response = dispatcher.dispatch(
        R"({"params":{"method":"evil","jsonrpc":"2.0"},"jsonrpc":"2.0","method":"test_ping","id":1})"
    );
    requireCondition(
        response.isSuccess() && calledMethod == "test_ping",
        "A nested params key must not override the top-level method."
    );

    requireCondition(
        JsonRpcDispatcher::extractParam(R"({"wrapper":{"tx":"inner"}})", "tx").empty() &&
        JsonRpcDispatcher::extractParam(R"({"note":"\"tx\":\"fake\"","tx":"real"})", "tx") == "real",
        "extractParam must read only the top-level member, never text inside other values."
    );
}

// Duplicate keys let a proxy and the node read different values from one body.
void testDuplicateKeysAreRejected() {
    std::string calledMethod;
    const JsonRpcDispatcher dispatcher = pingDispatcher(calledMethod);

    const auto topLevel = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping","method":"evil","id":1})"
    );
    const auto nested = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping","params":{"tx":"a","tx":"b"},"id":1})"
    );
    requireCondition(
        hasCode(topLevel, JsonRpcError::PARSE_ERROR) &&
        hasCode(nested, JsonRpcError::PARSE_ERROR) && calledMethod.empty(),
        "Duplicate keys at any depth must be rejected before dispatch."
    );
}

void testMalformedJsonIsRejected() {
    std::string calledMethod;
    const JsonRpcDispatcher dispatcher = pingDispatcher(calledMethod);

    for (const std::string& body : {
             std::string(R"({"jsonrpc":"2.0","method":"test_ping","id":1} trailing)"),
             std::string(R"({"jsonrpc":"2.0","method":"test_ping","id":1)"),
             std::string(R"({'jsonrpc':'2.0','method':'test_ping'})"),
             std::string("{\"jsonrpc\":\"2.0\",\"method\":\"test_\xff\",\"id\":1}"),
             std::string(),
         }) {
        const auto response = dispatcher.dispatch(body);
        requireCondition(
            hasCode(response, JsonRpcError::PARSE_ERROR) &&
            response.serialize().find("\"id\":null") != std::string::npos,
            "Malformed JSON must yield PARSE_ERROR with a null id: " + body
        );
    }
    requireCondition(calledMethod.empty(), "No handler may run for malformed JSON.");
}

void testExcessiveNestingIsRejected() {
    std::string calledMethod;
    const JsonRpcDispatcher dispatcher = pingDispatcher(calledMethod);

    const std::string deep = std::string(40, '[') + std::string(40, ']');
    const auto response = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping","params":)" + deep + R"(,"id":1})"
    );
    requireCondition(
        hasCode(response, JsonRpcError::PARSE_ERROR) && calledMethod.empty(),
        "JSON nested beyond the limit must be rejected."
    );
}

void testStructurallyInvalidRequestsAreRejected() {
    std::string calledMethod;
    const JsonRpcDispatcher dispatcher = pingDispatcher(calledMethod);

    const std::vector<std::pair<std::string, std::string>> cases{
        {R"([{"jsonrpc":"2.0","method":"test_ping","id":1}])", "Batch"},
        {R"("just a string")", "JSON object"},
        {R"({"jsonrpc":"1.0","method":"test_ping","id":1})", "jsonrpc"},
        {R"({"method":"test_ping","id":1})", "jsonrpc"},
        {R"({"jsonrpc":"2.0","method":"","id":1})", "method"},
        {R"({"jsonrpc":"2.0","method":7,"id":1})", "method"},
        {R"({"jsonrpc":"2.0","method":"test_ping","params":"x","id":1})", "params"},
        {R"({"jsonrpc":"2.0","method":"test_ping","id":1,"extra":true})", "extra"},
        {R"({"jsonrpc":"2.0","method":"test_ping","id":1.5})", "id"},
        {R"({"jsonrpc":"2.0","method":"test_ping","id":{"a":1}})", "id"},
    };

    for (const auto& [body, reasonFragment] : cases) {
        const auto response = dispatcher.dispatch(body);
        requireCondition(
            hasCode(response, JsonRpcError::INVALID_REQUEST) &&
            response.error.find(reasonFragment) != std::string::npos,
            "Expected INVALID_REQUEST mentioning '" + reasonFragment + "' for: " + body
        );
    }
    requireCondition(calledMethod.empty(), "No handler may run for invalid requests.");
}

// JSON-RPC 2.0 requires the response id to equal the request id, type included.
void testResponseEchoesRequestIdWithItsType() {
    std::string calledMethod;
    const JsonRpcDispatcher dispatcher = pingDispatcher(calledMethod);

    const auto numeric = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping","id":7})"
    ).serialize();
    const auto text = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping","id":"7"})"
    ).serialize();
    const auto absent = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"test_ping"})"
    ).serialize();
    const auto unknown = dispatcher.dispatch(
        R"({"jsonrpc":"2.0","method":"missing","id":"q\"1"})"
    ).serialize();

    requireCondition(
        numeric.find("\"id\":7,") != std::string::npos &&
        text.find("\"id\":\"7\",") != std::string::npos &&
        absent.find("\"id\":null,") != std::string::npos &&
        unknown.find(R"("id":"q\"1")") != std::string::npos,
        "Responses must echo the request id with its JSON type: " +
            numeric + " | " + text + " | " + absent + " | " + unknown
    );
}

void testUnicodeEscapesAreDecoded() {
    const JsonRpcRequest req = JsonRpcRequest::parse(
        R"({"jsonrpc":"2.0","method":"nodo_sendTransaction","params":{"tx":"café"},"id":1})"
    );
    requireCondition(
        req.isValid() &&
        JsonRpcDispatcher::extractParam(req.params, "tx") == "caf\xc3\xa9",
        "\\u escapes must decode to UTF-8."
    );
}

void testErrorMessagesAreAlwaysValidJson() {
    const std::string serialized = JsonRpcResponse::makeError(
        "", JsonRpcError::INTERNAL_ERROR, std::string("bad\x01\xff") + "\"end"
    ).serialize();
    requireCondition(
        serialized.find("\\u0001") != std::string::npos &&
        serialized.find("\x01") == std::string::npos &&
        serialized.find("\xff") == std::string::npos &&
        serialized.find("\\\"end") != std::string::npos,
        "Error messages must escape control characters and replace invalid UTF-8: " +
            serialized
    );
}

void testHeightParameterValidation() {
    JsonRpcDispatcher dispatcher;
    std::uint64_t seenHeight = 0;
    dispatcher.registerStandardMethods(
        [&seenHeight](std::uint64_t height) {
            seenHeight = height;
            return "{\"height\":" + std::to_string(height) + "}";
        },
        [](const std::string&) { return std::string("{}"); },
        [](const std::string&) { return std::string("{}"); },
        [](const std::string&) { return std::string("{}"); },
        [](const std::string&) { return std::string("{}"); },
        []() { return std::string("{}"); },
        [](const std::string&) { return std::string("{}"); },
        []() { return std::string("{}"); },
        []() { return std::string("{}"); }
    );

    const auto call = [&dispatcher](const std::string& height) {
        return dispatcher.dispatch(
            R"({"jsonrpc":"2.0","method":"nodo_getBlockByHeight","params":{"height":)" +
            height + R"(},"id":1})"
        );
    };

    requireCondition(
        call("18446744073709551615").isSuccess() &&
            seenHeight == 18446744073709551615ULL &&
            call("\"9\"").isSuccess() && seenHeight == 9,
        "Unsigned 64-bit heights, numeric or quoted, must be accepted."
    );

    for (const std::string& bad : {"18446744073709551616", "-1", "7.5", "true",
                                   "\"7a\"", "null", "{\"v\":1}"}) {
        const auto response = call(bad);
        requireCondition(
            hasCode(response, JsonRpcError::INVALID_PARAMS),
            "Height " + bad + " must be rejected as INVALID_PARAMS."
        );
    }
}

} // namespace

int main() {
    try {
        testDispatchMethodNotFound();
        testDispatchParseErrorForMalformedJson();
        testRegisteredHandlerIsCalled();
        testSuccessResponseHasCorrectFields();
        testErrorResponseHasCorrectCode();
        testGovernanceMethodsAreRegisteredAndDispatch();
        testEscapedJsonStringParameterIsDecoded();
        testStandardMethodsRegisterOfficialChainCalls();
        testNestedKeysCannotImpersonateRequestMembers();
        testDuplicateKeysAreRejected();
        testMalformedJsonIsRejected();
        testExcessiveNestingIsRejected();
        testStructurallyInvalidRequestsAreRejected();
        testResponseEchoesRequestIdWithItsType();
        testUnicodeEscapesAreDecoded();
        testErrorMessagesAreAlwaysValidJson();
        testHeightParameterValidation();

        std::cout << "Nodo JsonRpcServer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo JsonRpcServer tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
