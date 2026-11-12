// SPDX-License-Identifier: Apache-2.0
/* Éclat — transport de décision.
 *
 * Le moteur ne décide JAMAIS lui-même : il pose une question à un transport, qui répond. Deux
 * transports existent :
 *
 *   - local  : la décision est prise dans le processus, via `eclat_decide` de core/ ;
 *   - QNX    : la décision est prise par le courtier, dans un AUTRE processus, via un message
 *              synchrone (MsgSend/MsgReply) sur le nom « cap ».
 *
 * Le second est celui qui porte la promesse du projet : le module ne peut rien obtenir que la
 * politique n'accorde, et la politique n'est appliquée que d'un seul côté. Avec le transport
 * local, la frontière est une convention ; avec le transport QNX, elle est un processus.
 *
 * Ce fichier ne dépend que de <stdint.h> : il est compilé à l'identique partout. */
#ifndef ECLAT_TRANSPORT_H
#define ECLAT_TRANSPORT_H

#include <stdint.h>

#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Réponse d'un transport : code de décision et charge utile (ex. l'échantillon du capteur). */
typedef struct eclat_transport_reply {
    int32_t decision; /* code de décision (CAP_OK, CAP_DENIED_POLICY, ...) */
    int32_t payload;  /* valeur de retour, 0 si sans objet */
} eclat_transport_reply_t;

/* Un transport : une fonction qui décide, et son contexte. */
typedef struct eclat_transport {
    /* Décide d'une demande de capacité. Retourne 0 en cas de succès (reply renseigné),
     * -1 si le transport n'a pas pu répondre (alors la décision vaut CAP_BAD_ARG). */
    int32_t (*decide)(void *ctx, eclat_cap_t cap, int32_t id, int32_t value_milli,
                      eclat_transport_reply_t *reply);
    void *ctx;
} eclat_transport_t;

/* Transport local : la décision est prise dans le processus, par core/.
 * Portable : c'est celui utilisé sur l'hôte et quand aucun courtier n'est joignable. */
typedef struct eclat_transport_local_ctx {
    const eclat_policy_t *policy;
} eclat_transport_local_ctx_t;

void eclat_transport_local_init(eclat_transport_local_ctx_t *lc, eclat_transport_t *t,
                                const eclat_policy_t *policy);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_TRANSPORT_H */