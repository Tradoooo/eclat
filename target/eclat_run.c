// SPDX-License-Identifier: Apache-2.0
/* Éclat — chargeur de module : `eclat-run <politique.toml> <module.wasm> [--broker <nom>]`.
 *
 * Outil HÔTE : entrées/sorties fichier, allocation et processus sont permis ici (ce n'est pas
 * core/). Aucune règle métier : la décision vient du transport, et l'empreinte attendue et les
 * budgets viennent de la politique.
 *
 * Deux modes de décision :
 *   -- sans --broker  la décision est prise dans ce processus (mode « courtier embarqué ») ;
 *   --broker <nom>    la décision est prise par le courtier QNX, dans un AUTRE processus, via
 *                     MsgSend/MsgReply. C'est le mode qui porte la promesse du projet.
 *
 * Deux modes d'exécution :
 *   -- budget CPU     le module tourne dans un processus ENFANT surveillé : au-delà du budget,
 *                     il est tué (SIGKILL). Un module qui boucle sans jamais appeler le courtier
 *                     ne peut pas être arrêté de l'intérieur du runtime — seul un autre
 *                     processus peut le faire. C'est le rôle du superviseur, et c'est ainsi que
 *                     fonctionne un gestionnaire de processus temps réel.
 */
#define _POSIX_C_SOURCE 200809L

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "eclat/cap_policy.h"
#include "eclat_platform.h"
#include "eclat_runtime.h"
#include "eclat_transport.h"

#ifdef __QNX__
#include "eclat_transport_qnx.h"
#endif

#define MAX_FICHIER (4u * 1024u * 1024u)
#define MARGE_MS 1000 /* marge laissée à l'enfant avant la mise à mort */

static int lire_fichier(const char *chemin, uint8_t **out, size_t *out_len) {
    FILE *f = fopen(chemin, "rb");
    if (f == NULL) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long taille = ftell(f);
    rewind(f);
    if (taille < 0 || (unsigned long)taille > MAX_FICHIER) {
        fclose(f);
        return -1;
    }
    uint8_t *buf = malloc((size_t)taille + 1);
    if (buf == NULL) {
        fclose(f);
        return -1;
    }
    size_t lu = fread(buf, 1, (size_t)taille, f);
    fclose(f);
    buf[lu] = '\0';
    *out = buf;
    *out_len = lu;
    return 0;
}

static void afficher_bilan(const eclat_run_result_t *r) {
    printf("\n--- bilan ---\n");
    printf("appels au courtier : %u\n", r->calls);
    printf("refus              : %u\n", r->denials);
    printf("cpu mesuré         : %llu ns\n", (unsigned long long)r->cpu_ns);
    printf("temps écoulé       : %llu ns\n", (unsigned long long)r->wall_ns);
    printf("tué par le budget  : %s\n", r->killed ? "OUI" : "non");
    printf("code du module     : %d\n", r->module_rc);
}

/* Exécution directe : le module tourne dans ce processus. */
static int executer(const uint8_t *wasm, size_t wasm_len, const eclat_policy_t *policy,
                    const eclat_transport_t *transport, eclat_run_result_t *res) {
    char err[512];
    int rc = eclat_runtime_run(wasm, wasm_len, policy, transport, eclat_platform_get(), res, err,
                               sizeof(err));
    if (rc != ECLAT_RUN_OK) {
        fprintf(stderr, "REFUSÉ (%d) : %s\n", rc, err);
        return 1;
    }
    return 0;
}

