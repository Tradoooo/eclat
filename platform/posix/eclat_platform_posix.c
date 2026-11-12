// SPDX-License-Identifier: Apache-2.0
/* Éclat — couche plateforme POSIX (Linux). Implémentation du contrat eclat_platform.h.
 * Homologue exacte de platform/qnx/ : même contrat, aucune ligne recopiée. */
#define _POSIX_C_SOURCE 200809L

#include "eclat_platform.h"

#include <stdio.h>
#include <time.h>

static uint64_t timespec_ns(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000000ULL + (uint64_t)ts->tv_nsec;
}

static uint64_t posix_monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return timespec_ns(&ts);
}

/* CLOCK_THREAD_CPUTIME_ID : CPU réellement consommé par ce thread, et non temps écoulé.
 * C'est la bonne grandeur pour un budget CPU (CONTRACT.md §4). */
static uint64_t posix_cpu_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        return 0;
    }
    return timespec_ns(&ts);
}

static void posix_emit(int level, const char *msg, size_t len) {
    /* La longueur fait foi : pas d'hypothèse de chaîne terminée par zéro. */
    fprintf(stderr, "[eclat log niv=%d] %.*s\n", level, (int)len, msg ? msg : "");
}

static void posix_fail(const char *msg) {
    fprintf(stderr, "eclat: erreur fatale : %s\n", msg ? msg : "(sans message)");
}

static const eclat_platform_t PLATFORM_POSIX = {
    posix_monotonic_ns,
    posix_cpu_ns,
    posix_emit,
    posix_fail,
};

const eclat_platform_t *eclat_platform_get(void) { return &PLATFORM_POSIX; }