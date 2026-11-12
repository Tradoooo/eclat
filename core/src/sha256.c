// SPDX-License-Identifier: Apache-2.0
/* Éclat — SHA-256 (FIPS 180-4), C11 pur. Voir cap_sha256.h. */
#include "eclat/cap_sha256.h"

#include <string.h>

/* Constantes K de FIPS 180-4 §4.2.2 (32 premiers bits de la partie fractionnaire des
 * racines cubiques des 64 premiers nombres premiers). */
static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROR(x, 2) ^ ROR(x, 13) ^ ROR(x, 22))
#define BSIG1(x) (ROR(x, 6) ^ ROR(x, 11) ^ ROR(x, 25))
#define SSIG0(x) (ROR(x, 7) ^ ROR(x, 18) ^ ((x) >> 3))
#define SSIG1(x) (ROR(x, 17) ^ ROR(x, 19) ^ ((x) >> 10))

/* Lit 4 octets gros-boutistes : indépendant de l'endiannisme de la machine hôte. */
static uint32_t lire_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void ecrire_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* Expansion du planning de message : 16 mots lus, 48 dérivés. */
static void expandir(const uint8_t block[64], uint32_t w[64]) {
    for (int i = 0; i < 16; i++) {
        w[i] = lire_be32(block + (size_t)i * 4);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SSIG1(w[i - 2]) + w[i - 7] + SSIG0(w[i - 15]) + w[i - 16];
    }
}

/* 64 tours de compression sur un bloc. */
static void compresser(uint32_t h[8], const uint8_t block[64]) {
    uint32_t w[64];
    expandir(block, w);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], i = h[7];
    for (int t = 0; t < 64; t++) {
        uint32_t t1 = i + BSIG1(e) + CH(e, f, g) + K[t] + w[t];
        uint32_t t2 = BSIG0(a) + MAJ(a, b, c);
        i = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += i;
}

void eclat_sha256_init(eclat_sha256_ctx *ctx) {
    if (ctx == NULL) {
        return;
    }
    static const uint32_t H0[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    memcpy(ctx->h, H0, sizeof(H0));
    memset(ctx->block, 0, sizeof(ctx->block));
    ctx->bits = 0;
    ctx->buffered = 0;
}

void eclat_sha256_update(eclat_sha256_ctx *ctx, const void *data, size_t len) {
    if (ctx == NULL || (data == NULL && len != 0)) {
        return;
    }
    const uint8_t *p = (const uint8_t *)data;
    ctx->bits += (uint64_t)len * 8u;
    while (len > 0) {
        size_t place = sizeof(ctx->block) - ctx->buffered;
        size_t n = (place < len) ? place : len;
        memcpy(ctx->block + ctx->buffered, p, n);
        ctx->buffered += n;
        p += n;
        len -= n;
        if (ctx->buffered == sizeof(ctx->block)) {
            compresser(ctx->h, ctx->block);
            ctx->buffered = 0;
        }
    }
}

void eclat_sha256_final(eclat_sha256_ctx *ctx, uint8_t out[ECLAT_SHA256_DIGEST_LEN]) {
    if (ctx == NULL || out == NULL) {
        return;
    }
    uint64_t bits = ctx->bits; /* sauvegardé AVANT le bourrage, qui ne doit pas le modifier */
    ctx->block[ctx->buffered++] = 0x80;
    if (ctx->buffered > 56) {
        memset(ctx->block + ctx->buffered, 0, sizeof(ctx->block) - ctx->buffered);
        compresser(ctx->h, ctx->block);
        ctx->buffered = 0;
    }
    memset(ctx->block + ctx->buffered, 0, 56 - ctx->buffered);
    for (int i = 0; i < 8; i++) {
        ctx->block[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    compresser(ctx->h, ctx->block);
    for (int i = 0; i < 8; i++) {
        ecrire_be32(out + (size_t)i * 4, ctx->h[i]);
    }
}

void eclat_sha256(const void *data, size_t len, uint8_t out[ECLAT_SHA256_DIGEST_LEN]) {
    eclat_sha256_ctx ctx;
    eclat_sha256_init(&ctx);
    eclat_sha256_update(&ctx, data, len);
    eclat_sha256_final(&ctx, out);
}

int eclat_sha256_hex(const uint8_t digest[ECLAT_SHA256_DIGEST_LEN],
                     char out[ECLAT_SHA256_HEX_LEN + 1]) {
    static const char HEX[] = "0123456789abcdef";
    if (digest == NULL || out == NULL) {
        return -1;
    }
    for (int i = 0; i < ECLAT_SHA256_DIGEST_LEN; i++) {
        out[2 * i] = HEX[digest[i] >> 4];
        out[2 * i + 1] = HEX[digest[i] & 0x0Fu];
    }
    out[ECLAT_SHA256_HEX_LEN] = '\0';
    return 0;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int eclat_sha256_from_hex(const char *hex, size_t len,
                          uint8_t out[ECLAT_SHA256_DIGEST_LEN]) {
    if (hex == NULL || out == NULL || len != ECLAT_SHA256_HEX_LEN) {
        return -1;
    }
    for (int i = 0; i < ECLAT_SHA256_DIGEST_LEN; i++) {
        int hi = hex_val(hex[2 * i]);
        int lo = hex_val(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

int eclat_sha256_equal(const uint8_t a[ECLAT_SHA256_DIGEST_LEN],
                       const uint8_t b[ECLAT_SHA256_DIGEST_LEN]) {
    if (a == NULL || b == NULL) {
        return 0;
    }
    uint8_t diff = 0;
    for (int i = 0; i < ECLAT_SHA256_DIGEST_LEN; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]); /* aucune sortie anticipée : temps constant */
    }
    return diff == 0;
}