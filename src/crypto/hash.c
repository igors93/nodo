#include "crypto/hash.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NODO_SHA256_BLOCK_SIZE 64
#define NODO_SHA256_DIGEST_SIZE 32

typedef struct {
    uint8_t data[NODO_SHA256_BLOCK_SIZE];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} nodo_sha256_ctx;

static const uint32_t k_sha256[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

static uint32_t rotr32(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

static uint32_t choose32(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static uint32_t majority32(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t sigma0_upper(uint32_t x) {
    return rotr32(x, 2U) ^ rotr32(x, 13U) ^ rotr32(x, 22U);
}

static uint32_t sigma1_upper(uint32_t x) {
    return rotr32(x, 6U) ^ rotr32(x, 11U) ^ rotr32(x, 25U);
}

static uint32_t sigma0_lower(uint32_t x) {
    return rotr32(x, 7U) ^ rotr32(x, 18U) ^ (x >> 3U);
}

static uint32_t sigma1_lower(uint32_t x) {
    return rotr32(x, 17U) ^ rotr32(x, 19U) ^ (x >> 10U);
}

static void sha256_transform(
    nodo_sha256_ctx* ctx,
    const uint8_t data[NODO_SHA256_BLOCK_SIZE]
) {
    uint32_t message_schedule[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;

    for (uint32_t i = 0; i < 16U; ++i) {
        const uint32_t j = i * 4U;
        message_schedule[i] =
            ((uint32_t)data[j] << 24U) |
            ((uint32_t)data[j + 1U] << 16U) |
            ((uint32_t)data[j + 2U] << 8U) |
            ((uint32_t)data[j + 3U]);
    }

    for (uint32_t i = 16U; i < 64U; ++i) {
        message_schedule[i] =
            sigma1_lower(message_schedule[i - 2U]) +
            message_schedule[i - 7U] +
            sigma0_lower(message_schedule[i - 15U]) +
            message_schedule[i - 16U];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (uint32_t i = 0; i < 64U; ++i) {
        const uint32_t t1 =
            h +
            sigma1_upper(e) +
            choose32(e, f, g) +
            k_sha256[i] +
            message_schedule[i];

        const uint32_t t2 =
            sigma0_upper(a) +
            majority32(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(nodo_sha256_ctx* ctx) {
    ctx->datalen = 0U;
    ctx->bitlen = 0ULL;
    ctx->state[0] = 0x6a09e667UL;
    ctx->state[1] = 0xbb67ae85UL;
    ctx->state[2] = 0x3c6ef372UL;
    ctx->state[3] = 0xa54ff53aUL;
    ctx->state[4] = 0x510e527fUL;
    ctx->state[5] = 0x9b05688cUL;
    ctx->state[6] = 0x1f83d9abUL;
    ctx->state[7] = 0x5be0cd19UL;
}

static void sha256_update(
    nodo_sha256_ctx* ctx,
    const uint8_t* data,
    unsigned long long len
) {
    for (unsigned long long i = 0ULL; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;

        if (ctx->datalen == NODO_SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512ULL;
            ctx->datalen = 0U;
        }
    }
}

static void sha256_final(
    nodo_sha256_ctx* ctx,
    uint8_t hash[NODO_SHA256_DIGEST_SIZE]
) {
    uint32_t i = ctx->datalen;

    if (ctx->datalen < 56U) {
        ctx->data[i++] = 0x80U;

        while (i < 56U) {
            ctx->data[i++] = 0x00U;
        }
    } else {
        ctx->data[i++] = 0x80U;

        while (i < 64U) {
            ctx->data[i++] = 0x00U;
        }

        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56U);
    }

    ctx->bitlen += (uint64_t)ctx->datalen * 8ULL;

    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8U);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16U);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24U);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32U);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40U);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48U);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56U);

    sha256_transform(ctx, ctx->data);

    for (i = 0U; i < 4U; ++i) {
        hash[i]      = (uint8_t)((ctx->state[0] >> (24U - i * 8U)) & 0x000000ffUL);
        hash[i + 4U]  = (uint8_t)((ctx->state[1] >> (24U - i * 8U)) & 0x000000ffUL);
        hash[i + 8U]  = (uint8_t)((ctx->state[2] >> (24U - i * 8U)) & 0x000000ffUL);
        hash[i + 12U] = (uint8_t)((ctx->state[3] >> (24U - i * 8U)) & 0x000000ffUL);
        hash[i + 16U] = (uint8_t)((ctx->state[4] >> (24U - i * 8U)) & 0x000000ffUL);
        hash[i + 20U] = (uint8_t)((ctx->state[5] >> (24U - i * 8U)) & 0x000000ffUL);
        hash[i + 24U] = (uint8_t)((ctx->state[6] >> (24U - i * 8U)) & 0x000000ffUL);
        hash[i + 28U] = (uint8_t)((ctx->state[7] >> (24U - i * 8U)) & 0x000000ffUL);
    }
}

const char* nodo_hash_algorithm_name(void) {
    return "SHA-256";
}

void nodo_hash_bytes(
    const unsigned char* input,
    unsigned long long input_size,
    char* output,
    unsigned int output_size
) {
    uint8_t digest[NODO_SHA256_DIGEST_SIZE];
    nodo_sha256_ctx ctx;

    if (output == NULL || output_size < NODO_HASH_BUFFER_SIZE) {
        return;
    }

    if (input == NULL && input_size != 0ULL) {
        output[0] = '\0';
        return;
    }

    sha256_init(&ctx);

    if (input_size > 0ULL) {
        sha256_update(&ctx, input, input_size);
    }

    sha256_final(&ctx, digest);

    for (unsigned int i = 0U; i < NODO_SHA256_DIGEST_SIZE; ++i) {
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
    const char* safe_input = input != NULL ? input : "";

    nodo_hash_bytes(
        (const unsigned char*)safe_input,
        (unsigned long long)strlen(safe_input),
        output,
        output_size
    );
}
