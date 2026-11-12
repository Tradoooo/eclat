// SPDX-License-Identifier: Apache-2.0
/* Éclat — client de test du courtier : il pose les mêmes questions qu'un module et affiche
 * ce que le courtier répond. C'est l'outil qui produit la preuve de l'étape « la politique
 * s'applique bien à travers la frontière IPC ».
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include "eclat_ipc.h"

#include "eclat/cap_decision.h"

#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h> /* name_open */
#include <sys/neutrino.h>
#include <unistd.h>

static int coid = -1;
static uint32_t seq = 0;

/* Envoie une requête et affiche la réponse. Retourne la décision. */
static int32_t demander(const char *libelle, uint32_t op, uint32_t cap, int32_t id,
                         int32_t valeur) {
    eclat_ipc_request_t req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = ECLAT_IPC_TYPE;
    req.hdr.length = (uint32_t)(sizeof(req) - sizeof(req.hdr));
    req.op = op;
    req.cap = cap;
    req.id = id;
    req.value_milli = valeur;
    req.seq = ++seq;

    eclat_ipc_reply_t rep;
    memset(&rep, 0, sizeof(rep));
    if (MsgSend(coid, &req, sizeof(req), &rep, sizeof(rep)) < 0) {
        printf("  %-34s ÉCHEC D'ENVOI\n", libelle);
        return -1;
    }
    printf("  %-34s -> %-18s (payload=%d, seq=%u)\n", libelle,
           eclat_decision_name((eclat_decision_t)rep.decision), rep.payload, rep.seq);
    return rep.decision;
}

int main(void) {
    /* name_open donne le canal du courtier : un « coid » utilisable avec MsgSend. */
    coid = name_open(ECLAT_IPC_NAME, 0);
    if (coid < 0) {
        printf("client: ouverture de %s impossible\n", ECLAT_IPC_NAME);
        return 2;
    }
    printf("client: connecté à %s\n", ECLAT_IPC_NAME);

    printf("— vivacité —\n");
    demander("ping", ECLAT_OP_PING, 0, -1, 0);

    printf("— demandes autorisées —\n");
    demander("cap_read_sensor(id=1)", ECLAT_OP_REQUEST, ECLAT_CAP_READ_SENSOR, 1, 0);

    printf("— demandes refusées —\n");
    demander("cap_net_send(chan=9)", ECLAT_OP_REQUEST, ECLAT_CAP_NET_SEND, 9, 0);
    demander("cap_write_actuator(99999)", ECLAT_OP_REQUEST, ECLAT_CAP_WRITE_ACTUATOR, 3, 99999);
    demander("cap_read_sensor(id=42)", ECLAT_OP_REQUEST, ECLAT_CAP_READ_SENSOR, 42, 0);
    demander("cap_kv_put", ECLAT_OP_REQUEST, ECLAT_CAP_KV_PUT, -1, 0);

    name_close(coid);
    printf("client: terminé\n");
    return 0;
}