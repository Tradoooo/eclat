// SPDX-License-Identifier: Apache-2.0
/* {{MODULE}} — harnais natif Linux.
 *
 * Fournit les imports du contrat en les faisant passer par le VRAI moteur de décision du
 * courtier (`eclat_decide`). Le module s'exécute donc sur Linux avec exactement la même
 * sémantique de politique que sur cible ; seule la transportation change (appel de fonction au
 * lieu d'un message synchrone QNX). Aucune licence QNX, aucun SDP, aucune cible nécessaire.
 *
 * C'est le même harnais que le courtier utilisera : une seule implémentation de la décision,
 * jamais de logique recopiée côté module. */
#define _POSIX_C_SOURCE 200809L

#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"
#include "module_imports.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_POLICY (64 * 1024)
#define MAX_LOG 4096

/* État du harnais (l'hôte), jamais du module. */
static eclat_policy_t g_policy;
static uint32_t g_seq;
static uint32_t g_refus;
static int32_t g_derniere_consigne;
static const int32_t g_capteur_id1 = 4200; /* milli-unités */

/* Fait passer un appel par le moteur de décision et journalise la décision. */
static eclat_decision_t demander(eclat_cap_t cap, int32_t id, int32_t valeur) {
    eclat_request_t req;
    memset(&req, 0, sizeof(req));
    req.cap = cap;
    req.id = id;
    req.value_milli = valeur;
    eclat_decision_t d = eclat_decide(&g_policy, &req);
    g_seq++;
    if (d != ECLAT_CAP_OK) {
        g_refus++;
    }
    printf("  [%2u] %-15s id=%-4d valeur=%-7d -> %s\n", g_seq, eclat_cap_name(cap), id, valeur,
           eclat_decision_name(d));
    return d;
}

int64_t cap_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int32_t cap_log(const void *msg, int32_t len, int32_t level) {
    /* Longueur vérifiée AVANT toute copie ou impression (CONTRACT.md §1). */
    if (msg == NULL || len < 0 || len > MAX_LOG) {
        return ECLAT_CAP_BAD_ARG;
    }
    /* cap_log est une facilité hôte (CONTRACT.md §3 étape 3) : pas de règle allow requise. */
    printf("  [log niv=%d] %.*s\n", level, (int)len, (const char *)msg);
    return ECLAT_CAP_OK;
}

int32_t cap_read_sensor(int32_t id, void *out, int32_t out_len) {
    if (out == NULL || out_len < (int32_t)sizeof(int32_t)) {
        return ECLAT_CAP_BAD_ARG;
    }
    eclat_decision_t d = demander(ECLAT_CAP_READ_SENSOR, id, 0);
    if (d != ECLAT_CAP_OK) {
        return (int32_t)d;
    }
    int32_t valeur = (id == 1) ? g_capteur_id1 : 0;
    memcpy(out, &valeur, sizeof(valeur));
    return ECLAT_CAP_OK;
}

int32_t cap_write_actuator(int32_t id, int32_t value_milli) {
    eclat_decision_t d = demander(ECLAT_CAP_WRITE_ACTUATOR, id, value_milli);
    if (d != ECLAT_CAP_OK) {
        return (int32_t)d;
    }
    g_derniere_consigne = value_milli;
    return ECLAT_CAP_OK;
}

int32_t cap_kv_get(const void *key, int32_t key_len, void *val, int32_t val_len) {
    if (key == NULL || key_len <= 0 || val == NULL || val_len <= 0) {
        return ECLAT_CAP_BAD_ARG;
    }
    eclat_decision_t d = demander(ECLAT_CAP_KV_GET, -1, 0);
    if (d != ECLAT_CAP_OK) {
        return (int32_t)d;
    }
    memset(val, 0, (size_t)val_len);
    return ECLAT_CAP_OK;
}

int32_t cap_kv_put(const void *key, int32_t key_len, const void *val, int32_t val_len) {
    if (key == NULL || key_len <= 0 || val == NULL || val_len <= 0) {
        return ECLAT_CAP_BAD_ARG;
    }
    return (int32_t)demander(ECLAT_CAP_KV_PUT, -1, 0);
}

int32_t cap_net_send(int32_t chan, const void *buf, int32_t len) {
    if (buf == NULL || len < 0) {
        return ECLAT_CAP_BAD_ARG;
    }
    return (int32_t)demander(ECLAT_CAP_NET_SEND, chan, 0);
}

static int charger_politique(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "politique illisible : %s\n", path);
        return 2;
    }
    static uint8_t buf[MAX_POLICY];
    size_t len = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    eclat_policy_error_t err;
    memset(&err, 0, sizeof(err));
    if (eclat_policy_parse(buf, len, &g_policy, &err) != 0) {
        fprintf(stderr, "politique refusée : %s : %s\n", err.field, err.message);
        return 2;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "policy.toml";
    memset(&g_policy, 0, sizeof(g_policy));
    if (charger_politique(path) != 0) {
        return 2;
    }
    printf("module {{MODULE}} — politique « %.*s »\n", (int)g_policy.name_len, g_policy.name);
    int rc = module_main();
    printf("\n%d appels au courtier, %d refus, code de retour du module = %d\n", g_seq, g_refus, rc);
    printf("dernière consigne appliquée : %d milli-unités\n", g_derniere_consigne);
    return 0;
}