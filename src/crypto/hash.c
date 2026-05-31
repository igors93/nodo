#include "crypto/hash.h"

#include <openssl/evp.h>

#include <stdio.h>
#include <string.h>

const char* nodo_hash_algorithm_name(void) {
    return "SHA-256";
}

void nodo_hash_bytes(
    const unsigned char* input,
    unsigned long long input_size,
    char* output,
    unsigned int output_size
) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    EVP_MD_CTX* context = NULL;

    if (output == NULL || output_size < NODO_HASH_BUFFER_SIZE) {
        return;
    }

    output[0] = '\0';

    if (input == NULL && input_size != 0ULL) {
        return;
    }

    context = EVP_MD_CTX_new();
    if (context == NULL) {
        return;
    }

    if (EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(context, input, (size_t)input_size) != 1 ||
        EVP_DigestFinal_ex(context, digest, &digest_size) != 1 ||
        digest_size != 32U) {
        EVP_MD_CTX_free(context);
        output[0] = '\0';
        return;
    }

    EVP_MD_CTX_free(context);

    for (unsigned int i = 0U; i < digest_size; ++i) {
        (void)snprintf(
            output + (i * 2U),
            output_size - (i * 2U),
            "%02x",
            digest[i]
        );
    }

    output[NODO_HASH_HEX_SIZE] = '\0';
}

void nodo_hash_string(
    const char* input,
    char* output,
    unsigned int output_size
) {
    if (input == NULL) {
        nodo_hash_bytes(NULL, 0ULL, output, output_size);
        return;
    }

    nodo_hash_bytes(
        (const unsigned char*)input,
        (unsigned long long)strlen(input),
        output,
        output_size
    );
}
