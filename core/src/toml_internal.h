// SPDX-License-Identifier: Apache-2.0
/* Éclat — en-tête privé du parseur de valeurs TOML restreint (core/src, interne).
 * Réutilisable : policy.c et le fuzzer (lot L6) s'appuient dessus. Aucune allocation,
 * aucune I/O, aucun état global : les fonctions opèrent sur des vues en lecture seule. */
#ifndef ECLAT_TOML_INTERNAL_H
#define ECLAT_TOML_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* Vue en lecture seule sur un fragment du tampon d'entrée. */
typedef struct eclat_toml_view {
    const char *p;
    size_t n;
} eclat_toml_view;

/* Retire les espaces de tête/queue et un commentaire `#` (le `#` n'est traité comme
 * commentaire que s'il est en début de vue ou précédé d'un espace — jamais dans une chaîne). */
eclat_toml_view eclat_toml_trim(eclat_toml_view v);

/* Compare une vue à une chaîne littérale terminée par zéro. */
int eclat_toml_eq(eclat_toml_view v, const char *lit);

/* Parse un entier décimal signé. Retourne 0 si la vue est un entier valide. */
int eclat_toml_int(eclat_toml_view v, int64_t *out);

/* Parse une chaîne entre guillemets droits `"…"` (échappements `\"`, `\\`, `\n`, `\t`).
 * Copie dans `out` (capacité `cap`, NUL final inclus). Retourne 0 si valide. */
int eclat_toml_string(eclat_toml_view v, char *out, size_t cap, size_t *outlen);

/* Retourne 1 si la vue (déjà trimée) commence par `[`. */
int eclat_toml_is_array(eclat_toml_view v);

/* Parse un tableau d'entiers `[a, b, …]` dans `arr` (capacité `cap`), `*n` = nombre lu.
 * Retourne 0 si valide (tableau vide → *n == 0). */
int eclat_toml_int_array(eclat_toml_view v, int32_t *arr, size_t cap, size_t *n);

#endif /* ECLAT_TOML_INTERNAL_H */