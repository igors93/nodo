#include "crypto/hash.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

std::string hashString(
    const std::string& input
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};

    nodo_hash_string(
        input.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output);
}

void testAlgorithmName() {
    requireCondition(
        std::string(nodo_hash_algorithm_name()) == "SHA-256",
        "Nodo hash provider did not report SHA-256."
    );
}

void testKnownSha256Vectors() {
    requireCondition(
        hashString("") ==
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
        "SHA-256 vector failed for empty string."
    );

    requireCondition(
        hashString("abc") ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
        "SHA-256 vector failed for abc."
    );

    requireCondition(
        hashString("hello") ==
            "2cf24dba5fb0a30e26e83b2ac5b9e29e"
            "1b161e5c1fa7425e73043362938b9824",
        "SHA-256 vector failed for hello."
    );

    requireCondition(
        hashString("The quick brown fox jumps over the lazy dog") ==
            "d7a8fbb307d7809469ca9abcb0082e4f"
            "8d5651e46d3cdb762d02d0bf37c9e592",
        "SHA-256 vector failed for quick brown fox."
    );
}

void testHashShapeAndDeterminism() {
    const std::string first =
        hashString("nodo-deterministic-input");

    const std::string second =
        hashString("nodo-deterministic-input");

    const std::string different =
        hashString("nodo-deterministic-input!");

    requireCondition(
        first.size() == NODO_HASH_HEX_SIZE,
        "SHA-256 output length is not 64 hex characters."
    );

    requireCondition(
        first == second,
        "SHA-256 output is not deterministic."
    );

    requireCondition(
        first != different,
        "Different inputs unexpectedly produced the same hash."
    );

    for (const char current : first) {
        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';

        requireCondition(
            isDigit || isLowerHex,
            "SHA-256 output is not lowercase hexadecimal."
        );
    }
}

void testBytesApiMatchesStringApi() {
    const std::string input = "nodo-byte-api-check";
    char bytesOutput[NODO_HASH_BUFFER_SIZE] = {0};

    nodo_hash_bytes(
        reinterpret_cast<const unsigned char*>(input.data()),
        static_cast<unsigned long long>(input.size()),
        bytesOutput,
        sizeof(bytesOutput)
    );

    requireCondition(
        std::string(bytesOutput) == hashString(input),
        "nodo_hash_bytes does not match nodo_hash_string."
    );
}

} // namespace

int main() {
    try {
        testAlgorithmName();
        testKnownSha256Vectors();
        testHashShapeAndDeterminism();
        testBytesApiMatchesStringApi();

        std::cout << "Nodo crypto hash tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo crypto hash tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
