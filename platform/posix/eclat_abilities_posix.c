// SPDX-License-Identifier: Apache-2.0
/* Éclat — couche « abilities » hors QNX : il n'y a rien à retirer.
 *
 * Homologue de platform/qnx/eclat_abilities_qnx.c, sans une ligne recopiée : sur une cible
 * POSIX il n'existe pas d'abilities QNX, donc la liste est vide et le retrait est un no-op qui
 * réussit. C'est ce qui permet au runtime portable d'appeler eclat_abilities_drop_runtime()
 * sans `#ifdef` : la décision « faut-il retirer quelque chose ? » est prise par la couche, pas
 * par le caller.
 */
#include "eclat_abilities.h"

#include <string.h>

size_t eclat_abilities_ciblees_count(void) {
    return 0;
}

const eclat_ability_ciblee_t *eclat_abilities_ciblees(void) {
    return NULL;
}

int eclat_abilities_drop_one(unsigned id, int *errno_refus, char *err, size_t errlen) {
    (void)id;
    if (errno_refus != NULL) {
        *errno_refus = 0;
    }
    if (err != NULL && errlen > 0) {
        err[0] = '\0';
    }
    return 0;
}

int eclat_abilities_drop_runtime(eclat_abilities_bilan_t *bilan, char *err, size_t errlen) {
    if (bilan != NULL) {
        memset(bilan, 0, sizeof(*bilan));
    }
    if (err != NULL && errlen > 0) {
        err[0] = '\0';
    }
    return 0;
}

int eclat_abilities_drop_all(char *err, size_t errlen) {
    if (err != NULL && errlen > 0) {
        err[0] = '\0';
    }
    return 0;
}

int eclat_abilities_wx_encore_possible(void) {
    return 0;
}
