// SPDX-License-Identifier: Apache-2.0
/* Éclat — politique de capacités (core, C11 pur : ni allocation, ni horloge, ni fichier, ni
 * socket, ni état global mutable).
 * ECL-02 : la politique est analysée et validée ; politique invalide refusée avec champ et
 * cause ; tout refus par défaut.
 * Le parseur consomme un tampon d'octets fourni par l'appelant et remplit une structure
 * à tableaux de taille fixe. Voir CONTRACT.md §2 pour le sous-ensemble TOML accepté. */
#include "eclat/cap_policy.h"
#include "eclat/cap_sha256.h"
#include "toml_internal.h"

#include <string.h>

#define IDS_MAX 64

static void set_err(eclat_policy_error_t *err, int lineno, const char *field, const char *msg) {
    if (err == NULL) {
        return;
    }
    err->lineno = lineno;
    strncpy(err->field, field, sizeof(err->field) - 1);
    err->field[sizeof(err->field) - 1] = '\0';
    strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = '\0';
}

/* sha256 : chaîne de exactement 64 hex → 32 octets. Retourne 0 si valide.
 * Le décodage hexadécimal n'est implémenté qu'une seule fois, dans cap_sha256.h. */
static int parse_sha256(eclat_toml_view v, uint8_t out[ECLAT_SHA256_DIGEST_LEN]) {
    char buf[ECLAT_SHA256_HEX_LEN + 1];
    size_t blen = 0;
    if (eclat_toml_string(v, buf, sizeof(buf), &blen) != 0) {
        return -1;
    }
    return eclat_sha256_from_hex(buf, blen, out);
}

static int uint_key(eclat_toml_view val, uint32_t *dst, eclat_policy_error_t *err, int lineno,
                    const char *field) {
    int64_t v = 0;
    if (eclat_toml_int(val, &v) != 0) {
        set_err(err, lineno, field, "invalid integer");
        return -1;
    }
    if (v < 0 || v > 4294967295LL) {
        set_err(err, lineno, field, "out of range");
        return -1;
    }
    *dst = (uint32_t)v;
    return 0;
}

/* Clé `cap` (allow ou deny) : chaîne → éclat_cap_t via names.c. */
static int parse_cap(eclat_toml_view val, eclat_cap_t *out, eclat_policy_error_t *err, int lineno) {
    char buf[32];
    size_t blen = 0;
    if (eclat_toml_string(val, buf, sizeof(buf), &blen) != 0) {
        set_err(err, lineno, "cap", "invalid string");
        return -1;
    }
    if (eclat_cap_from_name(buf, blen, out) != 0) {
        set_err(err, lineno, "cap", "unknown capability");
        return -1;
    }
    return 0;
}

static int module_key(eclat_policy_t *out, eclat_toml_view key, eclat_toml_view val,
                      int *have_sha, eclat_policy_error_t *err, int lineno) {
    if (eclat_toml_eq(key, "name")) {
        size_t nlen = 0;
        if (eclat_toml_string(val, out->name, ECLAT_POLICY_NAME_MAX + 1, &nlen) != 0) {
            set_err(err, lineno, "name", "invalid string or longer than 64 chars");
            return -1;
        }
        out->name_len = nlen;
        return 0;
    }
    if (eclat_toml_eq(key, "sha256")) {
        if (parse_sha256(val, out->sha256) != 0) {
            set_err(err, lineno, "sha256", "must be a 64-char hex string");
            return -1;
        }
        *have_sha = 1;
        return 0;
    }
    if (eclat_toml_eq(key, "cpu_budget_ms_per_s"))
        return uint_key(val, &out->cpu_budget_ms_per_s, err, lineno, "cpu_budget_ms_per_s");
    if (eclat_toml_eq(key, "mem_max_kib"))
        return uint_key(val, &out->mem_max_kib, err, lineno, "mem_max_kib");
    if (eclat_toml_eq(key, "calls_per_s"))
        return uint_key(val, &out->calls_per_s, err, lineno, "calls_per_s");
    return 0; /* clé inconnue ignorée (CONTRACT.md §2) */
}

