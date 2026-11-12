// SPDX-License-Identifier: Apache-2.0
/* Éclat — harnais de fuzz du parseur de politique (ECL-08, ECL-15).
 * Outil HÔTE : lit un fichier (argv[1]) ou stdin, appelle eclat_policy_parse, et touche les
 * champs parsés pour qu'ASan voie toute lecture hors bornes.
 * Codes de sortie : 0 = politique acceptée, 2 = politique refusée, 3 = entrée illisible.
 * Sous ASan/UBSan, tout autre code (ou un plantage) est un bogue. */
#include "eclat/cap_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT (64 * 1024)

int main(int argc, char **argv) {
    static uint8_t buf[MAX_INPUT];
    FILE *f = stdin;
    if (argc > 1) {
        f = fopen(argv[1], "rb");
        if (f == NULL) {
            return 3;
        }
    }
    size_t len = fread(buf, 1, sizeof(buf), f);
    if (f != stdin) {
        fclose(f);
    }

    eclat_policy_t policy;
    eclat_policy_error_t err;
    memset(&policy, 0, sizeof(policy));
    memset(&err, 0, sizeof(err));

    int rc = eclat_policy_parse(buf, len, &policy, &err);
    if (rc == 0) {
        /* Touche les champs parsés : une lecture hors bornes est détectée par ASan. */
        volatile unsigned long sum = (unsigned long)policy.n_allows + policy.n_denies +
                                     policy.calls_per_s + policy.name_len + policy.sha256[31];
        for (uint32_t i = 0; i < policy.n_allows; i++) {
            sum += (unsigned long)policy.allows[i].cap + (unsigned long)policy.allows[i].id_min +
                   (unsigned long)policy.allows[i].val_max;
        }
        for (uint32_t i = 0; i < policy.n_denies; i++) {
            sum += (unsigned long)policy.denies[i];
        }
        (void)sum;
        printf("OK\n");
        return 0;
    }
    printf("ERROR:%s:%s\n", err.field, err.message);
    return 2;
}