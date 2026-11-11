// SPDX-License-Identifier: Apache-2.0
/* Éclat — tests ECL-05 (+ ECL-01 : compilation C11 strict) du moteur de décision.
 * Les politiques sont construites DIRECTEMENT en C (indépendant du parseur L1).
 * Couvre les 7 étapes de CONTRACT.md §3 dans l'ordre strict. */
#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"

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

/* Politique vide (refus par défaut, CONTRACT.md §2). */
static eclat_policy_t make_policy(void) {
    eclat_policy_t p;
    memset(&p, 0, sizeof(p));
    return p;
}

static eclat_allow_rule_t make_allow(eclat_cap_t cap, int32_t id_min, int32_t id_max,
                                     int32_t val_min, int32_t val_max) {
    eclat_allow_rule_t r;
    memset(&r, 0, sizeof(r));
    r.cap = cap;
    r.has_ids = (id_min != -1);
    r.id_min = id_min;
    r.id_max = id_max;
    r.has_value_range = (val_min != -1);
    r.val_min = val_min;
    r.val_max = val_max;
    return r;
}

static eclat_request_t make_req(eclat_cap_t cap, int32_t id, int32_t value_milli) {
    eclat_request_t req;
    memset(&req, 0, sizeof(req));
    req.cap = cap;
    req.id = id;
    req.value_milli = value_milli;
    return req;
}

/* Raccourci : construit la requête puis décide. */
static eclat_decision_t dec(const eclat_policy_t *p, eclat_cap_t cap, int32_t id, int32_t v) {
    eclat_request_t r = make_req(cap, id, v);
    return eclat_decide(p, &r);
}

/* §3 étape 1 : cap hors [0, ECLAT_CAP_COUNT) → CAP_DENIED_UNKNOWN. */
static void test_step1_unknown_cap(void) {
    eclat_policy_t p = make_policy();
    check(dec(&p, (eclat_cap_t)7, -1, 0) == ECLAT_CAP_DENIED_UNKNOWN,
          "ECL-05 step1 cap==COUNT -> UNKNOWN");
    check(dec(&p, (eclat_cap_t)999, -1, 0) == ECLAT_CAP_DENIED_UNKNOWN,
          "ECL-05 step1 cap==999 -> UNKNOWN");
    check(dec(&p, (eclat_cap_t)-1, -1, 0) == ECLAT_CAP_DENIED_UNKNOWN,
          "ECL-05 step1 cap==-1 -> UNKNOWN");
    check(eclat_decide(&p, NULL) == ECLAT_CAP_DENIED_UNKNOWN, "ECL-05 step1 req NULL -> UNKNOWN");
    check(ECLAT_CAP_COUNT == 7, "ECL-01 enum ECLAT_CAP_COUNT == 7 (CONTRACT.md §1)");
}

