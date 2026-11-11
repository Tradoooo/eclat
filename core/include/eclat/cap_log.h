// SPDX-License-Identifier: Apache-2.0
/* Éclat — journal des décisions (core, C11 pur).
 * ECL-05 : les refus sont journalisés. CONTRACT.md §5 : une ligne JSON par refus ou
 * dépassement ; les décisions accordées ne sont émises qu'en mode audit.
 * Aucune allocation, aucun état global : la mise en forme écrit dans un tampon de l'appelant.
 */
#ifndef ECLAT_CAP_LOG_H
#define ECLAT_CAP_LOG_H

#include <stddef.h>
#include <stdint.h>

#include "cap_decision.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eclat_log_level {
    ECLAT_LOG_DEBUG = 0,
    ECLAT_LOG_INFO = 1,
    ECLAT_LOG_WARN = 2,
    ECLAT_LOG_ERROR = 3
} eclat_log_level_t;

/* Récepteur de lignes : l'hôte fournit la fonction d'émission (fichier, socket, stdout). */
typedef void (*eclat_log_sink_fn)(void *ctx, eclat_log_level_t level, const char *msg,
                                  size_t len);

typedef struct eclat_log {
    eclat_log_sink_fn sink;
    void *ctx;
    int audit_mode; /* 1 = journalise aussi les décisions accordées */
} eclat_log_t;

void eclat_log_init(eclat_log_t *l, eclat_log_sink_fn sink, void *ctx, int audit_mode);

/* Émet une ligne via le récepteur (no-op si `sink` est NULL). */
void eclat_log_write(const eclat_log_t *l, eclat_log_level_t level, const char *msg, size_t len);

/* Met en forme la ligne JSON d'une décision dans `buf`. Retourne la longueur écrite, ou -1 si
 * le tampon est trop petit. Format (CONTRACT.md §5) :
 *   {"seq":12,"module":"diag-partner","cap":"write_actuator","decision":"CAP_DENIED_RANGE"} */
int eclat_log_format_decision(char *buf, size_t buflen, uint64_t seq, const char *module,
                              size_t module_len, eclat_cap_t cap, eclat_decision_t d);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_CAP_LOG_H */