static int allow_key(eclat_policy_t *out, eclat_toml_view key, eclat_toml_view val,
                     eclat_policy_error_t *err, int lineno) {
    eclat_allow_rule_t *r = &out->allows[out->n_allows - 1];
    if (eclat_toml_eq(key, "cap")) return parse_cap(val, &r->cap, err, lineno);
    if (eclat_toml_eq(key, "ids")) {
        int32_t arr[IDS_MAX];
        size_t n = 0;
        if (eclat_toml_is_array(val)) {
            if (eclat_toml_int_array(val, arr, IDS_MAX, &n) != 0) {
                set_err(err, lineno, "ids", "invalid integer array");
                return -1;
            }
        } else {
            int64_t v = 0;
            if (eclat_toml_int(val, &v) != 0 || v < INT32_MIN || v > INT32_MAX) {
                set_err(err, lineno, "ids", "invalid integer");
                return -1;
            }
            arr[0] = (int32_t)v;
            n = 1;
        }
        if (n == 0) {
            set_err(err, lineno, "ids", "empty");
            return -1;
        }
        int32_t mn = arr[0], mx = arr[0];
        for (size_t i = 1; i < n; i++) {
            if (arr[i] < mn) mn = arr[i];
            if (arr[i] > mx) mx = arr[i];
        }
        r->id_min = mn;
        r->id_max = mx;
        r->has_ids = 1;
        return 0;
    }
    if (eclat_toml_eq(key, "value_milli_range")) {
        int32_t arr[2];
        size_t n = 0;
        if (eclat_toml_int_array(val, arr, 2, &n) != 0 || n != 2) {
            set_err(err, lineno, "value_milli_range", "must be [min, max]");
            return -1;
        }
        r->val_min = arr[0];
        r->val_max = arr[1];
        r->has_value_range = 1;
        return 0;
    }
    return 0; /* clé inconnue ignorée */
}

static int deny_key(eclat_policy_t *out, eclat_toml_view key, eclat_toml_view val,
                    eclat_policy_error_t *err, int lineno) {
    if (eclat_toml_eq(key, "cap"))
        return parse_cap(val, &out->denies[out->n_denies - 1], err, lineno);
    return 0; /* clé inconnue ignorée */
}

/* En-tête de section : [module "nom"], [[allow]] ou [[deny]]. */
static int section_header(eclat_policy_t *out, eclat_toml_view line, int *section,
                          int *have_module, eclat_policy_error_t *err, int lineno) {
    if (eclat_toml_eq(line, "[[allow]]")) {
        if (out->n_allows >= ECLAT_POLICY_MAX_ALLOWS) {
            set_err(err, lineno, "allow", "too many [[allow]] rules");
            return -1;
        }
        eclat_allow_rule_t *r = &out->allows[out->n_allows++];
        r->cap = ECLAT_CAP_COUNT; /* placeholder, validé plus tard */
        r->has_ids = 0;
        r->has_value_range = 0;
        r->id_min = r->id_max = r->val_min = r->val_max = 0;
        *section = 2;
        return 0;
    }
    if (eclat_toml_eq(line, "[[deny]]")) {
        if (out->n_denies >= ECLAT_POLICY_MAX_DENIES) {
            set_err(err, lineno, "deny", "too many [[deny]] rules");
            return -1;
        }
        out->denies[out->n_denies++] = ECLAT_CAP_COUNT; /* placeholder */
        *section = 3;
        return 0;
    }
    if (eclat_toml_eq(line, "[module]")) {
        *have_module = 1;
        *section = 1;
        return 0;
    }
    set_err(err, lineno, "section", "unknown section header");
    return -1;
}

