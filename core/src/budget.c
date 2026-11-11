// SPDX-License-Identifier: Apache-2.0
/* Éclat — budget CPU et quota d'appels (core, C11 pur : ni horloge, ni allocation, ni état
 * global ; le temps est fourni par l'appelant).
 * ECL-06 : le budget CPU est respecté ; un module qui boucle est signalé/tué.
 * CONTRACT.md §4 : fenêtre glissante de 1 000 000 000 ns ; budget 0 = illimité ;
 * compte puis vérifie ; retour 0 OK, 1 CPU dépassé, 2 quota d'appels dépassé (2 prime).
 */
#include "eclat/cap_budget.h"

#include <stddef.h>

/* CONTRACT.md §4 : la fenêtre dure 1e9 ns. */
#define ECLAT_BUDGET_WINDOW_NS 1000000000ULL
/* CONTRACT.md §4 : budget = cpu_budget_ms_per_s * 1 000 000 ns. */
#define ECLAT_NS_PER_MS 1000000ULL
/* Sentinelle : win_start_ns est posé au premier charge (cap_budget.h). */
#define ECLAT_WIN_UNSET UINT64_MAX

void eclat_budget_init(eclat_budget_t *b, uint32_t cpu_budget_ms_per_s, uint32_t calls_per_s) {
    if (b == NULL) {
        return;
    }
    b->cpu_budget_ms_per_s = cpu_budget_ms_per_s;
    b->calls_per_s = calls_per_s;
    b->win_start_ns = ECLAT_WIN_UNSET;
    b->cpu_ns_used = 0;
    b->calls_used = 0;
    b->cpu_exceeded = 0;
    b->calls_exceeded = 0;
}

/* CONTRACT.md §4 : quand now_ns - win_start_ns >= 1e9 la fenêtre repart —
 * win_start_ns = now_ns, compteurs remis à zéro, drapeaux d'excès remis à zéro. */
static void window_restart_if_needed(eclat_budget_t *b, uint64_t now_ns) {
    if (b->win_start_ns == ECLAT_WIN_UNSET || now_ns - b->win_start_ns >= ECLAT_BUDGET_WINDOW_NS) {
        b->win_start_ns = now_ns;
        b->cpu_ns_used = 0;
        b->calls_used = 0;
        b->cpu_exceeded = 0;
        b->calls_exceeded = 0;
    }
}

/* CONTRACT.md §4 : compte le temps puis vérifie. Les drapeaux cpu_exceeded/calls_exceeded
 * restent levés (latched) jusqu'à la fin de la fenêtre. 2 (quota) prime sur 1 (CPU). */
int eclat_budget_charge(eclat_budget_t *b, uint64_t now_ns, uint64_t cpu_ns, uint32_t n_calls) {
    if (b == NULL) {
        return 0;
    }
    window_restart_if_needed(b, now_ns);
    /* Compte d'abord… */
    b->cpu_ns_used += cpu_ns;
    b->calls_used += n_calls;
    /* …puis vérifie. Budget 0 = illimité (CONTRACT.md §4). */
    uint64_t cpu_budget_ns = (uint64_t)b->cpu_budget_ms_per_s * ECLAT_NS_PER_MS;
    if (b->cpu_budget_ms_per_s != 0 && b->cpu_ns_used > cpu_budget_ns) {
        b->cpu_exceeded = 1;
    }
    if (b->calls_per_s != 0 && b->calls_used > b->calls_per_s) {
        b->calls_exceeded = 1;
    }
    if (b->calls_exceeded) {
        return 2; /* le quota d'appels prime */
    }
    if (b->cpu_exceeded) {
        return 1;
    }
    return 0;
}
