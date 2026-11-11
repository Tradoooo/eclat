// SPDX-License-Identifier: Apache-2.0
/* Éclat — moteur de décision (core, C11 pur : ni horloge, ni fichier, ni socket, ni
 * allocation, ni état global mutable).
 * ECL-01 : compile en C11 strict, sans QNX ni WAMR.
 * ECL-05 : le module hostile reçoit CAP_DENIED_POLICY (refus par défaut).
 * CONTRACT.md §3 : les 7 étapes dans l'ordre strict, du plus prioritaire au moins.
 */
#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"

#include <stddef.h>

/* Trouve la première règle allow portant sur `cap` (CONTRACT.md §3 étapes 4-6).
 * Retourne un pointeur vers la règle, ou NULL si aucune (étape 4). */
static const eclat_allow_rule_t *find_allow(const eclat_policy_t *policy, eclat_cap_t cap) {
    for (uint32_t i = 0; i < policy->n_allows; i++) {
        if (policy->allows[i].cap == cap) {
            return &policy->allows[i];
        }
    }
    return NULL;
}

/* CONTRACT.md §3 étape 2 : `cap` présent dans policy->denies → CAP_DENIED_POLICY. */
static int is_denied(const eclat_policy_t *policy, eclat_cap_t cap) {
    for (uint32_t i = 0; i < policy->n_denies; i++) {
        if (policy->denies[i] == cap) {
            return 1;
        }
    }
    return 0;
}

/* Contrat §3 étape 5 : contrainte d'id inclusive [id_min, id_max] si has_ids. */
static int id_out_of_range(const eclat_allow_rule_t *rule, int32_t id) {
    return rule->has_ids && (id < rule->id_min || id > rule->id_max);
}

/* Contrat §3 étape 6 : contrainte de valeur (write_actuator) si has_value_range. */
static int value_out_of_range(const eclat_allow_rule_t *rule, int32_t value_milli) {
    return rule->has_value_range && (value_milli < rule->val_min || value_milli > rule->val_max);
}

/* Décision déterministe, ordre gelé CONTRACT.md §3 (étapes 1 à 7).
 * Ne produit que CAP_OK / CAP_DENIED_POLICY / CAP_DENIED_RANGE / CAP_DENIED_UNKNOWN. */
eclat_decision_t eclat_decide(const struct eclat_policy *policy, const eclat_request_t *req) {
    /* Étape 1 : cap hors [0, ECLAT_CAP_COUNT) → CAP_DENIED_UNKNOWN. */
    if (req == NULL || (unsigned)req->cap >= (unsigned)ECLAT_CAP_COUNT) {
        return ECLAT_CAP_DENIED_UNKNOWN;
    }
    /* Sans politique, refus par défaut (CONTRACT.md §2 : tout refus par défaut). */
    if (policy == NULL) {
        return ECLAT_CAP_DENIED_POLICY;
    }
    /* Étape 2 : cap dans policy->denies → CAP_DENIED_POLICY (deny gagne sur allow). */
    if (is_denied(policy, req->cap)) {
        return ECLAT_CAP_DENIED_POLICY;
    }
    /* Étape 3 : TIME_NS et LOG passent sans règle allow (facilités hôte). */
    if (req->cap == ECLAT_CAP_TIME_NS || req->cap == ECLAT_CAP_LOG) {
        return ECLAT_CAP_OK;
    }
    /* Étape 4 : aucune règle allow avec rule.cap == cap → CAP_DENIED_POLICY. */
    const eclat_allow_rule_t *rule = find_allow(policy, req->cap);
    if (rule == NULL) {
        return ECLAT_CAP_DENIED_POLICY;
    }
    /* Étape 5 : id hors [id_min, id_max] → CAP_DENIED_RANGE. */
    if (id_out_of_range(rule, req->id)) {
        return ECLAT_CAP_DENIED_RANGE;
    }
    /* Étape 6 : value_milli hors [val_min, val_max] (write_actuator) → CAP_DENIED_RANGE. */
    if (req->cap == ECLAT_CAP_WRITE_ACTUATOR && value_out_of_range(rule, req->value_milli)) {
        return ECLAT_CAP_DENIED_RANGE;
    }
    /* Étape 7 : sinon CAP_OK. */
    return ECLAT_CAP_OK;
}
