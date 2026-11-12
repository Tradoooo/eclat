// SPDX-License-Identifier: Apache-2.0
/* Éclat — chargeur de module : `eclat-run <politique.toml> <module.wasm>`.
 * Outil HÔTE : entrées/sorties fichier, allocation et processus sont permis ici (ce n'est pas
 * core/). Aucune règle métier : la décision vient de core/, le temps de la couche plateforme.
 *
 * ECL-06, deuxième niveau : un module qui boucle sans jamais appeler le courtier ne peut pas
 * être arrêté de l'intérieur du runtime. Le chargeur l'exécute donc dans un **processus enfant**
 * surveillé, et le tue à l'échéance du budget. C'est exactement l'architecture visée sur QNX :
 * le courtier refuse de façon synchrone, le gestionnaire de processus (HAM) met à mort. */
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

/* Exécution directe, sans surveillance : utilisée quand aucun budget CPU n'est posé. */
static int executer_direct(const eclat_policy_t *policy, const uint8_t *wasm, size_t len,
                           eclat_run_result_t *res) {
    char err[512];
    int rc = eclat_runtime_run(wasm, len, policy, eclat_platform_get(), res, err, sizeof(err));
    if (rc != ECLAT_RUN_OK) {
        fprintf(stderr, "REFUSÉ (%d) : %s\n", rc, err);
        return 1;
    }
    return 0;
}

/* Bilan transmis de l'enfant au parent : structure plate, écrite d'un bloc. */
typedef struct {
    eclat_run_result_t res;
    int rc;
} transmission_t;

static void enfant(int ecrivure, const eclat_policy_t *policy, const uint8_t *wasm, size_t len) {
    transmission_t t;
    memset(&t, 0, sizeof(t));
    t.rc = executer_direct(policy, wasm, len, &t.res);
    ssize_t n = write(ecrivure, &t, sizeof(t));
    (void)n;
    _exit(0);
}

/* Surveille l'enfant et le tue à l'échéance du budget CPU (ECL-06). */
static int superviser(const eclat_policy_t *policy, const uint8_t *wasm, size_t len,
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
        enfant(tubes[1], policy, wasm, len);
        _exit(0); /* jamais atteint */
    }
    close(tubes[1]);

    int delai = (int)policy->cpu_budget_ms_per_s + MARGE_MS;
    struct pollfd pfd;
    pfd.fd = tubes[0];
    pfd.events = POLLIN;
    pfd.revents = 0;
    int pret = poll(&pfd, 1, delai);

    transmission_t t;
    memset(&t, 0, sizeof(t));
    int tue = 0;
    if (pret > 0 && read(tubes[0], &t, sizeof(t)) == (ssize_t)sizeof(t)) {
        *res = t.res;
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
        printf("module tué par le superviseur : budget CPU de %u ms dépassé\n",
               policy->cpu_budget_ms_per_s);
    }
    return t.rc;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage : eclat-run <politique.toml> <module.wasm>\n");
        return 2;
    }
    uint8_t *pol = NULL;
    size_t pol_len = 0;
    if (lire_fichier(argv[1], &pol, &pol_len) != 0) {
        fprintf(stderr, "politique illisible : %s\n", argv[1]);
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
    if (lire_fichier(argv[2], &wasm, &wasm_len) != 0) {
        fprintf(stderr, "module illisible : %s\n", argv[2]);
        return 2;
    }

    printf("module « %s » (%zu octets) sous politique « %.*s »\n", argv[2], wasm_len,
           (int)policy.name_len, policy.name);

    eclat_run_result_t res;
    memset(&res, 0, sizeof(res));
    res.module_rc = -1;
    int rc = (policy.cpu_budget_ms_per_s > 0) ? superviser(&policy, wasm, wasm_len, &res)
                                             : executer_direct(&policy, wasm, wasm_len, &res);
    free(wasm);
    afficher_bilan(&res);
    return rc;
}