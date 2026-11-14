// SPDX-License-Identifier: Apache-2.0
/* Éclat — contrat de la couche « abilities » : le retrait CIBLÉ des privilèges.
 *
 * Pourquoi un contrat : le retrait des abilities est un concept QNX (procmgr_ability), mais le
 * runtime qui l'exerce est du code portable C11. Comme pour `eclat_platform.h`, il y a donc UNE
 * déclaration et DEUX implémentations : `platform/qnx/` (le retrait réel) et `platform/posix/`
 * (rien à retirer : hors QNX, il n'y a pas d'abilities). Jamais de code recopié.
 *
 * Ce que « ciblé » veut dire, et pourquoi c'est mieux que « tout abandonner » :
 *
 *   - « tout abandonner » (PROCMGR_AID_EOL) retire TOUT, y compris ce dont le runtime a
 *     besoin pour charger son module, créer un processus et lire son fichier : c'est
 *     incompatible avec un runtime qui héberge du code.
 *   - « ciblé » retire une liste courte et justifiée : ce que le runtime n'utilise pas.
 *     Ce qui reste est dit explicitement, pour qu'on puisse le discuter au lieu de le croire.
 *
 * La liste est dans l'implémentation QNX ; ce contrat expose le compte, les noms et les
 * raisons, pour que le journal du runtime, la sonde et la documentation disent la même chose.
 */
#ifndef ECLAT_ABILITIES_H
#define ECLAT_ABILITIES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Une ability que le runtime retire de lui-même, et pourquoi il peut s'en passer. */
typedef struct eclat_ability_ciblee {
    unsigned id;         /* PROCMGR_AID_* sur QNX ; 0 ailleurs. */
    const char *nom;     /* nom QNX de l'ability (celui des politiques secpol). */
    const char *raison;  /* ce que le runtime perd, et pourquoi il s'en passe. */
} eclat_ability_ciblee_t;

/* Bilan d'un retrait : ce qui a été retiré, ce que le noyau a refusé.
 *
 * Un refus n'est pas forcément un échec de sécurité : sous une politique secpol, le type du
 * processus retire déjà ces abilities, et le noyau refuse alors de les retirer à nouveau.
 * Le bilan dit les deux, sans les confondre. */
typedef struct eclat_abilities_bilan {
    size_t retirees;         /* abilities effectivement retirées par le noyau. */
    size_t refusees;         /* abilities que le noyau a refusées. */
    const char *premier_refus; /* nom de la première ability refusée (NULL si aucune). */
    int premier_errno;       /* errno du premier refus (0 si aucun). */
} eclat_abilities_bilan_t;

/* Nombre d'abilities visées par eclat_abilities_drop_runtime(). 0 hors QNX. */
size_t eclat_abilities_ciblees_count(void);

/* La liste, dans l'ordre du code source. NULL si la liste est vide. */
const eclat_ability_ciblee_t *eclat_abilities_ciblees(void);

/* Retire UNE ability et la verrouille (PROCMGR_AOP_LOCK).
 *
 * Renvoie 0 si le noyau a accepté, -1 sinon ; dans ce cas `errno_refus` (si non NULL) reçoit
 * l'errno du refus et `err` (si non NULL) reçoit sa version lisible.
 *
 * Le verrouillage empêche la reprise : un processus qui a lâché un privilège ne peut pas le
 * réclamer ensuite. */
int eclat_abilities_drop_one(unsigned id, int *errno_refus, char *err, size_t errlen);

/* Retire les abilities listées, une par une, et les verrouille.
 *
 * Renvoie 0 si TOUTES les abilities ont été retirées, -1 sinon ; dans ce cas `err` (si non
 * NULL) reçoit le motif du PREMIER refus. `bilan` (si non NULL) est renseigné dans tous les
 * cas : on ne déclare pas un retrait partiel comme un retrait réussi.
 *
 * Ne bloque pas, ne modifie rien d'autre : c'est un appel fait une fois, au démarrage. */
int eclat_abilities_drop_runtime(eclat_abilities_bilan_t *bilan, char *err, size_t errlen);

/* Retire TOUTES les abilities (y compris celles dont le runtime a besoin) et les verrouille.
 *
 * C'est la variante « radicale », utile pour COMPARER : elle prouve que le retrait ciblé
 * laisse passer ce que la variante radicale casse. Elle ne doit pas être le comportement par
 * défaut d'un runtime qui charge un module. */
int eclat_abilities_drop_all(char *err, size_t errlen);

/* Le privilège « mémoire inscriptible ET exécutable » est-il ENCORE accessible ?
 *
 * Renvoie 1 si oui, 0 si non. Hors QNX : 0 (sans objet).
 *
 * C'est la mesure qui tranche : un retrait qui n'a pas été effectif doit se voir, pas se
 * supposer. */
int eclat_abilities_wx_encore_possible(void);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_ABILITIES_H */
