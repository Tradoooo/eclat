// SPDX-License-Identifier: Apache-2.0
/* Éclat — ABI des imports, vue du module WebAssembly.
 *
 * Reproduction fidèle de CONTRACT.md §1. Les attributs clang `import_module` / `import_name`
 * font de ces déclarations de vrais imports WebAssembly du module « env », sans passer par la
 * résolution de symboles à l'édition de liens : le module n'a besoin d'aucune libc.
 *
 * Règles du contrat, rappelées : aucune chaîne terminée par zéro (pointeur + longueur
 * systématiquement) ; toute longueur est vérifiée par l'hôte contre le tampon déclaré AVANT
 * copie ; un import inconnu est refusé à l'instantiation, jamais à l'exécution. */
#ifndef ECLAT_IMPORTS_H
#define ECLAT_IMPORTS_H

#include <stdint.h>

#define ECLAT_IMPORT __attribute__((import_module("env")))

ECLAT_IMPORT __attribute__((import_name("cap_time_ns")))
int64_t cap_time_ns(void);

ECLAT_IMPORT __attribute__((import_name("cap_log")))
int32_t cap_log(const void *msg, int32_t len, int32_t level);

ECLAT_IMPORT __attribute__((import_name("cap_read_sensor")))
int32_t cap_read_sensor(int32_t id, void *out, int32_t out_len);

ECLAT_IMPORT __attribute__((import_name("cap_write_actuator")))
int32_t cap_write_actuator(int32_t id, int32_t value_milli);

ECLAT_IMPORT __attribute__((import_name("cap_kv_get")))
int32_t cap_kv_get(const void *key, int32_t key_len, void *val, int32_t val_len);

ECLAT_IMPORT __attribute__((import_name("cap_kv_put")))
int32_t cap_kv_put(const void *key, int32_t key_len, const void *val, int32_t val_len);

ECLAT_IMPORT __attribute__((import_name("cap_net_send")))
int32_t cap_net_send(int32_t chan, const void *buf, int32_t len);

/* Point d'entrée exporté, appelé par le chargeur. Retourne 0 si le module a fini normalement. */

/* Journalisation : la longueur est calculée à la compilation, aucune mesure à l'exécution. */
#define LOG(msg, niveau) cap_log((msg), (int32_t)(sizeof(msg) - 1), (niveau))

#define NIVEAU_INFO 1
#define NIVEAU_ALERTE 2

#endif /* ECLAT_IMPORTS_H */