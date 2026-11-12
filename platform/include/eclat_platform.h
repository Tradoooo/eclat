// SPDX-License-Identifier: Apache-2.0
/* Éclat — contrat de la couche plateforme.
 *
 * Point d'architecture décisif (CONTRACT.md §7) : UNE déclaration d'interface, DEUX
 * implémentations — `platform/posix/` (Linux) et `platform/qnx/` (cible). Le runtime et le
 * courtier ne connaissent que ce contrat, donc toute la logique se teste sur PC avant d'avoir
 * le SDP. Jamais de code recopié d'une couche à l'autre.
 *
 * Aucune de ces fonctions ne doit bloquer : elles sont appelées sur le chemin de la boucle de
 * contrôle. */
#ifndef ECLAT_PLATFORM_H
#define ECLAT_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Niveaux de journalisation, alignés sur cap_log.h. */
enum {
    ECLAT_PLAT_LOG_DEBUG = 0,
    ECLAT_PLAT_LOG_INFO = 1,
    ECLAT_PLAT_LOG_WARN = 2,
    ECLAT_PLAT_LOG_ERROR = 3
};

typedef struct eclat_platform {
    /* Horloge monotone en nanosecondes. Sur QNX : clock_gettime(CLOCK_MONOTONIC). */
    uint64_t (*monotonic_ns)(void);

    /* Temps CPU consommé par le thread courant, en nanosecondes. Sert au budget
     * (CONTRACT.md §4). Sur QNX : CLOCK_THREAD_CPUTIME_ID ou équivalent. */
    uint64_t (*cpu_ns)(void);

    /* Émet une ligne de journal. `msg` n'est PAS nécessairement terminé par zéro : la
     * longueur fait foi (contrat : aucune chaîne NUL admise). */
    void (*emit)(int level, const char *msg, size_t len);

    /* Signale une erreur fatale de l'hôte (ne retourne pas nécessairement). */
    void (*fail)(const char *msg);
} eclat_platform_t;

/* Implémentation de la cible courante. Fournie par platform/posix/ ou platform/qnx/. */
const eclat_platform_t *eclat_platform_get(void);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_PLATFORM_H */