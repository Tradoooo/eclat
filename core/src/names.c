// SPDX-License-Identifier: Apache-2.0
/* Éclat — table des noms des capacités et codes (core, C11 pur). Source unique de vérité des
 * chaînes textuelles, pour le TOML, le journal et le jumeau Python. */
#include "eclat/cap_decision.h"

#include <string.h>

static const char *const CAP_NAMES[ECLAT_CAP_COUNT] = {
    "time_ns", "log", "read_sensor", "write_actuator", "kv_get", "kv_put", "net_send",
};

static const char *const DECISION_NAMES[] = {
    "CAP_OK",          "CAP_DENIED_POLICY", "CAP_DENIED_RANGE", "CAP_DENIED_BUDGET",
    "CAP_DENIED_UNKNOWN", "CAP_BAD_ARG",     "CAP_KILLED_CPU",  "CAP_KILLED_MEM",
};

const char *eclat_cap_name(eclat_cap_t c) {
    if ((unsigned)c >= (unsigned)ECLAT_CAP_COUNT) {
        return "unknown";
    }
    return CAP_NAMES[c];
}

const char *eclat_decision_name(eclat_decision_t d) {
    if ((unsigned)d > (unsigned)ECLAT_CAP_KILLED_MEM) {
        return "unknown";
    }
    return DECISION_NAMES[d];
}

int eclat_cap_from_name(const char *name, size_t len, eclat_cap_t *out) {
    for (size_t i = 0; i < ECLAT_CAP_COUNT; i++) {
        if (strlen(CAP_NAMES[i]) == len && memcmp(CAP_NAMES[i], name, len) == 0) {
            *out = (eclat_cap_t)i;
            return 0;
        }
    }
    return -1;
}