// SPDX-License-Identifier: Apache-2.0
/* Éclat — politique de capacités (core, C11 pur).
 * ECL-02 : la politique est analysée et validée ; politique invalide refusée avec champ et
 * cause ; tout refus par défaut.
 * Aucune allocation : le parseur remplit une structure fournie par l'appelant, à tableaux de
 * taille fixe. Aucune entrée/sortie de fichier : le parseur consomme un tampon d'octets.
 */
#ifndef ECLAT_CAP_POLICY_H
#define ECLAT_CAP_POLICY_H

#include <stddef.h>
#include <stdint.h>

#include "cap_decision.h"

#define ECLAT_POLICY_MAX_ALLOWS 32
#define ECLAT_POLICY_MAX_DENIES 16
#define ECLAT_POLICY_NAME_MAX 64

/* Règle d'autorisation (CONTRACT.md §2).
 * `has_ids == 0` : pas de contrainte d'id (id_min/id_max ignorés). Sinon contrainte inclusive
 * [id_min, id_max]. `has_value_range == 0` : pas de contrainte de valeur (val_min/val_max
 * ignorés). Sinon contrainte inclusive (write_actuator). Les drapeaux lèvent l'ambiguïté
 * d'un -1 à la fois sentinelle « absent » et valeur d'id légitime (voir docs/DECISIONS.md). */
typedef struct eclat_allow_rule {
    eclat_cap_t cap;
    int has_ids;
    int32_t id_min;
    int32_t id_max;
    int has_value_range;
    int32_t val_min;
    int32_t val_max;
} eclat_allow_rule_t;

/* Politique d'un module, telle que parsée et validée. */
typedef struct eclat_policy {
    char name[ECLAT_POLICY_NAME_MAX + 1];
    size_t name_len;
    uint8_t sha256[32]; /* empreinte obligatoire du .wasm (32 octets) */
    uint32_t cpu_budget_ms_per_s; /* 0 = illimité */
    uint32_t mem_max_kib;         /* 0 = illimité */
    uint32_t calls_per_s;         /* 0 = illimité */
    eclat_allow_rule_t allows[ECLAT_POLICY_MAX_ALLOWS];
    uint32_t n_allows;
    eclat_cap_t denies[ECLAT_POLICY_MAX_DENIES];
    uint32_t n_denies;
} eclat_policy_t;

/* Erreur de parse ou de validation : numéro de ligne, champ et motif lisible. */
typedef struct eclat_policy_error {
    int lineno;
    char field[32];
    char message[96];
} eclat_policy_error_t;

/* Analyse un tampon TOML (sous-ensemble documenté dans CONTRACT.md §2) et remplit *out.
 * Retourne 0 en cas de succès ; sinon remplit *err et laisse *out indéfini. */
int eclat_policy_parse(const uint8_t *data, size_t len, eclat_policy_t *out,
                       eclat_policy_error_t *err);

/* Valide la cohérence d'une politique déjà construite (champs requis, plages ordonnées,
 * budget, nombre de règles). Retourne 0 si valide. */
int eclat_policy_validate(const eclat_policy_t *p, eclat_policy_error_t *err);

#endif /* ECLAT_CAP_POLICY_H */