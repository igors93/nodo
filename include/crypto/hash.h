#ifndef NODO_CRYPTO_HASH_H
#define NODO_CRYPTO_HASH_H

#define NODO_HASH_HEX_SIZE 64
#define NODO_HASH_BUFFER_SIZE 65

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Nodo hash boundary.
 *
 * Current implementation:
 * SHA-256 with 64 lowercase hexadecimal output characters.
 *
 * Security note:
 * This replaces the previous development-only FNV-style placeholder.
 * The boundary remains intentionally small so production deployments can later
 * swap this internal implementation for an audited provider/library without
 * rewriting the blockchain core.
 */
const char* nodo_hash_algorithm_name(void);

void nodo_hash_bytes(
    const unsigned char* input,
    unsigned long long input_size,
    char* output,
    unsigned int output_size
);

void nodo_hash_string(
    const char* input,
    char* output,
    unsigned int output_size
);

#ifdef __cplusplus
}
#endif

#endif
