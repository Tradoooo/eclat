// SPDX-License-Identifier: Apache-2.0
/* Éclat — en-tête du moteur de décision (core, C11 pur).
 * ECL-01 : le moteur compile en C11 strict, sans QNX ni WAMR.
 * ECL-05 : le module hostile reçoit CAP_DENIED_POLICY et le refus est journalisé.
 * Ce fichier ne connaît ni QNX, ni WAMR, ni horloge, ni allocation.
 */
#ifndef ECLAT_CAP_DECISION_H
#define ECLAT_CAP_DECISION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Capacités importées (CONTRACT.md §1). L'ordre des énumérés est gelé. */
typedef enum eclat_cap {
    ECLAT_CAP_TIME_NS = 0,
    ECLAT_CAP_LOG = 1,
    ECLAT_CAP_READ_SENSOR = 2,
    ECLAT_CAP_WRITE_ACTUATOR = 3,
    ECLAT_CAP_KV_GET = 4,
    ECLAT_CAP_KV_PUT = 5,
    ECLAT_CAP_NET_SEND = 6,
    ECLAT_CAP_COUNT = 7
} eclat_cap_t;

/* Codes de décision (CONTRACT.md §3). L'ordre des énumérés est gelé. */
typedef enum eclat_decision {
    ECLAT_CAP_OK = 0,
    ECLAT_CAP_DENIED_POLICY = 1,
    ECLAT_CAP_DENIED_RANGE = 2,
    ECLAT_CAP_DENIED_BUDGET = 3,
    ECLAT_CAP_DENIED_UNKNOWN = 4,
    ECLAT_CAP_BAD_ARG = 5,
    ECLAT_CAP_KILLED_CPU = 6,
    ECLAT_CAP_KILLED_MEM = 7
} eclat_decision_t;

/* Demande de décision : l'unité d'entrée de eclat_decide.
 * `id` est l'id capteur/actionneur, le canal réseau ou le namespace kv ; -1 si sans objet.
 * `value_milli` n'est utilisé que pour WRITE_ACTUATOR.
 * `payload_len` est la longueur utile (log/kv/net) : l'hôte l'a déjà bornée.
 */
typedef struct eclat_request {
    eclat_cap_t cap;
    int32_t id;
    int32_t value_milli;
    uint32_t payload_len;
} eclat_request_t;

struct eclat_policy; /* défini dans cap_policy.h */

/* Nom lisible d'un code de décision (chaîne statique, jamais NULL). */
const char *eclat_decision_name(eclat_decision_t d);

/* Nom lisible d'une capacité (chaîne statique, jamais NULL). */
const char *eclat_cap_name(eclat_cap_t c);

/* Résout le nom textuel d'une capacité. Retourne 0 si trouvé, -1 sinon. */
int eclat_cap_from_name(const char *name, size_t len, eclat_cap_t *out);

/* Décision déterministe, ordre gelé (CONTRACT.md §3). Ne produit que
 * CAP_OK / CAP_DENIED_POLICY / CAP_DENIED_RANGE / CAP_DENIED_UNKNOWN. */
eclat_decision_t eclat_decide(const struct eclat_policy *policy, const eclat_request_t *req);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_CAP_DECISION_H */