/* §3 étape 2 : cap dans policy->denies → CAP_DENIED_POLICY (deny gagne sur allow). */
static void test_step2_deny_wins(void) {
    eclat_policy_t p = make_policy();
    p.allows[p.n_allows++] = make_allow(ECLAT_CAP_NET_SEND, -1, -1, -1, -1);
    p.denies[p.n_denies++] = ECLAT_CAP_NET_SEND;
    check(dec(&p, ECLAT_CAP_NET_SEND, -1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step2 deny gagne sur allow -> DENIED_POLICY");
    p.denies[p.n_denies++] = ECLAT_CAP_TIME_NS;
    check(dec(&p, ECLAT_CAP_TIME_NS, -1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step2 deny TIME_NS prime sur étape 3 -> DENIED_POLICY");
    p.denies[p.n_denies++] = ECLAT_CAP_LOG;
    check(dec(&p, ECLAT_CAP_LOG, -1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step2 deny LOG -> DENIED_POLICY");
    p.denies[p.n_denies++] = ECLAT_CAP_READ_SENSOR;
    check(dec(&p, ECLAT_CAP_READ_SENSOR, 1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step2 deny read_sensor -> DENIED_POLICY");
    check(dec(NULL, ECLAT_CAP_LOG, -1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 policy NULL -> refus par défaut DENIED_POLICY");
}

/* §3 étape 3 : TIME_NS et LOG passent SANS règle allow (facilités hôte). */
static void test_step3_host_facilities(void) {
    eclat_policy_t p = make_policy();
    check(dec(&p, ECLAT_CAP_TIME_NS, -1, 0) == ECLAT_CAP_OK, "ECL-05 step3 TIME_NS sans allow -> OK");
    check(dec(&p, ECLAT_CAP_LOG, -1, 0) == ECLAT_CAP_OK, "ECL-05 step3 LOG sans allow -> OK");
}

/* §3 étape 4 : aucune règle allow avec rule.cap == cap → CAP_DENIED_POLICY. */
static void test_step4_no_allow(void) {
    eclat_policy_t p = make_policy();
    p.allows[p.n_allows++] = make_allow(ECLAT_CAP_READ_SENSOR, -1, -1, -1, -1);
    check(dec(&p, ECLAT_CAP_WRITE_ACTUATOR, 3, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step4 write_actuator sans allow -> DENIED_POLICY");
    check(dec(&p, ECLAT_CAP_KV_GET, -1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step4 kv_get sans allow -> DENIED_POLICY");
    check(dec(&p, ECLAT_CAP_KV_PUT, -1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step4 kv_put sans allow -> DENIED_POLICY");
    check(dec(&p, ECLAT_CAP_NET_SEND, 0, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step4 net_send sans allow -> DENIED_POLICY");
    eclat_policy_t empty = make_policy();
    check(dec(&empty, ECLAT_CAP_READ_SENSOR, 1, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step4 politique vide -> DENIED_POLICY");
}

/* §3 étape 5 : id_min != -1 et id hors [id_min, id_max] → CAP_DENIED_RANGE. */
static void test_step5_id_range(void) {
    eclat_policy_t p = make_policy();
    p.allows[p.n_allows++] = make_allow(ECLAT_CAP_READ_SENSOR, 1, 7, -1, -1);
    check(dec(&p, ECLAT_CAP_READ_SENSOR, 0, 0) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step5 id=0 < min -> RANGE");
    check(dec(&p, ECLAT_CAP_READ_SENSOR, 8, 0) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step5 id=8 > max -> RANGE");
    check(dec(&p, ECLAT_CAP_READ_SENSOR, 1, 0) == ECLAT_CAP_OK, "ECL-05 step5 id=min inclusif -> OK");
    check(dec(&p, ECLAT_CAP_READ_SENSOR, 7, 0) == ECLAT_CAP_OK, "ECL-05 step5 id=max inclusif -> OK");
    check(dec(&p, ECLAT_CAP_READ_SENSOR, 4, 0) == ECLAT_CAP_OK, "ECL-05 step5 id au milieu -> OK");
    check(dec(&p, ECLAT_CAP_READ_SENSOR, -1, 0) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step5 id absent (-1) avec contrainte -> RANGE");
    p.allows[p.n_allows++] = make_allow(ECLAT_CAP_KV_GET, -1, -1, -1, -1);
    check(dec(&p, ECLAT_CAP_KV_GET, -1, 0) == ECLAT_CAP_OK, "ECL-05 step5 id_min==-1 -> OK");
    check(dec(&p, ECLAT_CAP_KV_GET, 12345, 0) == ECLAT_CAP_OK,
          "ECL-05 step5 id libre si pas de contrainte -> OK");
}

/* §3 étape 6 : WRITE_ACTUATOR, val_min != -1, value hors plage → CAP_DENIED_RANGE. */
static void test_step6_value_range(void) {
    eclat_policy_t p = make_policy();
    p.allows[p.n_allows++] = make_allow(ECLAT_CAP_WRITE_ACTUATOR, 3, 3, -1000, 1000);
    check(dec(&p, ECLAT_CAP_WRITE_ACTUATOR, 3, -1001) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step6 val < min -> RANGE");
    check(dec(&p, ECLAT_CAP_WRITE_ACTUATOR, 3, 1001) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step6 val > max -> RANGE");
    check(dec(&p, ECLAT_CAP_WRITE_ACTUATOR, 3, -1000) == ECLAT_CAP_OK,
          "ECL-05 step6 val=min inclusif -> OK");
    check(dec(&p, ECLAT_CAP_WRITE_ACTUATOR, 3, 1000) == ECLAT_CAP_OK,
          "ECL-05 step6 val=max inclusif -> OK");
    check(dec(&p, ECLAT_CAP_WRITE_ACTUATOR, 3, 0) == ECLAT_CAP_OK,
          "ECL-05 step7 val au milieu -> OK (étape 7)");
    check(dec(&p, ECLAT_CAP_WRITE_ACTUATOR, 4, 0) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step5 id actionneur hors plage -> RANGE");
    /* La contrainte de valeur ne s'applique qu'à WRITE_ACTUATOR. */
    eclat_policy_t q = make_policy();
    q.allows[q.n_allows++] = make_allow(ECLAT_CAP_KV_PUT, -1, -1, 0, 10);
    check(dec(&q, ECLAT_CAP_KV_PUT, -1, 9999) == ECLAT_CAP_OK,
          "ECL-05 step6 val ignorée hors write_actuator");
    /* val_min == -1 : pas de contrainte de valeur. */
    eclat_policy_t s = make_policy();
    s.allows[s.n_allows++] = make_allow(ECLAT_CAP_WRITE_ACTUATOR, -1, -1, -1, -1);
    check(dec(&s, ECLAT_CAP_WRITE_ACTUATOR, 9, 123456) == ECLAT_CAP_OK,
          "ECL-05 step6 val_min==-1 sans contrainte -> OK");
}

/* ECL-01 : noms des codes (names.c réutilisé, CONTRACT.md §3/§6). */
static void test_names(void) {
    check(strcmp(eclat_decision_name(ECLAT_CAP_OK), "CAP_OK") == 0, "ECL-01 name CAP_OK");
    check(strcmp(eclat_decision_name(ECLAT_CAP_DENIED_POLICY), "CAP_DENIED_POLICY") == 0,
          "ECL-01 name CAP_DENIED_POLICY");
    check(strcmp(eclat_decision_name(ECLAT_CAP_DENIED_RANGE), "CAP_DENIED_RANGE") == 0,
          "ECL-01 name CAP_DENIED_RANGE");
    check(strcmp(eclat_decision_name(ECLAT_CAP_DENIED_UNKNOWN), "CAP_DENIED_UNKNOWN") == 0,
          "ECL-01 name CAP_DENIED_UNKNOWN");
    check(strcmp(eclat_cap_name(ECLAT_CAP_WRITE_ACTUATOR), "write_actuator") == 0,
          "ECL-01 cap name write_actuator");
    eclat_cap_t cap;
    check(eclat_cap_from_name("net_send", 8, &cap) == 0 && cap == ECLAT_CAP_NET_SEND,
          "ECL-01 cap_from_name net_send");
}

/* Compléments : ordre gelé des énumérés (ECL-01, CONTRACT.md §3), première règle allow
 * gagnante (§3 étapes 4-6), payload_len ignoré par decide. */
static void test_extra(void) {
    check(ECLAT_CAP_OK == 0 && ECLAT_CAP_DENIED_POLICY == 1 && ECLAT_CAP_DENIED_RANGE == 2 &&
              ECLAT_CAP_DENIED_BUDGET == 3 && ECLAT_CAP_DENIED_UNKNOWN == 4,
          "ECL-01 ordre gelé des codes (§3)");
    eclat_policy_t p = make_policy();
    p.allows[p.n_allows++] = make_allow(ECLAT_CAP_READ_SENSOR, 1, 7, -1, -1);
    p.allows[p.n_allows++] = make_allow(ECLAT_CAP_READ_SENSOR, 8, 9, -1, -1);
    check(dec(&p, ECLAT_CAP_READ_SENSOR, 8, 0) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step4-5 première règle allow gagnante");
    eclat_request_t r = make_req(ECLAT_CAP_READ_SENSOR, 3, 0);
    r.payload_len = 999999;
    check(eclat_decide(&p, &r) == ECLAT_CAP_OK, "ECL-05 step7 payload_len ignoré par decide");
    eclat_policy_t q = make_policy();
    q.allows[q.n_allows++] = make_allow(ECLAT_CAP_NET_SEND, 0, 6, -1, -1);
    q.denies[q.n_denies++] = ECLAT_CAP_KV_PUT;
    check(dec(&q, ECLAT_CAP_NET_SEND, 6, 0) == ECLAT_CAP_OK, "ECL-05 step7 net_send dans [0,6] -> OK");
    check(dec(&q, ECLAT_CAP_NET_SEND, 7, 0) == ECLAT_CAP_DENIED_RANGE,
          "ECL-05 step5 net_send id=7 -> RANGE");
    check(dec(&q, ECLAT_CAP_KV_PUT, 0, 0) == ECLAT_CAP_DENIED_POLICY,
          "ECL-05 step2 kv_put deny -> POLICY");
}

int main(void) {
    test_step1_unknown_cap();
    test_step2_deny_wins();
    test_step3_host_facilities();
    test_step4_no_allow();
    test_step5_id_range();
    test_step6_value_range();
    test_names();
    test_extra();
    if (failures != 0) {
        printf("KO %d/%d checks\n", failures, checks);
        return 1;
    }
    printf("OK %d checks\n", checks);
    return 0;
}
