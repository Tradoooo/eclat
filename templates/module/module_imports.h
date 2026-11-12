// SPDX-License-Identifier: Apache-2.0
/* {{MODULE}} — ABI des imports du courtier, vue du module.
 *
 * Ce fichier reproduit EXACTEMENT le contrat de CONTRACT.md §1 : aucune chaîne terminée par
 * zéro (pointeur + longueur systématiquement), toute longueur vérifiée par l'hôte avant copie.
 * Il ne connaît ni QNX, ni WAMR, ni l'hôte : c'est la même déclaration que le module retrouvera
 * une fois compilé en .wasm. */
#ifndef MODULE_IMPORTS_H
#define MODULE_IMPORTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* () -> i64 : instant monotone en nanosecondes fourni par l'hôte. */
int64_t cap_time_ns(void);

/* (ptr, len, level) -> i32 : 0 si accepté, code de décision sinon. */
int32_t cap_log(const void *msg, int32_t len, int32_t level);

/* (id, out_ptr, out_len) -> i32 : remplit un tampon borné, soumis à politique. */
int32_t cap_read_sensor(int32_t id, void *out, int32_t out_len);

/* (id, value_milli) -> i32 : valeur entière en milli-unités, soumise à politique. */
int32_t cap_write_actuator(int32_t id, int32_t value_milli);

/* (key_ptr, key_len, val_ptr, val_len) -> i32 : stockage clés-valeurs borné. */
int32_t cap_kv_get(const void *key, int32_t key_len, void *val, int32_t val_len);
int32_t cap_kv_put(const void *key, int32_t key_len, const void *val, int32_t val_len);

/* (chan, ptr, len) -> i32 : envoi sur un canal déclaré, soumis à politique. */
int32_t cap_net_send(int32_t chan, const void *buf, int32_t len);

/* Point d'entrée du module — appelé par l'hôte (harnais natif ou runtime WAMR). */
int module_main(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_IMPORTS_H */