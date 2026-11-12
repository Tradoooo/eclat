// SPDX-License-Identifier: Apache-2.0
/* Éclat — banc de latence de décision (ECL-12 partiel, idée B25).
 *
 * Méthode : mesurer CHAQUE décision isolément serait dominé par le coût de clock_gettime
 * (≈ 25 ns), ce qui rendrait le chiffre illisible. On mesure donc des LOTS de décisions et on
 * rapporte la moyenne par décision ainsi que la dispersion des lots. Le coût de l'horloge est
 * mesuré séparément et rapporté, jamais soustrait en douce.
 *
 * Usage : eclat-bench <policy.toml> [batches] [batch_size]
 */
#define _POSIX_C_SOURCE 200809L

#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_BATCHES 200
#define DEFAULT_BATCH_SIZE 1000
#define MAX_INPUT (64 * 1024)

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Coût d'un appel d'horloge seul : sert à interpréter honnêtement les chiffres. */
static double clock_overhead_ns(void) {
    const int n = 100000;
    uint64_t t0 = now_ns();
    for (int i = 0; i < n; i++) {
        volatile uint64_t t = now_ns();
        (void)t;
    }
    return (double)(now_ns() - t0) / (double)n;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: eclat-bench <policy.toml> [batches] [batch_size]\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        fprintf(stderr, "ERROR:policy:cannot read %s\n", argv[1]);
        return 2;
    }
    static uint8_t buf[MAX_INPUT];
    size_t len = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    eclat_policy_t policy;
    eclat_policy_error_t err;
    memset(&policy, 0, sizeof(policy));
    memset(&err, 0, sizeof(err));
    if (eclat_policy_parse(buf, len, &policy, &err) != 0) {
        fprintf(stderr, "ERROR:%s:%s\n", err.field, err.message);
        return 2;
    }

    int batches = (argc > 2) ? atoi(argv[2]) : DEFAULT_BATCHES;
    int bsize = (argc > 3) ? atoi(argv[3]) : DEFAULT_BATCH_SIZE;
    if (batches < 1 || bsize < 1) {
        fprintf(stderr, "ERROR:args:batches et batch_size doivent être ≥ 1\n");
        return 2;
    }

    uint64_t *samples = calloc((size_t)batches, sizeof(uint64_t));
    if (samples == NULL) {
        fprintf(stderr, "ERROR:mem:allocation échouée\n");
        return 2;
    }
    for (int b = 0; b < batches; b++) {
        uint64_t t0 = now_ns();
        for (int i = 0; i < bsize; i++) {
            eclat_request_t req;
            memset(&req, 0, sizeof(req));
            req.cap = (eclat_cap_t)(i % ECLAT_CAP_COUNT);
            req.id = i % 17;
            req.value_milli = (i % 2001) - 1000;
            volatile eclat_decision_t d = eclat_decide(&policy, &req);
            (void)d;
        }
        samples[b] = now_ns() - t0;
    }
    qsort(samples, (size_t)batches, sizeof(uint64_t), cmp_u64);

    double total = 0.0;
    for (int b = 0; b < batches; b++) {
        total += (double)samples[b];
    }
    double mean = total / (double)batches / (double)bsize;
    double p50 = (double)samples[batches / 2] / (double)bsize;
    double p99 = (double)samples[(batches * 99) / 100] / (double)bsize;
    double p999 = (double)samples[(batches * 999) / 1000] / (double)bsize;
    double mn = (double)samples[0] / (double)bsize;
    double mx = (double)samples[batches - 1] / (double)bsize;

    printf("{\"engine\":\"c\",\"batches\":%d,\"batch_size\":%d,\"mean_ns_per_decision\":%.3f,"
           "\"p50_ns\":%.3f,\"p99_ns\":%.3f,\"p999_ns\":%.3f,\"min_ns\":%.3f,\"max_ns\":%.3f,"
           "\"clock_overhead_ns\":%.3f}\n",
           batches, bsize, mean, p50, p99, p999, mn, mx, clock_overhead_ns());
    free(samples);
    return 0;
}