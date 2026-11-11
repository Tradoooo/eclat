// SPDX-License-Identifier: Apache-2.0
/* Éclat — tests ECL-05 du journal (CONTRACT.md §5).
 * Format JSON exact, tampon trop petit -> -1, sink appelé / NULL no-op, audit_mode. */
#include "eclat/cap_log.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static void check(int cond, const char *name) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL %s (check #%d)\n", name, checks);
    }
}

/* Exemple canonique de CONTRACT.md §5. */
static void test_format_exact(void) {
    char buf[256];
    int n = eclat_log_format_decision(buf, sizeof(buf), 12, "diag-partner", 12,
                                      ECLAT_CAP_WRITE_ACTUATOR, ECLAT_CAP_DENIED_RANGE);
    const char *want =
        "{\"seq\":12,\"module\":\"diag-partner\",\"cap\":\"write_actuator\","
        "\"decision\":\"CAP_DENIED_RANGE\"}";
    check(n == (int)strlen(want), "ECL-05 format : longueur == strlen(JSON)");
    check(strcmp(buf, want) == 0, "ECL-05 format : chaîne JSON exacte (§5)");
    check(buf[n] == '\0', "ECL-05 format : NUL final posé");
}

/* Autres combinaisons : seq 0, CAP_OK, seq u64 max, module vide. */
static void test_format_variants(void) {
    char buf[256];
    int n = eclat_log_format_decision(buf, sizeof(buf), 0, "m", 1, ECLAT_CAP_TIME_NS,
                                      ECLAT_CAP_OK);
    check(n > 0 && strcmp(buf, "{\"seq\":0,\"module\":\"m\",\"cap\":\"time_ns\","
                               "\"decision\":\"CAP_OK\"}") == 0,
          "ECL-05 format : seq=0/time_ns/CAP_OK");
    n = eclat_log_format_decision(buf, sizeof(buf), 18446744073709551615ULL, "", 0,
                                  ECLAT_CAP_NET_SEND, ECLAT_CAP_DENIED_UNKNOWN);
    check(n > 0 && strstr(buf, "\"seq\":18446744073709551615,") == buf + 1,
          "ECL-05 format : seq u64 max, 20 chiffres");
    check(n > 0 && strstr(buf, "\"cap\":\"net_send\"") != NULL, "ECL-05 format : cap net_send");
    check(n > 0 && strstr(buf, "\"decision\":\"CAP_DENIED_UNKNOWN\"}") != NULL,
          "ECL-05 format : decision UNKNOWN en fin de ligne");
    check(n > 0 && strstr(buf, "\"module\":\"\",") != NULL, "ECL-05 format : module vide");
    n = eclat_log_format_decision(buf, sizeof(buf), 7, NULL, 0, ECLAT_CAP_LOG,
                                  ECLAT_CAP_DENIED_POLICY);
    check(n > 0 && strstr(buf, "\"module\":\"\"") != NULL, "ECL-05 format : module NULL -> vide");
}

/* Tampon trop petit -> -1 ; taille exacte (avec NUL) -> succès. */
static void test_buffer_bounds(void) {
    const char *want =
        "{\"seq\":12,\"module\":\"diag-partner\",\"cap\":\"write_actuator\","
        "\"decision\":\"CAP_DENIED_RANGE\"}";
    size_t need = strlen(want);
    char buf[256];
    check(eclat_log_format_decision(buf, need, 12, "diag-partner", 12, ECLAT_CAP_WRITE_ACTUATOR,
                                    ECLAT_CAP_DENIED_RANGE) == -1,
          "ECL-05 bounds : buflen == len (pas de place NUL) -> -1");
    check(eclat_log_format_decision(buf, need - 1, 12, "diag-partner", 12,
                                    ECLAT_CAP_WRITE_ACTUATOR, ECLAT_CAP_DENIED_RANGE) == -1,
          "ECL-05 bounds : buflen < len -> -1");
    check(eclat_log_format_decision(buf, 0, 12, "diag-partner", 12, ECLAT_CAP_WRITE_ACTUATOR,
                                    ECLAT_CAP_DENIED_RANGE) == -1,
          "ECL-05 bounds : buflen 0 -> -1");
    check(eclat_log_format_decision(NULL, 256, 12, "diag-partner", 12, ECLAT_CAP_WRITE_ACTUATOR,
                                    ECLAT_CAP_DENIED_RANGE) == -1,
          "ECL-05 bounds : buf NULL -> -1");
    int n = eclat_log_format_decision(buf, need + 1, 12, "diag-partner", 12,
                                      ECLAT_CAP_WRITE_ACTUATOR, ECLAT_CAP_DENIED_RANGE);
    check(n == (int)need && strcmp(buf, want) == 0, "ECL-05 bounds : buflen exact+1 -> écrit");
}

/* Récepteur de test : capture ctx/level/msg/len. */
typedef struct capture {
    int calls;
    eclat_log_level_t level;
    char msg[256];
    size_t len;
    void *ctx_seen;
} capture_t;

static void capture_sink(void *ctx, eclat_log_level_t level, const char *msg, size_t len) {
    capture_t *c = (capture_t *)ctx;
    c->calls++;
    c->level = level;
    c->len = len;
    c->ctx_seen = ctx;
    if (len < sizeof(c->msg)) {
        memcpy(c->msg, msg, len);
        c->msg[len] = '\0';
    }
}

/* init, sink appelé avec les bons arguments, NULL no-op, audit_mode préservé. */
static void test_sink_and_init(void) {
    capture_t cap;
    memset(&cap, 0, sizeof(cap));
    eclat_log_t l;
    eclat_log_init(&l, capture_sink, &cap, 1);
    check(l.sink == capture_sink, "ECL-05 init : sink enregistré");
    check(l.ctx == &cap, "ECL-05 init : ctx enregistré");
    check(l.audit_mode == 1, "ECL-05 init : audit_mode=1 préservé");
    eclat_log_init(&l, capture_sink, &cap, 0);
    check(l.audit_mode == 0, "ECL-05 init : audit_mode=0 préservé");
    eclat_log_write(&l, ECLAT_LOG_WARN, "hello", 5);
    check(cap.calls == 1, "ECL-05 write : sink appelé une fois");
    check(cap.level == ECLAT_LOG_WARN, "ECL-05 write : level transmis");
    check(cap.len == 5 && strcmp(cap.msg, "hello") == 0, "ECL-05 write : msg/len transmis");
    check(cap.ctx_seen == &cap, "ECL-05 write : ctx transmis au sink");
    eclat_log_t nosink;
    eclat_log_init(&nosink, NULL, NULL, 0);
    eclat_log_write(&nosink, ECLAT_LOG_ERROR, "x", 1); /* no-op, ne doit pas planter */
    check(nosink.sink == NULL, "ECL-05 write : sink NULL -> no-op");
    eclat_log_write(NULL, ECLAT_LOG_INFO, "x", 1); /* no-op défensif */
    check(1, "ECL-05 write : log NULL -> no-op sans crash");
}

int main(void) {
    test_format_exact();
    test_format_variants();
    test_buffer_bounds();
    test_sink_and_init();
    if (failures != 0) {
        printf("KO %d/%d checks\n", failures, checks);
        return 1;
    }
    printf("OK %d checks\n", checks);
    return 0;
}
