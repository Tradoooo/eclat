// SPDX-License-Identifier: Apache-2.0
/* Éclat — intégration WAMR : instanciation, résolution des imports, appels, budget.
 * ECL-04 : un module .wasm s'exécute et appelle les imports de référence.
 * ECL-05 : le module hostile reçoit CAP_DENIED_POLICY et le refus est journalisé.
 * ECL-06 : le budget est respecté (refus synchrone ici ; mise à mort du processus par le
 *          superviseur, voir target/eclat_run.c).
 * ECL-07 : l'empreinte est vérifiée avant chargement.
 *
 * Chaque import du module devient une décision du moteur `core/` : une seule implémentation de
 * la politique, jamais de logique recopiée ici. Aucun thread n'est créé : la couche reste
 * portable telle quelle vers QNX. */
#define _POSIX_C_SOURCE 200809L

#include "eclat_runtime.h"

#include <stdio.h>
#include <string.h>

#include "eclat/cap_log.h"
#include "eclat/cap_sha256.h"
#include "eclat_abilities.h"
#include "wasm_export.h"

#define WASM_MODULE_NAME "env"
#define STACK_SIZE (64 * 1024)
#define HEAP_SIZE (16 * 1024)
#define MAX_LOG 1024
#define NS_PAR_MS 1000000ULL

/* Contexte d'exécution, passé aux imports natifs via NativeSymbol.attachment. */
typedef struct eclat_ctx {
    const eclat_policy_t *policy;   /* pour l'empreinte attendue et les budgets */
    const eclat_transport_t *transport; /* qui décide : local ou courtier distant */
    const eclat_platform_t *plat;
    eclat_budget_t budget;
    wasm_module_inst_t inst;
    uint64_t start_ns;
    uint32_t calls;
    uint32_t denials;
    uint64_t seq;
    int killed;
} eclat_ctx_t;

/* --- Budget (ECL-06) ------------------------------------------------------ */

/* Le quota d'appels et le budget CPU sont vérifiés de façon SYNCHRONE à chaque import : aucun
 * thread, aucune préemption. Une boucle de calcul pure qui n'appelle aucun import ne peut pas
 * être arrêtée d'ici — c'est le superviseur de processus (target/eclat_run.c) qui la tue. Sur
 * QNX ces deux niveaux correspondent au courtier (refus synchrone) et au HAM (mise à mort). */
static eclat_decision_t charge_appel(eclat_ctx_t *c) {
    uint64_t now = c->plat->monotonic_ns();
    if (eclat_budget_charge(&c->budget, now, 0, 1) == 2) {
        return ECLAT_CAP_DENIED_BUDGET;
    }
    uint64_t budget_ns = (uint64_t)c->budget.cpu_budget_ms_per_s * NS_PAR_MS;
    if (budget_ns != 0 && now - c->start_ns >= budget_ns) {
        c->killed = 1;
        return ECLAT_CAP_KILLED_CPU;
    }
    return ECLAT_CAP_OK;
}

/* Journalise une décision refusée au format de CONTRACT.md §5, via la couche plateforme. */
static void journaliser(eclat_ctx_t *c, eclat_cap_t cap, eclat_decision_t d) {
    char buf[256];
    int n = eclat_log_format_decision(buf, sizeof(buf), ++c->seq, c->policy->name,
                                      c->policy->name_len, cap, d);
    if (n > 0) {
        c->plat->emit(ECLAT_PLAT_LOG_WARN, buf, (size_t)n);
    }
}

/* Décide puis journalise. `payload` reçoit la valeur de retour (ex. l'échantillon du capteur).
 * La décision vient du transport : localement, ou du courtier dans un autre processus. */
static int32_t decider(eclat_ctx_t *c, eclat_cap_t cap, int32_t id, int32_t valeur,
                       int32_t *payload) {
    c->calls++;
    eclat_decision_t budget = charge_appel(c);
    if (budget != ECLAT_CAP_OK) {
        c->denials++;
        c->killed = (budget == ECLAT_CAP_KILLED_CPU) ? 1 : c->killed;
        journaliser(c, cap, budget);
        return (int32_t)budget;
    }
    eclat_transport_reply_t rep;
    memset(&rep, 0, sizeof(rep));
    if (c->transport == NULL || c->transport->decide == NULL ||
        c->transport->decide(c->transport->ctx, cap, id, valeur, &rep) != 0) {
        c->denials++;
        journaliser(c, cap, ECLAT_CAP_BAD_ARG);
        return (int32_t)ECLAT_CAP_BAD_ARG;
    }
    if (payload != NULL) {
        *payload = rep.payload;
    }
    eclat_decision_t d = (eclat_decision_t)rep.decision;
    if (d != ECLAT_CAP_OK) {
        c->denials++;
        journaliser(c, cap, d);
    }
    return (int32_t)d;
}

