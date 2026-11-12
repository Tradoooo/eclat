// SPDX-License-Identifier: Apache-2.0
/* Éclat — module de démonstration GOURMAND (ECL-06).
 * Boucle sans jamais rendre la main et sans appeler le courtier : seul le budget CPU, appliqué
 * par l'hôte, peut l'arrêter. C'est le cas que la démonstration met en avant — sur un noyau
 * monolithique, cette boucle emporte la boucle de contrôle avec elle. */
#include "imports.h"

__attribute__((export_name("run"))) int run(void) {
    LOG("gourmand : demarrage d'une boucle sans fin", NIVEAU_INFO);

    /* Accumulateur volatile : le compilateur ne peut pas supprimer la boucle. */
    volatile uint64_t compteur = 0;
    volatile uint64_t somme = 0;
    while (compteur < 0xFFFFFFFFFFFFFFFFULL) {
        somme += compteur % 7u;
        compteur++;
    }

    /* Jamais atteint en pratique : le budget CPU doit tuer le module avant. */
    LOG("gourmand : boucle terminee", NIVEAU_INFO);
    return 0;
}