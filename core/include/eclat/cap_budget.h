// SPDX-License-Identifier: Apache-2.0
/* Éclat — budget CPU et quota d'appels (core, C11 pur).
 * ECL-06 : le budget CPU est respecté ; un module qui boucle est tué et le dépassement est
 * journalisé.
 * Pas d'horloge interne : le temps (monotone, en nanosecondes) est fourni par l'appelant.
 */
#ifndef ECLAT_CAP_BUDGET_H
#define ECLAT_CAP_BUDGET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Budget par fenêtre glissante d'une seconde (CONTRACT.md §4). */
typedef struct eclat_budget {
    uint32_t cpu_budget_ms_per_s; /* 0 = illimité */
    uint32_t calls_per_s;         /* 0 = illimité */
    uint64_t win_start_ns;        /* début de la fenêtre glissante courante */
    uint64_t cpu_ns_used;         /* CPU compté dans la fenêtre courante */
    uint32_t calls_used;          /* appels comptés dans la fenêtre courante */
    int cpu_exceeded;             /* levé, ne retombe qu'en fin de fenêtre */
    int calls_exceeded;
} eclat_budget_t;

/* Initialise. Les compteurs sont à zéro ; win_start_ns est posé au premier charge. */
void eclat_budget_init(eclat_budget_t *b, uint32_t cpu_budget_ms_per_s, uint32_t calls_per_s);

/* Compte `cpu_ns` de CPU et `n_calls` appels au temps `now_ns` (monotone, ns).
 * Retourne 0 si dans le budget, 1 si le budget CPU est dépassé, 2 si le quota d'appels est
 * dépassé (2 prime sur 1). Les drapeaux d'excès restent levés jusqu'à la fin de la fenêtre. */
int eclat_budget_charge(eclat_budget_t *b, uint64_t now_ns, uint64_t cpu_ns, uint32_t n_calls);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_CAP_BUDGET_H */