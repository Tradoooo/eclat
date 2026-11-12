// SPDX-License-Identifier: Apache-2.0
/* Éclat — couche plateforme QNX. Implémentation du contrat eclat_platform.h.
 *
 * Homologue de platform/posix/ : même contrat, **aucune ligne recopiée**. Les deux
 * implémentations diffèrent volontairement là où QNX offre mieux qu'un appel POSIX générique :
 *
 *   - horloge monotone : `ClockTime()` de QNX renvoie directement des nanosecondes en un seul
 *     appel noyau, là où POSIX impose clock_gettime + conversion timespec. Sur une boucle de
 *     contrôle à 1 kHz où chaque import passe par le courtier, cet appel est sur le chemin
 *     critique : on prend le moins cher des deux.
 *   - temps CPU : `CLOCK_THREAD_CPUTIME_ID` existe dans le time.h de QNX 8.0 (vérifié, valeur 4),
 *     donc la grandeur « CPU du thread » est la même que sous POSIX — c'est la bonne grandeur
 *     pour un budget CPU, et non le temps écoulé.
 *   - journal : `slogger2.h` n'est PAS présent dans les en-têtes du SDP 8.0 installé (le journal
 *     système QNX est dans un paquet séparé). La sortie se fait donc sur stderr, et le branchement
 *     sur slogger2 est explicitement signalé comme reste à faire plutôt que silently absent.
 *
 * ECL-10 / ECL-11 : cette couche est ce que le courtier utilisera sur cible. */
/* _QNX_SOURCE débloque les extensions QNX et les déclarations POSIX que -std=c11 strict
 * masque sinon (CLOCK_MONOTONIC, CLOCK_THREAD_CPUTIME_ID, clock_gettime, EOK).
 * Garde #ifndef : la chaîne de construction QNX peut aussi le passer par -D. */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include "eclat_platform.h"

#include <errno.h>
#include <stdio.h>
#include <sys/neutrino.h>
#include <time.h>

/* Horloge monotone QNX : un appel, des nanosecondes. */
static uint64_t qnx_monotonic_ns(void) {
    _Uint64t ns = 0;
    if (ClockTime(CLOCK_MONOTONIC, NULL, &ns) != EOK) {
        return 0;
    }
    return (uint64_t)ns;
}

/* CPU réellement consommé par ce thread — grandeur du budget CPU (CONTRACT.md §4). */
static uint64_t qnx_cpu_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* RESTE À FAIRE : router vers slogger2 quand le paquet est installé, pour que le journal des
 * décisions rejoigne le journal système de la cible au lieu de stderr. */
static void qnx_emit(int level, const char *msg, size_t len) {
    fprintf(stderr, "[eclat log niv=%d] %.*s\n", level, (int)len, msg ? msg : "");
}

static void qnx_fail(const char *msg) {
    fprintf(stderr, "eclat: erreur fatale : %s\n", msg ? msg : "(sans message)");
}

static const eclat_platform_t PLATFORM_QNX = {
    qnx_monotonic_ns,
    qnx_cpu_ns,
    qnx_emit,
    qnx_fail,
};

const eclat_platform_t *eclat_platform_get(void) { return &PLATFORM_QNX; }