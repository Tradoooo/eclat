// SPDX-License-Identifier: Apache-2.0
/* Éclat — harnais différentiel (CONTRACT.md §6). Outil HÔTE : les I/O fichiers sont
 * permises ici (ce n'est pas core/).
 * Usage : eclat-decide <policy.toml> <requests.txt>
 * Sortie : une ligne par requête, le nom du code de décision (eclat_decision_name).
 * Politique invalide : stderr "ERROR:<field>:<message>" et code de sortie 2.
 */
#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lit un fichier entier dans un tampon alloué (outil hôte). Retourne 0 si succès. */
static int read_file(const char *path, char **out, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    char *buf = malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(f);
        return -1;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    *out = buf;
    *out_len = rd;
    return 0;
}

/* CONTRACT.md §6 : politique invalide → stderr "ERROR:<field>:<message>", exit 2. */
static int load_policy(const char *path, eclat_policy_t *policy) {
    char *data = NULL;
    size_t len = 0;
    if (read_file(path, &data, &len) != 0) {
        fprintf(stderr, "ERROR:policy:cannot read file %s\n", path);
        return 2;
    }
    eclat_policy_error_t err;
    memset(&err, 0, sizeof(err));
    int rc = eclat_policy_parse((const uint8_t *)data, len, policy, &err);
    free(data);
    if (rc != 0) {
        fprintf(stderr, "ERROR:%s:%s\n", err.field, err.message);
        return 2;
    }
    return 0;
}

/* CONTRACT.md §6 : champ entier décimal, ou "-" si sans objet.
 * "-" pour id → -1 ; "-" pour value_milli → 0. */
static int32_t parse_field(const char *tok, int32_t absent_value) {
    if (tok == NULL || strcmp(tok, "-") == 0) {
        return absent_value;
    }
    return (int32_t)strtol(tok, NULL, 10);
}

/* Traite une ligne de requête : <cap_name> <id> <value_milli> (CONTRACT.md §6). */
static void handle_request_line(const eclat_policy_t *policy, char *line) {
    char cap_tok[64] = "";
    char id_tok[32] = "";
    char val_tok[32] = "";
    int n = sscanf(line, "%63s %31s %31s", cap_tok, id_tok, val_tok);
    if (n < 1) {
        return; /* ligne vide : ignorée */
    }
    eclat_request_t req;
    memset(&req, 0, sizeof(req));
    eclat_cap_t cap;
    if (eclat_cap_from_name(cap_tok, strlen(cap_tok), &cap) != 0) {
        req.cap = ECLAT_CAP_COUNT; /* hors plage → CAP_DENIED_UNKNOWN (§3 étape 1) */
    } else {
        req.cap = cap;
    }
    req.id = parse_field(id_tok, -1);
    req.value_milli = parse_field(val_tok, 0);
    eclat_decision_t d = eclat_decide(policy, &req);
    printf("%s\n", eclat_decision_name(d));
}

/* Lit requests.txt ligne par ligne et imprime une décision par ligne. */
static int run_requests(const eclat_policy_t *policy, const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "ERROR:requests:cannot read file %s\n", path);
        return 2;
    }
    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        handle_request_line(policy, line);
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: eclat-decide <policy.toml> <requests.txt>\n");
        return 2;
    }
    eclat_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    int rc = load_policy(argv[1], &policy);
    if (rc != 0) {
        return rc;
    }
    return run_requests(&policy, argv[2]);
}
