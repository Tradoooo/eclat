// SPDX-License-Identifier: Apache-2.0
/* Éclat — transport local : la décision est prise dans le processus, par le moteur de core/.
 *
 * C'est le mode « courtier embarqué » : utile sur l'hôte, et utile comme repli si le courtier
 * QNX n'est pas joignable. Le mode qui porte la promesse du projet est le transport QNX
 * (platform/qnx/eclat_transport_qnx.c), où la décision est prise dans un autre processus.
 *
 * Aucune allocation, aucun état global mutable : le contexte est fourni par l'appelant. */
#include "eclat_transport.h"

#include "eclat/cap_policy.h"

#include <string.h>

/* Contexte du transport local : la politique, et rien d'autre. */
typedef struct {
    const eclat_policy_t *policy;
} local_ctx_t;

static int32_t local_decide(void *ctx, eclat_cap_t cap, int32_t id, int32_t value_milli,
                            eclat_transport_reply_t *reply) {
    local_ctx_t *lc = (local_ctx_t *)ctx;
    if (lc == NULL || lc->policy == NULL || reply == NULL) {
        return -1;
    }
    eclat_request_t req;
    memset(&req, 0, sizeof(req));
    req.cap = cap;
    req.id = id;
    req.value_milli = value_milli;
    reply->decision = (int32_t)eclat_decide(lc->policy, &req);
    reply->payload = 0;
    if (reply->decision == (int32_t)ECLAT_CAP_OK && cap == ECLAT_CAP_READ_SENSOR) {
        /* Capteur simulé, en milli-unités : même valeur que celle du courtier QNX, pour que
         * les deux modes soient directement comparables. */
        reply->payload = 4200;
    }
    return 0;
}

void eclat_transport_local_init(eclat_transport_local_ctx_t *lc, eclat_transport_t *t,
                                const eclat_policy_t *policy) {
    lc->policy = policy;
    t->decide = local_decide;
    t->ctx = lc;
}