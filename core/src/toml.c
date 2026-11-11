// SPDX-License-Identifier: Apache-2.0
/* Éclat — parseur de valeurs TOML restreint (core, C11 pur : ni allocation, ni I/O, ni
 * état global). Voir toml_internal.h. Réutilisable par policy.c et le fuzzer (lot L6). */
#include "toml_internal.h"

#include <string.h>

static int is_space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

eclat_toml_view eclat_toml_trim(eclat_toml_view v) {
    while (v.n > 0 && is_space(v.p[0])) {
        v.p++;
        v.n--;
    }
    for (size_t i = 0; i < v.n; i++) {
        if (v.p[i] == '#' && (i == 0 || is_space(v.p[i - 1]))) {
            v.n = i;
            break;
        }
    }
    while (v.n > 0 && is_space(v.p[v.n - 1])) {
        v.n--;
    }
    return v;
}

int eclat_toml_eq(eclat_toml_view v, const char *lit) {
    size_t n = strlen(lit);
    return v.n == n && memcmp(v.p, lit, n) == 0;
}

int eclat_toml_int(eclat_toml_view v, int64_t *out) {
    if (v.n == 0) {
        return -1;
    }
    size_t i = 0;
    int neg = 0;
    if (v.p[0] == '-' || v.p[0] == '+') {
        neg = (v.p[0] == '-');
        i = 1;
    }
    if (i >= v.n) {
        return -1;
    }
    int64_t acc = 0;
    for (; i < v.n; i++) {
        if (v.p[i] < '0' || v.p[i] > '9') {
            return -1;
        }
        acc = acc * 10 + (v.p[i] - '0');
        if (acc > 4294967296LL) { /* garde anti-débordement (2^32) */
            return -1;
        }
    }
    *out = neg ? -acc : acc;
    return 0;
}

int eclat_toml_string(eclat_toml_view v, char *out, size_t cap, size_t *outlen) {
    if (v.n < 2 || v.p[0] != '"' || v.p[v.n - 1] != '"') {
        return -1;
    }
    size_t w = 0;
    for (size_t i = 1; i + 1 < v.n; i++) {
        char c = v.p[i];
        if (c == '\\' && i + 2 < v.n) {
            char e = v.p[++i];
            if (e == '"' || e == '\\') {
                c = e;
            } else if (e == 'n') {
                c = '\n';
            } else if (e == 't') {
                c = '\t';
            } else {
                return -1; /* échappement inconnu */
            }
        } else if (c == '"') {
            return -1; /* guillemet non échappé à l'intérieur */
        }
        if (w + 1 >= cap) {
            return -1; /* tampon trop petit */
        }
        out[w++] = c;
    }
    out[w] = '\0';
    if (outlen) {
        *outlen = w;
    }
    return 0;
}

int eclat_toml_is_array(eclat_toml_view v) { return v.n > 0 && v.p[0] == '['; }

int eclat_toml_int_array(eclat_toml_view v, int32_t *arr, size_t cap, size_t *n) {
    if (v.n < 2 || v.p[0] != '[' || v.p[v.n - 1] != ']') {
        return -1;
    }
    eclat_toml_view inner = eclat_toml_trim((eclat_toml_view){v.p + 1, v.n - 2});
    size_t count = 0;
    size_t i = 0;
    while (i < inner.n) {
        size_t start = i;
        while (i < inner.n && inner.p[i] != ',') {
            i++;
        }
        eclat_toml_view tok = eclat_toml_trim((eclat_toml_view){inner.p + start, i - start});
        if (tok.n > 0) {
            int64_t val;
            if (eclat_toml_int(tok, &val) != 0 || val < INT32_MIN || val > INT32_MAX) {
                return -1;
            }
            if (count >= cap) {
                return -1; /* tableau trop grand */
            }
            arr[count++] = (int32_t)val;
        }
        if (i < inner.n && inner.p[i] == ',') {
            i++;
        }
    }
    *n = count;
    return 0;
}