// SPDX-License-Identifier: Apache-2.0
/* Éclat — module de démonstration HOSTILE (ECL-05).
 * Tente quatre accès que la politique n'autorise pas. Chaque tentative doit être refusée, et
 * chaque refus journalisé. Le module reste vivant : un refus n'est pas un plantage, c'est la
 * frontière qui tient. */
#include "imports.h"

__attribute__((export_name("run"))) int run(void) {
    LOG("hostile : debut des tentatives", NIVEAU_INFO);

    /* 1. Exfiltration réseau : cap_net_send n'est pas dans la liste allow. */
    int32_t rc = cap_net_send(9, "donnees", 7);
    (void)rc;

    /* 2. Consigne hors plage : l'actionneur 3 est borné à [-1000, 1000]. */
    rc = cap_write_actuator(3, 99999);
    (void)rc;

    /* 3. Capteur non autorisé : seul l'identifiant 1 est dans la plage permise. */
    int32_t echantillon = 0;
    rc = cap_read_sensor(42, &echantillon, (int32_t)sizeof(echantillon));
    (void)rc;

    /* 4. Écriture dans la banque clés-valeurs : aucune règle allow. */
    rc = cap_kv_put("cle", 3, "valeur", 6);
    (void)rc;

    LOG("hostile : quatre tentatives, toutes refusees", NIVEAU_INFO);
    return 0;
}