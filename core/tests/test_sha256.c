// SPDX-License-Identifier: Apache-2.0
/* Éclat — tests unitaires de SHA-256 (ECL-07, ECL-21).
 * Vecteurs officiels FIPS 180-4 / NIST, vérifiés indépendamment par `sha256sum`.
 * Couvre aussi le hachage incrémental aux longueurs qui touchent le bourrage (55/56/57/63/64/65). */
#include "eclat/cap_sha256.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static void expect(int cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* Compare l'empreinte de `data` (len octets) à la valeur hexadécimale attendue. */
static void expect_digest(const void *data, size_t len, const char *hex_attendu,
                          const char *what) {
    uint8_t digest[ECLAT_SHA256_DIGEST_LEN];
    char obtenu[ECLAT_SHA256_HEX_LEN + 1];
    eclat_sha256(data, len, digest);
    eclat_sha256_hex(digest, obtenu);
    checks++;
    if (strcmp(obtenu, hex_attendu) != 0) {
        failures++;
        printf("FAIL: %s\n  attendu : %s\n  obtenu : %s\n", what, hex_attendu, obtenu);
    }
}

/* Vecteurs NIST. */
#define VEC_VIDE "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
#define VEC_ABC "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
#define VEC_448 \
    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
#define VEC_MILLION "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"

static void test_vecteurs_nist(void) {
    expect_digest("", 0, VEC_VIDE, "ECL-07 NIST message vide");
    expect_digest("abc", 3, VEC_ABC, "ECL-07 NIST « abc »");
    const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    expect_digest(m, strlen(m), VEC_448, "ECL-07 NIST 448 bits");
}

/* Un million de 'a' : calculé par update répétés, sans allouer un million d'octets. */
static void test_million(void) {
    static uint8_t morceau[1000];
    memset(morceau, 'a', sizeof(morceau));
    eclat_sha256_ctx ctx;
    eclat_sha256_init(&ctx);
    for (int i = 0; i < 1000; i++) {
        eclat_sha256_update(&ctx, morceau, sizeof(morceau));
    }
    uint8_t digest[ECLAT_SHA256_DIGEST_LEN];
    char obtenu[ECLAT_SHA256_HEX_LEN + 1];
    eclat_sha256_final(&ctx, digest);
    eclat_sha256_hex(digest, obtenu);
    checks++;
    if (strcmp(obtenu, VEC_MILLION) != 0) {
        failures++;
        printf("FAIL: ECL-07 NIST un million de 'a'\n  obtenu : %s\n", obtenu);
    }
}

/* L'incrémental doit donner exactement le même résultat que le one-shot, pour des longueurs
 * qui touchent toutes les frontières du bourrage. */
static void test_incremental(void) {
    static const size_t tailles[] = {0,  1,  2,  3,  31, 32, 55, 56,  57,  63,
                                     64, 65, 119, 120, 121, 127, 128, 129, 255, 1000};
    uint8_t donnees[1000];
    for (size_t i = 0; i < sizeof(donnees); i++) {
        donnees[i] = (uint8_t)(i * 37u + 11u);
    }
    for (size_t t = 0; t < sizeof(tailles) / sizeof(tailles[0]); t++) {
        size_t len = tailles[t];
        uint8_t ref[ECLAT_SHA256_DIGEST_LEN];
        eclat_sha256(donnees, len, ref);
        /* Découpage en morceaux de 7 octets : force plusieurs blocs partiels. */
        eclat_sha256_ctx ctx;
        eclat_sha256_init(&ctx);
        size_t reste = len;
        size_t pos = 0;
        while (reste > 0) {
            size_t n = (reste < 7) ? reste : 7;
            eclat_sha256_update(&ctx, donnees + pos, n);
            pos += n;
            reste -= n;
        }
        uint8_t got[ECLAT_SHA256_DIGEST_LEN];
        eclat_sha256_final(&ctx, got);
        char nom[64];
        snprintf(nom, sizeof(nom), "ECL-07 incrémental len=%zu", len);
        expect(memcmp(ref, got, ECLAT_SHA256_DIGEST_LEN) == 0, nom);
    }
}

/* 'a'→'A' sans dépendre de ctype.h (core/ reste minimal). */
static int majuscule(int c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

static void test_hex(void) {
    uint8_t digest[ECLAT_SHA256_DIGEST_LEN];
    char hex[ECLAT_SHA256_HEX_LEN + 1];
    eclat_sha256("abc", 3, digest);
    expect(eclat_sha256_hex(digest, hex) == 0, "ECL-07 hex retourne 0");
    expect(strlen(hex) == 64, "ECL-07 hex produit 64 caractères");

    uint8_t relu[ECLAT_SHA256_DIGEST_LEN];
    expect(eclat_sha256_from_hex(hex, 64, relu) == 0, "ECL-07 from_hex accepte minuscules");
    expect(memcmp(digest, relu, 32) == 0, "ECL-07 aller-retour hex identique");

    /* Majuscules acceptées. */
    char maj[65];
    for (int i = 0; i < 64; i++) {
        maj[i] = (char)majuscule(hex[i]);
    }
    maj[64] = '\0';
    expect(eclat_sha256_from_hex(maj, 64, relu) == 0, "ECL-07 from_hex accepte majuscules");
    expect(memcmp(digest, relu, 32) == 0, "ECL-07 majuscules décodent pareil");

    /* Longueurs invalides. */
    expect(eclat_sha256_from_hex(hex, 63, relu) == -1, "ECL-07 from_hex refuse 63");
    expect(eclat_sha256_from_hex(hex, 65, relu) == -1, "ECL-07 from_hex refuse 65");
    char mauvais[65];
    memcpy(mauvais, hex, 64);
    mauvais[64] = '\0';
    mauvais[10] = 'z';
    expect(eclat_sha256_from_hex(mauvais, 64, relu) == -1, "ECL-07 from_hex refuse non-hex");
}

static void test_equal(void) {
    uint8_t a[ECLAT_SHA256_DIGEST_LEN];
    uint8_t b[ECLAT_SHA256_DIGEST_LEN];
    eclat_sha256("module-v1", 9, a);
    eclat_sha256("module-v1", 9, b);
    expect(eclat_sha256_equal(a, b) == 1, "ECL-07 equal empreintes identiques");
    b[0] ^= 0x01;
    expect(eclat_sha256_equal(a, b) == 0, "ECL-07 equal diffère d'un bit");
    b[0] ^= 0x01;
    b[31] ^= 0x80;
    expect(eclat_sha256_equal(a, b) == 0, "ECL-07 equal diffère du dernier octet");
    expect(eclat_sha256_equal(NULL, b) == 0, "ECL-07 equal NULL refusé");
}

int main(void) {
    test_vecteurs_nist();
    test_million();
    test_incremental();
    test_hex();
    test_equal();
    if (failures) {
        printf("FAIL %d/%d checks\n", failures, checks);
        return 1;
    }
    printf("OK %d checks\n", checks);
    return 0;
}