static int keyval(eclat_policy_t *out, int section, eclat_toml_view key, eclat_toml_view val,
                  int *have_sha, eclat_policy_error_t *err, int lineno) {
    if (section == 1) return module_key(out, key, val, have_sha, err, lineno);
    if (section == 2) return allow_key(out, key, val, err, lineno);
    if (section == 3) return deny_key(out, key, val, err, lineno);
    set_err(err, lineno, "section", "key outside any section");
    return -1;
}

int eclat_policy_parse(const uint8_t *data, size_t len, eclat_policy_t *out,
                       eclat_policy_error_t *err) {
    if (data == NULL || out == NULL) {
        set_err(err, 0, "policy", "null argument");
        return -1;
    }
    memset(out, 0, sizeof(*out));
    int have_module = 0, have_sha = 0, section = 0, lineno = 0;
    const char *p = (const char *)data;
    size_t i = 0;
    while (i < len) {
        size_t ls = i;
        while (i < len && p[i] != '\n') i++;
        eclat_toml_view line = eclat_toml_trim((eclat_toml_view){p + ls, i - ls});
        if (i < len) i++; /* saute le '\n' */
        lineno++;
        if (line.n == 0) continue;
        if (line.p[0] == '[') {
            if (section_header(out, line, &section, &have_module, err, lineno) != 0) return -1;
            continue;
        }
        size_t eq = 0;
        int found = 0;
        for (size_t k = 0; k < line.n; k++) {
            if (line.p[k] == '=') { eq = k; found = 1; break; }
        }
        if (!found) {
            set_err(err, lineno, "line", "expected key = value");
            return -1;
        }
        eclat_toml_view key = eclat_toml_trim((eclat_toml_view){line.p, eq});
        eclat_toml_view val = eclat_toml_trim((eclat_toml_view){line.p + eq + 1, line.n - eq - 1});
        if (keyval(out, section, key, val, &have_sha, err, lineno) != 0) return -1;
    }
    if (!have_module) {
        set_err(err, lineno, "module", "missing [module \"name\"] section");
        return -1;
    }
    if (!have_sha) {
        set_err(err, lineno, "sha256", "missing required sha256");
        return -1;
    }
    if (out->name_len == 0) {
        set_err(err, lineno, "name", "empty module name");
        return -1;
    }
    return eclat_policy_validate(out, err);
}

int eclat_policy_validate(const eclat_policy_t *p, eclat_policy_error_t *err) {
    if (p == NULL) {
        set_err(err, 0, "policy", "null");
        return -1;
    }
    if (p->name_len == 0 || p->name_len > ECLAT_POLICY_NAME_MAX) {
        set_err(err, 0, "name", "invalid length");
        return -1;
    }
    if (p->n_allows > ECLAT_POLICY_MAX_ALLOWS) {
        set_err(err, 0, "allow", "too many rules");
        return -1;
    }
    if (p->n_denies > ECLAT_POLICY_MAX_DENIES) {
        set_err(err, 0, "deny", "too many rules");
        return -1;
    }
    for (uint32_t i = 0; i < p->n_allows; i++) {
        const eclat_allow_rule_t *r = &p->allows[i];
        if ((unsigned)r->cap >= (unsigned)ECLAT_CAP_COUNT) {
            set_err(err, 0, "cap", "unknown capability in allow");
            return -1;
        }
        if (r->has_ids && r->id_min > r->id_max) {
            set_err(err, 0, "ids", "id_min > id_max");
            return -1;
        }
        if (r->has_value_range && r->val_min > r->val_max) {
            set_err(err, 0, "value_milli_range", "val_min > val_max");
            return -1;
        }
    }
    for (uint32_t i = 0; i < p->n_denies; i++) {
        if ((unsigned)p->denies[i] >= (unsigned)ECLAT_CAP_COUNT) {
            set_err(err, 0, "cap", "unknown capability in deny");
            return -1;
        }
    }
    return 0;
}