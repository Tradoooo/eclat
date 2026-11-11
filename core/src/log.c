// SPDX-License-Identifier: Apache-2.0
/* Éclat — journal des décisions (core, C11 pur : aucune allocation, aucun état global,
 * aucune I/O — n'utilise QUE le tampon fourni par l'appelant).
 * ECL-05 : les refus sont journalisés.
 * CONTRACT.md §5 : ligne JSON exacte
 *   {"seq":12,"module":"diag-partner","cap":"write_actuator","decision":"CAP_DENIED_RANGE"}
 */
#include "eclat/cap_log.h"

#include <string.h>

void eclat_log_init(eclat_log_t *l, eclat_log_sink_fn sink, void *ctx, int audit_mode) {
    if (l == NULL) {
        return;
    }
    l->sink = sink;
    l->ctx = ctx;
    l->audit_mode = audit_mode;
}

/* cap_log.h : émet une ligne via le récepteur ; no-op si sink est NULL. */
void eclat_log_write(const eclat_log_t *l, eclat_log_level_t level, const char *msg, size_t len) {
    if (l == NULL || l->sink == NULL || msg == NULL) {
        return;
    }
    l->sink(l->ctx, level, msg, len);
}

/* Écrit un entier décimal non signé dans out (sans NUL). Retourne le nombre de chiffres. */
static size_t fmt_u64(char *out, uint64_t v) {
    char tmp[20];
    size_t n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v != 0);
    for (size_t i = 0; i < n; i++) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

static size_t u64_digits(uint64_t v) {
    size_t n = 1;
    while (v >= 10) {
        v /= 10;
        n++;
    }
    return n;
}

/* CONTRACT.md §5 : met en forme la ligne JSON dans buf. Retourne la longueur écrite
 * (hors NUL), ou -1 si le tampon est trop petit. Réutilise names.c pour cap/decision. */
int eclat_log_format_decision(char *buf, size_t buflen, uint64_t seq, const char *module,
                              size_t module_len, eclat_cap_t cap, eclat_decision_t d) {
    if (buf == NULL) {
        return -1;
    }
    if (module == NULL) {
        module_len = 0;
    }
    /* Noms issus de names.c (source unique de vérité, zéro duplication). */
    const char *cap_name = eclat_cap_name(cap);
    const char *dec_name = eclat_decision_name(d);
    size_t cap_len = strlen(cap_name);
    size_t dec_len = strlen(dec_name);
    size_t digits = u64_digits(seq);
    /* Longueurs des segments fixes :
     * {"seq":  ,"module":"  ","cap":"  ","decision":"  "} */
    size_t need = 7 + digits + 11 + module_len + 9 + cap_len + 14 + dec_len + 2;
    if (buflen < need + 1) { /* +1 : octet NUL final */
        return -1;
    }
    size_t o = 0;
    memcpy(buf + o, "{\"seq\":", 7); o += 7;
    o += fmt_u64(buf + o, seq);
    memcpy(buf + o, ",\"module\":\"", 11); o += 11;
    if (module_len > 0) {
        memcpy(buf + o, module, module_len);
        o += module_len;
    }
    memcpy(buf + o, "\",\"cap\":\"", 9); o += 9;
    memcpy(buf + o, cap_name, cap_len); o += cap_len;
    memcpy(buf + o, "\",\"decision\":\"", 14); o += 14;
    memcpy(buf + o, dec_name, dec_len); o += dec_len;
    memcpy(buf + o, "\"}", 2); o += 2;
    buf[o] = '\0';
    return (int)o;
}