/* --- Imports natifs (CONTRACT.md §1) -------------------------------------- */

static int64_t native_time_ns(wasm_exec_env_t env) {
    eclat_ctx_t *c = wasm_runtime_get_function_attachment(env);
    decider(c, ECLAT_CAP_TIME_NS, -1, 0, NULL);
    return (int64_t)c->plat->monotonic_ns();
}

static int32_t native_log(wasm_exec_env_t env, char *msg, uint32_t len, int32_t level) {
    eclat_ctx_t *c = wasm_runtime_get_function_attachment(env);
    /* Longueur vérifiée AVANT tout usage (contrat). WAMR a déjà borné le pointeur. */
    if (msg == NULL || len > MAX_LOG) {
        return (int32_t)ECLAT_CAP_BAD_ARG;
    }
    int32_t rc = decider(c, ECLAT_CAP_LOG, level, 0, NULL);
    if (rc == (int32_t)ECLAT_CAP_OK) {
        c->plat->emit(level, msg, len);
    }
    return rc;
}

static int32_t native_read_sensor(wasm_exec_env_t env, int32_t id, void *out, uint32_t out_len) {
    eclat_ctx_t *c = wasm_runtime_get_function_attachment(env);
    if (out == NULL || out_len < sizeof(int32_t)) {
        return (int32_t)ECLAT_CAP_BAD_ARG;
    }
    /* La valeur vient du transport : du courtier si le module tourne contre le courtier QNX,
     * du capteur simulé local sinon. Elle est en milli-unités : rejeu bit à bit possible. */
    int32_t echantillon = 0;
    int32_t rc = decider(c, ECLAT_CAP_READ_SENSOR, id, 0, &echantillon);
    if (rc != (int32_t)ECLAT_CAP_OK) {
        return rc;
    }
    memcpy(out, &echantillon, sizeof(echantillon));
    return (int32_t)ECLAT_CAP_OK;
}

static int32_t native_write_actuator(wasm_exec_env_t env, int32_t id, int32_t valeur) {
    eclat_ctx_t *c = wasm_runtime_get_function_attachment(env);
    return decider(c, ECLAT_CAP_WRITE_ACTUATOR, id, valeur, NULL);
}

static int32_t native_kv(wasm_exec_env_t env, eclat_cap_t cap, void *key, uint32_t key_len,
                         void *val, uint32_t val_len) {
    eclat_ctx_t *c = wasm_runtime_get_function_attachment(env);
    if (key == NULL || key_len == 0 || val == NULL || val_len == 0) {
        return (int32_t)ECLAT_CAP_BAD_ARG;
    }
    int32_t rc = decider(c, cap, -1, 0, NULL);
    if (rc == (int32_t)ECLAT_CAP_OK && cap == ECLAT_CAP_KV_GET) {
        memset(val, 0, val_len); /* banque vide : le contrat borne la copie, pas le contenu */
    }
    return rc;
}

static int32_t native_kv_get(wasm_exec_env_t env, void *k, uint32_t kl, void *v, uint32_t vl) {
    return native_kv(env, ECLAT_CAP_KV_GET, k, kl, v, vl);
}

static int32_t native_kv_put(wasm_exec_env_t env, void *k, uint32_t kl, void *v, uint32_t vl) {
    return native_kv(env, ECLAT_CAP_KV_PUT, k, kl, v, vl);
}

static int32_t native_net_send(wasm_exec_env_t env, int32_t chan, void *buf, uint32_t len) {
    eclat_ctx_t *c = wasm_runtime_get_function_attachment(env);
    if (buf == NULL && len != 0) {
        return (int32_t)ECLAT_CAP_BAD_ARG;
    }
    return decider(c, ECLAT_CAP_NET_SEND, chan, (int32_t)len, NULL);
}

/* Signatures WAMR : « * » = pointeur borné par le runtime, « ~ » = sa longueur (i32),
 * « i » = i32, « I » = i64. exec_env est toujours le premier paramètre C, hors signature. */
static NativeSymbol NATIVES[] = {
    {"cap_time_ns", (void *)native_time_ns, "()I", NULL},
    {"cap_log", (void *)native_log, "(*~i)i", NULL},
    {"cap_read_sensor", (void *)native_read_sensor, "(i*~)i", NULL},
    {"cap_write_actuator", (void *)native_write_actuator, "(ii)i", NULL},
    {"cap_kv_get", (void *)native_kv_get, "(*~*~)i", NULL},
    {"cap_kv_put", (void *)native_kv_put, "(*~*~)i", NULL},
    {"cap_net_send", (void *)native_net_send, "(i*~)i", NULL},
};
#define N_NATIVES (sizeof(NATIVES) / sizeof(NATIVES[0]))

