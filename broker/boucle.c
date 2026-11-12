// SPDX-License-Identifier: Apache-2.0
/* Éclat — boucle de contrôle temps réel à 1 kHz, et mesure de sa gigue.
 *
 * À chaque tic (1 ms, réveil par impulsion QNX), la boucle lit le capteur VIA LE COURTIER
 * (c'est-à-dire : la valeur traverse la frontière de capacité), et mesure sa propre ponctualité.
 * À la fin, elle affiche un histogramme de gigue. C'est l'outil qui prouve la promesse du
 * projet : « la boucle ne bouge pas », ici mesurée, pas affirmée.
 *
 * Usage : boucle [nb_ticks]       (défaut : 2000 = 2 s)
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include "eclat_ipc.h"
#include "eclat/cap_decision.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <time.h>

#define NOMINAL_NS 1000000ULL /* 1 ms */
#define MAX_TICKS 100000

static int coid = -1;
static uint32_t seq = 0;

/* Une lecture du capteur au travers du courtier. Retourne la valeur, ou -1. */
static int32_t lire_capteur(int32_t *valeur) {
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
    if (valeur != NULL) {
        *valeur = rep.payload;
    }
    return 0;
}

static int cmp_u64(const void *a, const void *b) {
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    int nb_ticks = (argc > 1) ? atoi(argv[1]) : 2000;
    if (nb_ticks < 1) {
        nb_ticks = 1;
    }
    if (nb_ticks > MAX_TICKS) {
        nb_ticks = MAX_TICKS;
    }

    coid = name_open(ECLAT_IPC_NAME, 0);
    if (coid < 0) {
        fprintf(stderr, "boucle: courtier « %s » introuvable\n", ECLAT_IPC_NAME);
        return 2;
    }

    /* Canal pour les impulsions du timer : le réveil périodique de QNX. */
    int chid = ChannelCreate(0);
    int self = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    struct sigevent ev;
    memset(&ev, 0, sizeof(ev));
    ev.sigev_notify = SIGEV_PULSE;
    ev.sigev_coid = self;
    ev.sigev_priority = 21; /* priorité temps réel, au-dessus de la normale */
    ev.sigev_code = 1;

    timer_t timer;
    if (timer_create(CLOCK_MONOTONIC, &ev, &timer) != 0) {
        fprintf(stderr, "boucle: timer_create: %s\n", strerror(errno));
        return 2;
    }
    struct itimerspec spec;
    memset(&spec, 0, sizeof(spec));
    spec.it_value.tv_nsec = NOMINAL_NS;
    spec.it_interval.tv_nsec = NOMINAL_NS;
    if (timer_settime(timer, 0, &spec, NULL) != 0) {
        fprintf(stderr, "boucle: timer_settime: %s\n", strerror(errno));
        return 2;
    }

    int64_t *ecarts = malloc(sizeof(*ecarts) * (size_t)(nb_ticks > 0 ? nb_ticks - 1 : 0));
    if (ecarts == NULL) {
        return 2;
    }

    int n_mesures = 0;
    uint64_t precedent = 0;
    struct _pulse pulse;
    for (int i = 0; i < nb_ticks; i++) {
        int rcvid = MsgReceive(chid, &pulse, sizeof(pulse), NULL);
        if (rcvid != 0) {
            continue; /* un message inattendu : on l'ignore dans cette démo */
        }
        uint64_t maintenant;
        ClockTime(CLOCK_MONOTONIC, NULL, &maintenant);
        if (i > 0) {
            ecarts[n_mesures++] = (int64_t)(maintenant - precedent) - (int64_t)NOMINAL_NS;
        }
        precedent = maintenant;
        int32_t ech = 0;
        if (lire_capteur(&ech) != 0) {
            fprintf(stderr, "boucle: lecture du capteur via le courtier a échoué\n");
        }
    }
    timer_delete(timer);

    printf("boucle 1 kHz : %d tics, %d mesures de gigue\n", nb_ticks, n_mesures);
    if (n_mesures == 0) {
        return 0;
    }
    qsort(ecarts, (size_t)n_mesures, sizeof(*ecarts), cmp_u64);
    long long p50 = (long long)ecarts[n_mesures / 2];
    long long p99 = (long long)ecarts[n_mesures * 99 / 100];
    long long p999 = (long long)ecarts[n_mesures * 999 / 1000];
    long long mini = (long long)ecarts[0];
    long long maxi = (long long)ecarts[n_mesures - 1];
    /* comptage des échéances « ratées » : un écart > 500 µs au-delà du nominal, soit 1,5 ms. */
    uint32_t ratees = 0;
    for (int i = 0; i < n_mesures; i++) {
        if (ecarts[i] > 500000) {
            ratees++;
        }
    }
    printf("gigue (écart au nominal de 1 ms) : min=%lld ns  p50=%lld ns  p99=%lld ns  "
           "p99.9=%lld ns  max=%lld ns\n",
           mini, p50, p99, p999, maxi);
    printf("échéances au-delà de 1,5 ms : %u (%.3f %%)\n", ratees,
           (double)ratees * 100.0 / (double)n_mesures);

    free(ecarts);
    return 0;
}