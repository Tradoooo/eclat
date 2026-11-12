// SPDX-License-Identifier: Apache-2.0
/* {{MODULE}} — logique métier du module tiers.
 *
 * Ce fichier est celui qui sera compilé en `.wasm`. Il ne connaît QUE l'ABI du contrat
 * (`module_imports.h`) : ni QNX, ni WAMR, ni socket, ni fichier. Chaque accès au monde
 * extérieur passe par un import, donc par une décision de politique du courtier.
 *
 * Le code est volontairement identique qu'il s'exécute sous le harnais natif Linux ou sous le
 * runtime : seul le fournisseur des imports change, jamais le module. */
#include "module_imports.h"

#include <stddef.h>

#define SENSOR_ID 1
#define ACTUATOR_ID 3
#define NIVEAU_INFO 1
#define NIVEAU_ALERTE 2

/* Le contrat interdit les chaînes terminées par zéro : on passe toujours pointeur + longueur.
 * `sizeof(littéral) - 1` est évalué à la compilation, donc aucune mesure à l'exécution. */
#define JOURNAL(msg, niveau) cap_log((msg), (int32_t)(sizeof(msg) - 1), (niveau))

/* Lecture d'un capteur autorisé. Retourne 0 si la lecture a abouti. */
static int32_t lire_capteur(int32_t *sample) {
    int32_t rc = cap_read_sensor(SENSOR_ID, sample, (int32_t)sizeof(*sample));
    if (rc != 0) {
        JOURNAL("lecture capteur refusée par la politique", NIVEAU_ALERTE);
    }
    return rc;
}

/* Écriture d'une consigne : c'est la politique qui impose la plage, jamais le module. */
static int32_t ecrire_consigne(int32_t sample) {
    int32_t consigne = sample / 2;
    int32_t rc = cap_write_actuator(ACTUATOR_ID, consigne);
    if (rc != 0) {
        JOURNAL("consigne refusée (hors plage ou non autorisée)", NIVEAU_ALERTE);
    }
    return rc;
}

/* Démonstration d'un accès NON autorisé : la politique doit le refuser, c'est attendu. */
static void tenter_acces_interdit(void) {
    int32_t rc = cap_net_send(9, "x", 1);
    if (rc != 0) {
        JOURNAL("accès réseau refusé, comme prévu", NIVEAU_INFO);
    }
}

int module_main(void) {
    int64_t debut = cap_time_ns();
    JOURNAL("{{MODULE}} démarré", NIVEAU_INFO);

    int32_t sample = 0;
    int32_t rc = lire_capteur(&sample);
    if (rc == 0) {
        rc = ecrire_consigne(sample);
    }
    tenter_acces_interdit();

    /* Un module n'a pas d'accès privilégié à l'horloge : la durée est mesurée côté courtier. */
    (void)(cap_time_ns() - debut);
    return rc;
}