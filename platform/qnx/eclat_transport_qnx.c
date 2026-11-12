// SPDX-License-Identifier: Apache-2.0
/* Éclat — transport QNX : la décision est prise par le courtier, dans un autre processus.
 *
 * Chaque import du module devient un MsgSend vers le courtier, qui répond par un code de
 * décision. Le module ne voit jamais la politique : il ne voit que des verdicts.
 *
 * QNX uniquement : ce fichier n'est compilé que pour la cible. */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include "eclat_transport_qnx.h"

#include "eclat_ipc.h"

#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

static int32_t qnx_decide(void *ctx, eclat_cap_t cap, int32_t id, int32_t value_milli,
                          eclat_transport_reply_t *reply) {
    eclat_transport_qnx_ctx_t *qc = (eclat_transport_qnx_ctx_t *)ctx;
    if (qc == NULL || reply == NULL || qc->coid < 0) {
        return -1;
    }
    eclat_ipc_request_t req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = ECLAT_IPC_TYPE;
    req.hdr.length = (uint32_t)(sizeof(req) - sizeof(req.hdr));
    req.op = ECLAT_OP_REQUEST;
    req.cap = (uint32_t)cap;
    req.id = id;
    req.value_milli = value_milli;
    req.seq = ++qc->seq;

    eclat_ipc_reply_t rep;
    memset(&rep, 0, sizeof(rep));
    if (MsgSend(qc->coid, &req, sizeof(req), &rep, sizeof(rep)) < 0) {
        fprintf(stderr, "eclat: MsgSend vers le courtier a échoué\n");
        return -1;
    }
    reply->decision = rep.decision;
    reply->payload = rep.payload;
    return 0;
}

int eclat_transport_qnx_init(eclat_transport_qnx_ctx_t *qc, eclat_transport_t *t,
                              const char *nom) {
    qc->coid = name_open(nom, 0);
    if (qc->coid < 0) {
        fprintf(stderr, "eclat: courtier « %s » introuvable\n", nom);
        return -1;
    }
    qc->seq = 0;
    t->decide = qnx_decide;
    t->ctx = qc;
    return 0;
}