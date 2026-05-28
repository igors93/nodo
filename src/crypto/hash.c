#include "crypto/hash.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * FNV-1a 64-bit educativo.
 *
 * NÃO USAR EM PRODUÇÃO.
 * Futuramente substituir por SHA-256 real.
 */
static uint64_t fnv1a64(const char* input, uint64_t seed) {
    uint64_t hash = 14695981039346656037ULL ^ seed;

    while (*input) {
        hash ^= (unsigned char)(*input);
        hash *= 1099511628211ULL;
        input++;
    }

    return hash;
}

void nodo_hash_string(const char* input, char* output, unsigned int output_size) {
    if (input == NULL || output == NULL || output_size < 65) {
        return;
    }

    /*
     * Produzimos 64 caracteres hexadecimais para parecer SHA-256,
     * mas ainda é apenas educativo.
     */
    uint64_t h1 = fnv1a64(input, 0xA1);
    uint64_t h2 = fnv1a64(input, 0xB2);
    uint64_t h3 = fnv1a64(input, 0xC3);
    uint64_t h4 = fnv1a64(input, 0xD4);

    snprintf(
        output,
        output_size,
        "%016llx%016llx%016llx%016llx",
        (unsigned long long)h1,
        (unsigned long long)h2,
        (unsigned long long)h3,
        (unsigned long long)h4
    );
}