// SPDX-License-Identifier: Apache-2.0
/* Éclat — « pouls » : un battement périodique, pour voir la cible en temps réel.
 *
 * À chaque battement : lit le capteur VIA LE COURTIER, relève l'uptime du noyau, et imprime une
 * ligne. Le sinus du capteur évolue et l'uptime avance : c'est la preuve, visible en continu,
 * que la chaîne complète court — pas une capture figée.
 *
 * Modes :
 *   pouls [intervalle_ms]        boucle infinie, sortie lisible
 *   pouls once                   un seul battement puis retour
 *   pouls json [intervalle_ms]   sortie JSON par battement (pour le tableau de bord web)
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include "eclat_ipc.h"
#include "eclat/cap_decision.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <time.h>

static int coid = -1;
static uint32_t seq = 0;

static int lire_capteur(int32_t *valeur) {
    eclat_ipc_request_t req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = ECLAT_IPC_TYPE;
    req.hdr.length = (uint32_t)(sizeof(req) - sizeof(req.hdr));
    req.op = ECLAT_OP_REQUEST;
    req.cap = ECLAT_CAP_READ_SENSOR;
    req.id = 1;
    req.seq = ++seq;
    eclat_ipc_reply_t rep;
    memset(&rep, 0, sizeof(rep));
    if (MsgSend(coid, &req, sizeof(req), &rep, sizeof(rep)) < 0) {
        return -1;
    }
    if (rep.decision != ECLAT_CAP_OK) {
        return -1;
    }
    *valeur = rep.payload;
    return 0;
}

int main(int argc, char **argv) {
    int une_fois = 0;
    int json = 0;
    long intervalle = 2000;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "once") == 0) {
            une_fois = 1;
        } else if (strcmp(argv[i], "json") == 0) {
            json = 1;
        } else {
            intervalle = atol(argv[i]);
        }
    }
    if (intervalle < 100) {
        intervalle = 100;
    }

    coid = open(ECLAT_IPC_PATH, O_RDWR);
    if (coid < 0) {
        coid = name_open(ECLAT_IPC_NAME, 0);
    }
    if (coid < 0) {
        fprintf(stderr, "pouls: courtier introuvable\n");
        return 2;
    }

    if (!json) {
        printf("pouls: l'horloge (uptime), le capteur (milli-unités) — un battement toutes les "
               "%ld ms, via le courtier\n",
               intervalle);
        fflush(stdout);
    }

    for (;;) {
        _Uint64t ns = 0;
        ClockTime(CLOCK_MONOTONIC, NULL, &ns);
        long long uptime_ms = (long long)(ns / 1000000ull);
        int32_t valeur = -1;
        int ok = lire_capteur(&valeur);
        if (json) {
            printf("{\"ok\":%d,\"uptime_ms\":%lld,\"sensor\":%d}\n", ok == 0, uptime_ms, valeur);
        } else if (ok != 0) {
            printf("pouls: +%lld ms  courtier injoignable\n", uptime_ms);
        } else {
            printf("pouls: +%lld ms  capteur=%d milli-unités (décidé par le courtier)\n",
                   uptime_ms, valeur);
        }
        fflush(stdout);
        if (une_fois) {
            break;
        }
        struct timespec d;
        d.tv_sec = intervalle / 1000;
        d.tv_nsec = (intervalle % 1000) * 1000000L;
        nanosleep(&d, NULL);
    }
    return 0;
}