/* --- Empreinte du module (ECL-07) ----------------------------------------- */

static int verifier_empreinte(const uint8_t *wasm, size_t len, const eclat_policy_t *policy,
                              char *err, size_t errlen) {
    uint8_t digest[ECLAT_SHA256_DIGEST_LEN];
    eclat_sha256(wasm, len, digest);
    if (eclat_sha256_equal(digest, policy->sha256) != 1) {
        char obtenu[ECLAT_SHA256_HEX_LEN + 1];
        eclat_sha256_hex(digest, obtenu);
        snprintf(err, errlen, "empreinte du module différente de la politique (%s...)", obtenu);
        return ECLAT_RUN_ERR_DIGEST;
    }
    return ECLAT_RUN_OK;
}

/* --- Chargement ----------------------------------------------------------- */

static int charger(eclat_ctx_t *c, const uint8_t *buf, uint32_t size, wasm_module_t *module,
                   wasm_exec_env_t *env, char *err, size_t errlen) {
    *module = wasm_runtime_load((uint8_t *)buf, size, err, (uint32_t)errlen);
    if (*module == NULL) {
        return ECLAT_RUN_ERR_LOAD;
    }
    c->inst = wasm_runtime_instantiate(*module, STACK_SIZE, HEAP_SIZE, err, (uint32_t)errlen);
    if (c->inst == NULL) {
        wasm_runtime_unload(*module);
        *module = NULL;
        return ECLAT_RUN_ERR_LOAD;
    }
    *env = wasm_runtime_create_exec_env(c->inst, STACK_SIZE);
    if (*env == NULL) {
        snprintf(err, errlen, "impossible de créer l'environnement d'exécution");
        return ECLAT_RUN_ERR_RUNTIME;
    }
    return ECLAT_RUN_OK;
}

/* Attache le contexte à chaque import natif puis enregistre la table. Contrainte assumée : un
 * seul module à la fois — l'isolation multi-modules (ECL-19) est hors périmètre. */
static int enregistrer_imports(eclat_ctx_t *c, char *err, size_t errlen) {
    for (size_t i = 0; i < N_NATIVES; i++) {
        NATIVES[i].attachment = c;
    }
    if (!wasm_runtime_register_natives(WASM_MODULE_NAME, NATIVES, (uint32_t)N_NATIVES)) {
        snprintf(err, errlen, "échec de l'enregistrement des imports natifs");
        return ECLAT_RUN_ERR_RUNTIME;
    }
    return ECLAT_RUN_OK;
}

static void nettoyer(wasm_module_t module, wasm_exec_env_t env, eclat_ctx_t *c) {
    if (env != NULL) {
        wasm_runtime_destroy_exec_env(env);
    }
    if (c->inst != NULL) {
        wasm_runtime_deinstantiate(c->inst);
        c->inst = NULL;
    }
    if (module != NULL) {
        wasm_runtime_unload(module);
    }
    wasm_runtime_destroy();
}

/* Exécute l'export « run » et renseigne le bilan mesuré. */
static int executer(eclat_ctx_t *c, wasm_exec_env_t env, eclat_run_result_t *out, char *err,
                    size_t errlen) {
    wasm_function_inst_t run = wasm_runtime_lookup_function(c->inst, "run");
    if (run == NULL) {
        snprintf(err, errlen, "export « run » absent du module");
        return ECLAT_RUN_ERR_NO_ENTRY;
    }
    c->start_ns = c->plat->monotonic_ns();
    uint64_t cpu0 = c->plat->cpu_ns();
    uint32_t argv[1] = {0};
    bool appele = wasm_runtime_call_wasm(env, run, 0, argv);
    out->wall_ns = c->plat->monotonic_ns() - c->start_ns;
    out->cpu_ns = c->plat->cpu_ns() - cpu0;
    out->calls = c->calls;
    out->denials = c->denials;
    out->killed = c->killed;
    if (!appele) {
        const char *exc = wasm_runtime_get_exception(c->inst);
        snprintf(err, errlen, "exception du runtime : %s", exc ? exc : "(inconnue)");
        /* Un module tué par le budget s'arrête par une exception : c'est le comportement voulu,
         * pas une erreur d'exécution. */
        return c->killed ? ECLAT_RUN_OK : ECLAT_RUN_ERR_RUNTIME;
    }
    out->module_rc = (int)argv[0];
    return ECLAT_RUN_OK;
}

/* --- Retrait ciblé des abilities (ECL-14, couche 3) -------------------------- */

