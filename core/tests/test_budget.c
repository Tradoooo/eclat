// SPDX-License-Identifier: Apache-2.0
/* Éclat — tests ECL-06 du budget (fenêtre glissante d'une seconde, CONTRACT.md §4).
 * Le temps est injecté (core pur : aucune horloge). */
#include "eclat/cap_budget.h"

#include <stdio.h>

#define WIN 1000000000ULL /* CONTRACT.md §4 : fenêtre de 1e9 ns */
#define MS 1000000ULL     /* 1 ms = 1e6 ns */

static int checks = 0;
static int failures = 0;

static void check(int cond, const char *name) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL %s (check #%d)\n", name, checks);
    }
}

/* init : compteurs à zéro, budgets enregistrés ; win_start posé au premier charge. */
static void test_init(void) {
    eclat_budget_t b;
    eclat_budget_init(&b, 50, 200);
    check(b.cpu_budget_ms_per_s == 50, "ECL-06 init cpu_budget_ms_per_s");
    check(b.calls_per_s == 200, "ECL-06 init calls_per_s");
    check(b.cpu_ns_used == 0, "ECL-06 init cpu_ns_used == 0");
    check(b.calls_used == 0, "ECL-06 init calls_used == 0");
    check(b.cpu_exceeded == 0, "ECL-06 init cpu_exceeded == 0");
    check(b.calls_exceeded == 0, "ECL-06 init calls_exceeded == 0");
    check(eclat_budget_charge(&b, 12345, 0, 0) == 0, "ECL-06 premier charge pose la fenêtre");
    check(b.win_start_ns == 12345, "ECL-06 win_start_ns posé au premier charge");
}

/* §4 : budget 0 = illimité (cpu et calls). */
static void test_unlimited(void) {
    eclat_budget_t b;
    eclat_budget_init(&b, 0, 0);
    check(eclat_budget_charge(&b, 0, 10ULL * WIN, 1) == 0, "ECL-06 budget 0 : CPU illimité");
    check(eclat_budget_charge(&b, 100, 123456789ULL * MS, 1000000) == 0,
          "ECL-06 budget 0 : charges énormes -> 0");
    check(b.cpu_exceeded == 0 && b.calls_exceeded == 0, "ECL-06 budget 0 : drapeaux à zéro");
    eclat_budget_t c;
    eclat_budget_init(&c, 0, 5);
    check(eclat_budget_charge(&c, 0, 999ULL * WIN, 1) == 0, "ECL-06 cpu 0 illimité, calls limité");
    for (int i = 0; i < 5; i++) {
        eclat_budget_charge(&c, 0, 0, 1);
    }
    check(eclat_budget_charge(&c, 0, 0, 1) == 2, "ECL-06 calls limité malgré cpu 0 -> 2");
}

/* §4 : CPU dépassé -> retour 1 ; exactement au budget -> 0 ; drapeau latched. */
static void test_cpu_exceeded(void) {
    eclat_budget_t b;
    eclat_budget_init(&b, 50, 0);
    check(eclat_budget_charge(&b, 0, 49 * MS, 1) == 0, "ECL-06 49ms/50ms -> 0");
    check(eclat_budget_charge(&b, 1000, 1 * MS, 1) == 0, "ECL-06 cumul == budget -> 0 (inclusif)");
    check(eclat_budget_charge(&b, 2000, 1, 1) == 1, "ECL-06 budget+1ns -> 1");
    check(b.cpu_exceeded == 1, "ECL-06 cpu_exceeded levé");
    check(eclat_budget_charge(&b, 3000, 0, 1) == 1, "ECL-06 drapeau latched : charge nulle -> 1");
    check(eclat_budget_charge(&b, WIN - 1, 0, 0) == 1, "ECL-06 latched jusqu'à fin de fenêtre");
}

/* §4 : quota d'appels dépassé -> retour 2 ; 2 prime sur 1. */
static void test_calls_exceeded(void) {
    eclat_budget_t b;
    eclat_budget_init(&b, 0, 3);
    check(eclat_budget_charge(&b, 0, 0, 2) == 0, "ECL-06 2 appels/3 -> 0");
    check(eclat_budget_charge(&b, 10, 0, 1) == 0, "ECL-06 3 appels == quota -> 0 (inclusif)");
    check(eclat_budget_charge(&b, 20, 0, 1) == 2, "ECL-06 4e appel -> 2");
    check(b.calls_exceeded == 1, "ECL-06 calls_exceeded levé");
    check(eclat_budget_charge(&b, 30, 0, 0) == 2, "ECL-06 quota latched -> 2");
    eclat_budget_t c;
    eclat_budget_init(&c, 10, 1);
    check(eclat_budget_charge(&c, 0, 11 * MS, 2) == 2, "ECL-06 CPU+quota dépassés : 2 prime sur 1");
    check(c.cpu_exceeded == 1 && c.calls_exceeded == 1, "ECL-06 les deux drapeaux levés");
}

/* §4 : now - win_start >= 1e9 -> fenêtre repart (compteurs et drapeaux à zéro). */
static void test_window_restart(void) {
    eclat_budget_t b;
    eclat_budget_init(&b, 50, 2);
    check(eclat_budget_charge(&b, 1000, 51 * MS, 3) == 2, "ECL-06 dépassement CPU+quota -> 2");
    check(eclat_budget_charge(&b, 1000 + WIN - 1, 0, 0) == 2,
          "ECL-06 pas de restart à 1e9-1 : drapeaux latched");
    check(b.win_start_ns == 1000, "ECL-06 win_start inchangé avant 1e9");
    check(eclat_budget_charge(&b, 1000 + WIN, 0, 0) == 0, "ECL-06 restart à >= 1e9 -> 0");
    check(b.win_start_ns == 1000 + WIN, "ECL-06 win_start_ns = now_ns au restart");
    check(b.cpu_ns_used == 0 && b.calls_used == 0, "ECL-06 compteurs remis à zéro");
    check(b.cpu_exceeded == 0 && b.calls_exceeded == 0, "ECL-06 drapeaux remis à zéro");
    check(eclat_budget_charge(&b, 1000 + WIN + 1, 10 * MS, 1) == 0,
          "ECL-06 nouvelle fenêtre : budget retrouvé");
    check(eclat_budget_charge(&b, 1000 + 2 * WIN, 0, 0) == 0, "ECL-06 saut de plusieurs fenêtres");
    check(b.win_start_ns == 1000 + 2 * WIN, "ECL-06 win_start suit le dernier restart");
}

int main(void) {
    test_init();
    test_unlimited();
    test_cpu_exceeded();
    test_calls_exceeded();
    test_window_restart();
    if (failures != 0) {
        printf("KO %d/%d checks\n", failures, checks);
        return 1;
    }
    printf("OK %d checks\n", checks);
    return 0;
}
