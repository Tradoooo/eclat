// SPDX-License-Identifier: Apache-2.0
/* Éclat — couche « abilities » : le retrait CIBLÉ des privilèges (QNX 8.0).
 *
 * ECL-14, troisième couche. La politique `secpol` dessine le périmètre de communication du
 * runtime ; le retrait d'abilities dessine son périmètre de privilèges. Ce fichier fait le
 * second — de façon CIBLÉE : on retire ce que le runtime n'utilise pas, et on garde
 * explicitement ce qu'il lui faut.
 *
 * Ce qu'on garde, et pourquoi (c'est mesuré, pas supposé) :
 *
 *   - `fork`      : le superviseur (target/eclat_run.c) lance le module dans un processus
 *                   enfant et le tue si le budget CPU est dépassé (ECL-06). Sans `fork`, plus de
 *                   mise à mort possible.
 *   - les entrées/sorties de fichier : le runtime lit le `.wasm` et son empreinte.
 *   - la connexion au courtier : c'est le seul canal autorisé par le type secpol, et le
 *                   runtime en a besoin pour chaque import.
 *
 * Ce qu'on retire, et pourquoi le runtime s'en passe :
 *
 *   - `prot_write_and_exec` : une page à la fois inscriptible et exécutable est le cas
 *                   classique du W+X ; un runtime qui exécute du code non certifié n'en a pas
 *                   besoin (le module est interprété, pas du code machine).
 *   - `public_channel`      : le runtime se CONNECTE au courtier, il ne publie aucun canal.
 *   - `interrupt`, `io`     : piloter le matériel n'est pas le métier d'un runtime.
 *   - `priority`, `trace`   : réordonner le système ou espionner les autres processus, non plus.
 *   - `pathspace`           : le runtime ne crée aucun chemin ; il passe par le courtier.
 *   - `aps_root`            : modifier le partitionnement adaptatif n'appartient pas au runtime.
 *
 * L'idiome est celui de la documentation QNX : chaque ability est un descripteur
 * (domaine | opération | identifiant) et la liste se termine par PROCMGR_AID_EOL. Le LOCK
 * empêche la reprise : un processus qui a lâché un privilège ne peut pas le réclamer ensuite.
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include "eclat_abilities.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/procmgr.h>
#include <unistd.h>

/* La liste des abilities retirées, dans l'ordre du code source. */
static const eclat_ability_ciblee_t CIBLEES[] = {
    {PROCMGR_AID_PROT_WRITE_AND_EXEC, "prot_write_and_exec",
     "mémoire inscriptible et exécutable à la fois : un module interprété n'en a pas besoin"},
    {PROCMGR_AID_PUBLIC_CHANNEL, "public_channel",
     "publier un canal : le runtime se connecte au courtier, il ne publie rien"},
    {PROCMGR_AID_INTERRUPT, "interrupt", "piloter les interruptions matérielles"},
    {PROCMGR_AID_IO, "io", "accéder aux ports d'entrée/sortie du matériel"},
    {PROCMGR_AID_PRIORITY, "priority", "changer les priorités : le module ne réordonne pas le système"},
    {PROCMGR_AID_TRACE, "trace", "tracer les autres processus"},
    {PROCMGR_AID_PATHSPACE, "pathspace", "créer des chemins : le runtime publie par le courtier"},
    {PROCMGR_AID_APS_ROOT, "aps_root", "modifier le partitionnement adaptatif du système"},
};

#define N_CIBLEES (sizeof(CIBLEES) / sizeof(CIBLEES[0]))

size_t eclat_abilities_ciblees_count(void) {
    return N_CIBLEES;
}

const eclat_ability_ciblee_t *eclat_abilities_ciblees(void) {
    return CIBLEES;
}

int eclat_abilities_drop_one(unsigned id, int *errno_refus, char *err, size_t errlen) {
    if (errno_refus != NULL) {
        *errno_refus = 0;
    }
    if (err != NULL && errlen > 0) {
        err[0] = '\0';
    }
    /* L'idiome documenté : l'identifiant de l'ability est dans le DESCRIPTEUR (bits bas), et
     * la liste se termine par PROCMGR_AID_EOL. Un appel par ability — c'est la forme la moins
     * ambiguë (dans une liste partagée, les flags d'une ability-valideraient toutes les
     * suivantes).
     *
     * Les deux domaines sont visés (root ET non-root) : le runtime tourne en root dans l'image
     * de démonstration, mais il doit rester contraint s'il perd ses privilèges. */
    errno = 0;
    if (procmgr_ability(0, PROCMGR_ADN_ROOT | PROCMGR_ADN_NONROOT | PROCMGR_AOP_DENY |
                                  PROCMGR_AOP_LOCK | id,
                        PROCMGR_AID_EOL) != 0) {
        int e = errno;
        if (errno_refus != NULL) {
            *errno_refus = e;
        }
        if (err != NULL && errlen > 0) {
            snprintf(err, errlen, "%s", strerror(e));
        }
        return -1;
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
    /* On ne s'arrête pas au premier refus : le bilan doit dire ce qui a été retiré ET ce qui a
     * résisté. Un retrait partiel n'est pas un retrait réussi, et `err` le dit. */
    for (size_t i = 0; i < N_CIBLEES; i++) {
        char detail[128];
        int refus = 0;
        if (eclat_abilities_drop_one(CIBLEES[i].id, &refus, detail, sizeof(detail)) == 0) {
            if (bilan != NULL) {
                bilan->retirees++;
            }
            continue;
        }
        if (bilan != NULL) {
            bilan->refusees++;
            if (bilan->premier_refus == NULL) {
                bilan->premier_refus = CIBLEES[i].nom;
                bilan->premier_errno = refus;
            }
        }
        if (err != NULL && errlen > 0 && err[0] == '\0') {
            snprintf(err, errlen, "retrait de %s refusé : %s", CIBLEES[i].nom, detail);
        }
    }
    size_t refusees = (bilan != NULL) ? bilan->refusees : 0;
    return (refusees == 0) ? 0 : -1;
}

/* Variante radicale : tout retirer, y compris ce dont le runtime a besoin. */
int eclat_abilities_drop_all(char *err, size_t errlen) {
    if (err != NULL && errlen > 0) {
        err[0] = '\0';
    }
    errno = 0;
    if (procmgr_ability(0, PROCMGR_ADN_ROOT | PROCMGR_ADN_NONROOT | PROCMGR_AOP_DENY |
                                  PROCMGR_AOP_LOCK | PROCMGR_AID_EOL) != 0) {
        int e = errno;
        if (err != NULL && errlen > 0) {
            snprintf(err, errlen, "abandon total des abilities refusé : %s", strerror(e));
        }
        return -1;
    }
    return 0;
}

/* Le privilège dangereux est-il ENCORE accessible ? On ne devine pas : on tente.
 *
 * `prot_write_and_exec` retiré, une page inscriptible ET exécutable doit être refusée par le
 * noyau. Si elle passe encore, le retrait n'a pas été effectif — et le runtime ne doit pas
 * charger de code non certifié. C'est la mesure qui tranche, pas l'intention.
 *
 * Renvoie 1 si le privilège est encore accessible, 0 sinon. Hors QNX : 0 (sans objet). */
int eclat_abilities_wx_encore_possible(void) {
    errno = 0;
    void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) {
        return 0;
    }
    munmap(p, 4096);
    return 1;
}