/* Exécution supervisée : le module tourne dans un processus enfant, tué au-delà du budget. */
static int superviser(const uint8_t *wasm, size_t wasm_len, const eclat_policy_t *policy,
                      const eclat_transport_t *transport, uint32_t budget_ms,
                      eclat_run_result_t *res) {
    int tubes[2];
    if (pipe(tubes) != 0) {
        perror("pipe");
        return 1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(tubes[0]);
        close(tubes[1]);
        return 1;
    }
    if (pid == 0) {
        close(tubes[0]);
        eclat_run_result_t local;
        memset(&local, 0, sizeof(local));
        local.module_rc = -1;
        executer(wasm, wasm_len, policy, transport, &local);
        ssize_t n = write(tubes[1], &local, sizeof(local));
        (void)n;
        _exit(0);
    }
    close(tubes[1]);

    struct pollfd pfd;
    pfd.fd = tubes[0];
    pfd.events = POLLIN;
    pfd.revents = 0;
    int pret = poll(&pfd, 1, (int)budget_ms + MARGE_MS);
    memset(res, 0, sizeof(*res));
    res->module_rc = -1;
    int tue = 0;
    if (pret > 0 && read(tubes[0], res, sizeof(*res)) == (ssize_t)sizeof(*res)) {
        /* le module a rendu la main avant l'échéance */
    } else {
        kill(pid, SIGKILL);
        tue = 1;
    }
    close(tubes[0]);
    int statut = 0;
    waitpid(pid, &statut, 0);
    if (tue) {
        res->killed = 1;
        res->module_rc = -1;
        printf("module tué par le superviseur : budget CPU de %u ms dépassé\n", budget_ms);
    }
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage : %s <politique.toml> <module.wasm> [--broker <nom>]\n", prog);
    fprintf(stderr, "  --broker <nom>  les décisions sont prises par le courtier QNX « nom »,\n");
    fprintf(stderr, "                  dans un autre processus (MsgSend/MsgReply).\n");
}

int main(int argc, char **argv) {
    const char *broker = NULL;
    const char *chemins[2] = {NULL, NULL};
    int n_chemins = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--broker") == 0 && i + 1 < argc) {
            broker = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (n_chemins < 2) {
            chemins[n_chemins++] = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (n_chemins != 2) {
        usage(argv[0]);
        return 2;
    }

    uint8_t *pol = NULL;
    size_t pol_len = 0;
    if (lire_fichier(chemins[0], &pol, &pol_len) != 0) {
        fprintf(stderr, "politique illisible : %s\n", chemins[0]);
        return 2;
    }
    eclat_policy_t policy;
    eclat_policy_error_t perr;
    memset(&policy, 0, sizeof(policy));
    memset(&perr, 0, sizeof(perr));
    if (eclat_policy_parse(pol, pol_len, &policy, &perr) != 0) {
        fprintf(stderr, "politique refusée : %s : %s\n", perr.field, perr.message);
        free(pol);
        return 2;
    }
    free(pol);

    uint8_t *wasm = NULL;
    size_t wasm_len = 0;
    if (lire_fichier(chemins[1], &wasm, &wasm_len) != 0) {
        fprintf(stderr, "module illisible : %s\n", chemins[1]);
        return 2;
    }

    /* Le transport : local par défaut, distant si le courtier est demandé. */
    eclat_transport_t transport;
    eclat_transport_local_ctx_t local_ctx;
    eclat_transport_local_init(&local_ctx, &transport, &policy);
#ifdef __QNX__
    eclat_transport_qnx_ctx_t qnx_ctx;
    if (broker != NULL) {
        if (eclat_transport_qnx_init(&qnx_ctx, &transport, broker) != 0) {
            free(wasm);
            return 2;
        }
        printf("eclat: décisions prises par le courtier « %s » (autre processus)\n", broker);
    }
#else
    if (broker != NULL) {
        fprintf(stderr,
                "KO : --broker n'est disponible que sur QNX (le courtier est un processus QNX)\n");
        free(wasm);
        return 2;
    }
#endif

    printf("module « %s » (%zu octets) sous politique « %.*s »\n", chemins[1], wasm_len,
           (int)policy.name_len, policy.name);

    eclat_run_result_t res;
    memset(&res, 0, sizeof(res));
    res.module_rc = -1;
    int rc;
    if (policy.cpu_budget_ms_per_s > 0) {
        rc = superviser(wasm, wasm_len, &policy, &transport, policy.cpu_budget_ms_per_s, &res);
    } else {
        rc = executer(wasm, wasm_len, &policy, &transport, &res);
    }
    free(wasm);
    afficher_bilan(&res);
    return rc;
}