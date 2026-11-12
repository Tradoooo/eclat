// SPDX-License-Identifier: Apache-2.0
/* Éclat — module de démonstration HONNÊTE (ECL-04).
 * Ne demande que ce que la politique autorise : un capteur dans la plage permise, un actionneur
 * dans la plage permise, et du journal. Toutes ses requêtes doivent être acceptées. */
#include "imports.h"

__attribute__((export_name("run"))) int run(void) {
    LOG("honnete : demarrage", NIVEAU_INFO);

    int32_t echantillon = 0;
    int32_t rc = cap_read_sensor(1, &echantillon, (int32_t)sizeof(echantillon));
    if (rc != 0) {
        LOG("honnete : lecture capteur refusee", NIVEAU_ALERTE);
        return rc;
    }

    /* Consigne volontairement petite : 4200 / 8 = 525 milli-unités, dans [-1000, 1000]. */
    rc = cap_write_actuator(3, echantillon / 8);
    if (rc != 0) {
        LOG("honnete : consigne refusee", NIVEAU_ALERTE);
        return rc;
    }

    LOG("honnete : termine, consigne appliquee", NIVEAU_INFO);
    return 0;
}