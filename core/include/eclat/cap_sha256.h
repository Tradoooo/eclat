// SPDX-License-Identifier: Apache-2.0
/* Éclat — SHA-256 (FIPS 180-4) en C11 pur.
 * ECL-07 : l'empreinte du module est vérifiée avant chargement ; un octet modifié refuse le
 * chargement. ECL-21 : socle de l'attestation.
 *
 * Aucune allocation, aucune I/O, aucun état global mutable : l'état vit dans le contexte fourni
 * par l'appelant. Portable : l'ordre des octets est traité explicitement, sans dépendre de
 * l'endiannisme de la machine (QNX aarch64le comme x86_64). */
#ifndef ECLAT_CAP_SHA256_H
#define ECLAT_CAP_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECLAT_SHA256_DIGEST_LEN 32
#define ECLAT_SHA256_HEX_LEN 64

/* Contexte de hachage incrémental. */
typedef struct eclat_sha256_ctx {
    uint32_t h[8];      /* état courant */
    uint8_t block[64];  /* bloc partiel */
    uint64_t bits;      /* nombre total de bits consommés */
    size_t buffered;    /* octets en attente dans block */
} eclat_sha256_ctx;

void eclat_sha256_init(eclat_sha256_ctx *ctx);
void eclat_sha256_update(eclat_sha256_ctx *ctx, const void *data, size_t len);
void eclat_sha256_final(eclat_sha256_ctx *ctx, uint8_t out[ECLAT_SHA256_DIGEST_LEN]);

/* Hachage d'un seul tenant. */
void eclat_sha256(const void *data, size_t len, uint8_t out[ECLAT_SHA256_DIGEST_LEN]);

/* Empreinte → 64 caractères hexadécimaux minuscules + NUL. Retourne 0. */
int eclat_sha256_hex(const uint8_t digest[ECLAT_SHA256_DIGEST_LEN],
                     char out[ECLAT_SHA256_HEX_LEN + 1]);

/* 64 caractères hexadécimaux (majuscules ou minuscules) → empreinte.
 * Retourne 0 si valide, -1 sinon (longueur ≠ 64 ou caractère non hexadécimal). */
int eclat_sha256_from_hex(const char *hex, size_t len,
                          uint8_t out[ECLAT_SHA256_DIGEST_LEN]);

/* Comparaison en temps constant : 1 si égales, 0 sinon. Aucune sortie anticipée, pour ne pas
 * fuir la position du premier octet différent. */
int eclat_sha256_equal(const uint8_t a[ECLAT_SHA256_DIGEST_LEN],
                       const uint8_t b[ECLAT_SHA256_DIGEST_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_CAP_SHA256_H */