/* Retire au runtime les privilèges dont il n'a pas besoin, et journalise ce qui reste.
 *
 * Quand l'on a ce choix, on retire l'ability AVANT que le module ne soit chargé : c'est le
 * module non certifié qui tourne dans ce processus, pas le runtime qui le charge. Le runtime
 * garde ce dont il a besoin pour la suite — lire, se connecter au courtier, et (pour le
 * superviseur) créer un processus.
 *
 * La règle est MESURÉE, pas supposée : après le retrait, on tente une page inscriptible ET
 * exécutable. Si le noyau l'accorde encore, le retrait n'a pas été effectif et on refuse de
 * charger du code non certifié. Un refus du noyau sur une ability que le processus ne
 * détenait pas (typiquement sous une politique secpol, où le type retire déjà tout) n'est pas
 * un échec : c'est journalisé, pas bloquant.
 *
 * Sur POSIX il n'y a pas d'ability : la couche renvoie « rien à retirer » et le runtime
 * continue. */
static int retirer_abilities_ciblees(const eclat_platform_t *plat, char *err, size_t errlen) {
    size_t n = eclat_abilities_ciblees_count();
    if (n == 0) {
        return ECLAT_RUN_OK;
    }
    eclat_abilities_bilan_t bilan;
    char detail[192];
    (void)eclat_abilities_drop_runtime(&bilan, detail, sizeof(detail));
    const eclat_ability_ciblee_t *liste = eclat_abilities_ciblees();
    for (size_t i = 0; i < n; i++) {
        char ligne[256];
        int k = snprintf(ligne, sizeof(ligne), "abilities retirée : %s — %s", liste[i].nom,
                         liste[i].raison);
        if (k > 0 && plat != NULL && plat->emit != NULL) {
            plat->emit(ECLAT_PLAT_LOG_INFO, ligne, (size_t)k);
        }
    }
    if (bilan.refusees > 0) {
        char ligne[256];
        int k = snprintf(ligne, sizeof(ligne),
                         "abilities refusées par le noyau : %zu (première : %s, errno %d)",
                         bilan.refusees, bilan.premier_refus ? bilan.premier_refus : "?",
                         bilan.premier_errno);
        if (k > 0 && plat != NULL && plat->emit != NULL) {
            plat->emit(ECLAT_PLAT_LOG_WARN, ligne, (size_t)k);
        }
    }
    /* Le seul test qui compte : le privilège dangereux est-il encore accessible ? */
    if (eclat_abilities_wx_encore_possible()) {
        snprintf(err, errlen,
                 "le retrait des abilities n'a pas pris : une page inscriptible ET exécutable "
                 "reste accessible — aucun module n'est chargé");
        return ECLAT_RUN_ERR_RUNTIME;
    }
    return ECLAT_RUN_OK;
}

int eclat_runtime_run(const uint8_t *wasm, size_t len, const eclat_policy_t *policy,
                      const eclat_transport_t *transport, const eclat_platform_t *plat,
                      eclat_run_result_t *out, char *err, size_t errlen) {
    if (wasm == NULL || policy == NULL || transport == NULL || plat == NULL || out == NULL ||
        err == NULL) {
        return ECLAT_RUN_ERR_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->module_rc = -1;
    err[0] = '\0';

    int rc = verifier_empreinte(wasm, len, policy, err, errlen);
    if (rc != ECLAT_RUN_OK) {
        return rc;
    }

    eclat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.policy = policy;
    ctx.transport = transport;
    ctx.plat = plat;
    eclat_budget_init(&ctx.budget, policy->cpu_budget_ms_per_s, policy->calls_per_s);

    if (!wasm_runtime_init()) {
        snprintf(err, errlen, "échec de l'initialisation du runtime");
        return ECLAT_RUN_ERR_RUNTIME;
    }
    rc = enregistrer_imports(&ctx, err, errlen);
    if (rc == ECLAT_RUN_OK) {
        wasm_module_t module = NULL;
        wasm_exec_env_t env = NULL;
        /* Le retrait des abilities précède le CHARGEMENT du module : c'est à ce moment-là
         * qu'il faut pouvoir dire « le module a été chargé sans les privilèges inutiles ».
         * Si le retrait n'a pas pris, on ne charge rien du tout (voir le gate plus bas). */
        rc = retirer_abilities_ciblees(plat, err, errlen);
        if (rc == ECLAT_RUN_OK) {
            rc = charger(&ctx, wasm, (uint32_t)len, &module, &env, err, errlen);
        }
        if (rc == ECLAT_RUN_OK) {
            rc = executer(&ctx, env, out, err, errlen);
        }
        nettoyer(module, env, &ctx);
    } else {
        wasm_runtime_destroy();
    }
    return rc;
}