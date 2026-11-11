// SPDX-License-Identifier: Apache-2.0
/* Éclat — tests unitaires de la politique (lot L1).
 * ECL-02 : la politique est analysée et validée ; politique invalide refusée avec champ et
 * cause ; tout refus par défaut.
 * Couvre : parse de politiques valides, banque de 30 politiques fautives, limites
 * (32 allow / 16 deny, name 64), et validation sémantique. */
#include "eclat/cap_policy.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static void expect(int cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static int parse_str(const char *toml, eclat_policy_t *out, eclat_policy_error_t *err) {
    memset(out, 0, sizeof(*out));
    memset(err, 0, sizeof(*err));
    return eclat_policy_parse((const uint8_t *)toml, strlen(toml), out, err);
}

static void expect_ok(const char *toml, const char *what) {
    eclat_policy_t p;
    eclat_policy_error_t e;
    expect(parse_str(toml, &p, &e) == 0, what);
}

/* Cas fautif : parse doit échouer ET remplir un champ d'erreur. */
static void expect_bad(const char *toml, const char *what) {
    eclat_policy_t p;
    eclat_policy_error_t e;
    int rc = parse_str(toml, &p, &e);
    checks++;
    if (rc == 0) {
        failures++;
        printf("FAIL (attendu refus): %s\n", what);
    } else if (e.field[0] == '\0') {
        failures++;
        printf("FAIL (champ vide): %s\n", what);
    }
}

#define SHA64 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define MOD "[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n"

static void test_valid_policies(void) {
    const char *full =
        "[module]\nname = \"diag-partner\"\nsha256 = \"" SHA64 "\"\n"
        "cpu_budget_ms_per_s = 50\nmem_max_kib = 2048\ncalls_per_s = 200\n"
        "[[allow]]\ncap = \"read_sensor\"\nids = [1, 2, 7]\n"
        "[[allow]]\ncap = \"write_actuator\"\nids = [3]\nvalue_milli_range = [-1000, 1000]\n"
        "[[deny]]\ncap = \"net_send\"\n";
    eclat_policy_t p;
    eclat_policy_error_t e;
    expect(parse_str(full, &p, &e) == 0, "ECL-02 politique valide complète parse");
    expect(p.name_len == 12 && memcmp(p.name, "diag-partner", 12) == 0, "ECL-02 name extrait");
    expect(p.sha256[0] == 0xaa && p.sha256[31] == 0xaa, "ECL-02 sha256 décodé");
    expect(p.cpu_budget_ms_per_s == 50, "ECL-02 cpu_budget");
    expect(p.mem_max_kib == 2048, "ECL-02 mem_max");
    expect(p.calls_per_s == 200, "ECL-02 calls_per_s");
    expect(p.n_allows == 2, "ECL-02 deux allow");
    expect(p.allows[0].cap == ECLAT_CAP_READ_SENSOR, "ECL-02 allow[0] cap");
    expect(p.allows[0].id_min == 1 && p.allows[0].id_max == 7, "ECL-02 allow[0] ids plage");
    expect(p.allows[1].cap == ECLAT_CAP_WRITE_ACTUATOR, "ECL-02 allow[1] cap");
    expect(p.allows[1].val_min == -1000 && p.allows[1].val_max == 1000, "ECL-02 allow[1] value range");
    expect(p.n_denies == 1 && p.denies[0] == ECLAT_CAP_NET_SEND, "ECL-02 deny net_send");

    expect_ok(MOD, "ECL-02 politique minimale sans allow");
    eclat_policy_t p2;
    eclat_policy_error_t e2;
    parse_str(MOD, &p2, &e2);
    expect(p2.n_allows == 0, "ECL-02 politique sans allow → n_allows 0");
    expect_ok("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"kv_get\"\nids = 5\n",
              "ECL-02 ids entier seul");
    expect_ok("# commentaire\n[module]\nname = \"m\" # inline\nsha256 = \"" SHA64 "\"\n\n",
              "ECL-02 commentaires tolérés");
}

static void test_faulty_bank(void) {
    /* ECL-02 : banque de 30 politiques fautives, chacune refusée avec champ + cause. */
    expect_bad("", "faute 01: document vide");
    expect_bad("[module]\nname = \"m\"\n", "faute 02: sha256 absent");
    expect_bad("name = \"m\"\nsha256 = \"" SHA64 "\"\n", "faute 03: section module absente");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"zz\"\n", "faute 04: sha256 non hex");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"aa\"\n", "faute 05: sha256 trop court");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "aa\"\n", "faute 06: sha256 trop long");
    expect_bad("[module]\nname = \"m\"\nsha256 = 12345\n", "faute 07: sha256 pas une chaîne");
    expect_bad("[module]\nsha256 = \"" SHA64 "\"\n", "faute 08: name absent");
    expect_bad("[module]\nname = \"\"\nsha256 = \"" SHA64 "\"\n", "faute 09: name vide");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\ncpu_budget_ms_per_s = -1\n",
               "faute 10: budget négatif");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\ncpu_budget_ms_per_s = abc\n",
               "faute 11: budget non entier");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\nmem_max_kib = 99999999999999\n",
               "faute 12: mem_max hors plage");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"inconnu\"\n",
               "faute 13: cap inconnu dans allow");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[deny]]\ncap = \"pas_un_cap\"\n",
               "faute 14: cap inconnu dans deny");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\n", "faute 15: allow sans cap");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[deny]]\n", "faute 16: deny sans cap");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"read_sensor\"\nids = [1, 2\n",
               "faute 17: ids tableau non fermé");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"write_actuator\"\nvalue_milli_range = [10]\n",
               "faute 18: value_milli_range un seul élément");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"write_actuator\"\nvalue_milli_range = [1,2,3]\n",
               "faute 19: value_milli_range trop d'éléments");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"read_sensor\"\nids = []\n",
               "faute 20: ids tableau vide");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"read_sensor\"\nids = [a]\n",
               "faute 21: ids élément non entier");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\ncle sans egal\n", "faute 22: ligne sans '='");
    expect_bad("[module\nname = \"m\"\nsha256 = \"" SHA64 "\"\n", "faute 23: header module non fermé");
    expect_bad("[[allow]]\ncap = \"log\"\n", "faute 24: allow avant module");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[section_inconnue]\n", "faute 25: section inconnue");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"read_sensor\"\nids = \"notarray\"\n",
               "faute 26: ids chaîne au lieu d'entier/tableau");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"read_sensor\"\nids = 99999999999\n",
               "faute 27: ids entier hors plage");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap\n", "faute 28: cap sans valeur");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = read_sensor\n",
               "faute 29: cap sans guillemets");
    expect_bad("[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n[[allow]]\ncap = \"read_sensor\"\nvalue_milli_range = [5, 1]\n",
               "faute 30: value_milli_range min>max");
}

static void build_allows(char *buf, size_t cap, int n) {
    int off = snprintf(buf, cap, "[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n");
    for (int i = 0; i < n; i++)
        off += snprintf(buf + off, cap - (size_t)off, "[[allow]]\ncap = \"log\"\n");
}

static void build_denies(char *buf, size_t cap, int n) {
    int off = snprintf(buf, cap, "[module]\nname = \"m\"\nsha256 = \"" SHA64 "\"\n");
    for (int i = 0; i < n; i++)
        off += snprintf(buf + off, cap - (size_t)off, "[[deny]]\ncap = \"log\"\n");
}

static void test_limits(void) {
    /* ECL-02 : limites de taille des tableaux et du nom. */
    char buf[8192];
    build_allows(buf, sizeof(buf), 32);
    expect_ok(buf, "ECL-02 32 allow accepté");
    build_allows(buf, sizeof(buf), 33);
    expect_bad(buf, "ECL-02 33 allow refusé");
    build_denies(buf, sizeof(buf), 16);
    expect_ok(buf, "ECL-02 16 deny accepté");
    build_denies(buf, sizeof(buf), 17);
    expect_bad(buf, "ECL-02 17 deny refusé");
    char name[66];
    memset(name, 'x', 64);
    name[64] = '\0';
    snprintf(buf, sizeof(buf), "[module]\nname = \"%s\"\nsha256 = \"" SHA64 "\"\n", name);
    expect_ok(buf, "ECL-02 name 64 caractères accepté");
    name[64] = 'x';
    name[65] = '\0';
    snprintf(buf, sizeof(buf), "[module]\nname = \"%s\"\nsha256 = \"" SHA64 "\"\n", name);
    expect_bad(buf, "ECL-02 name 65 caractères refusé");
}

static void test_validate(void) {
    /* ECL-02 : validate sur une politique construite en C. */
    eclat_policy_t p;
    eclat_policy_error_t e;
    memset(&p, 0, sizeof(p));
    memcpy(p.name, "m", 1);
    p.name_len = 1;
    expect(eclat_policy_validate(&p, &e) == 0, "ECL-02 validate politique minimale OK");
    p.n_allows = 1;
    p.allows[0].cap = ECLAT_CAP_COUNT;
    p.allows[0].has_ids = 0;
    p.allows[0].has_value_range = 0;
    expect(eclat_policy_validate(&p, &e) != 0, "ECL-02 validate cap invalide refusé");
    p.allows[0].cap = ECLAT_CAP_READ_SENSOR;
    p.allows[0].has_ids = 1;
    p.allows[0].id_min = 5;
    p.allows[0].id_max = 1;
    expect(eclat_policy_validate(&p, &e) != 0, "ECL-02 validate id_min>id_max refusé");
    p.allows[0].has_ids = 0;
    p.allows[0].has_value_range = 1;
    p.allows[0].val_min = 5;
    p.allows[0].val_max = 1;
    expect(eclat_policy_validate(&p, &e) != 0, "ECL-02 validate val_min>val_max refusé");
}

int main(void) {
    test_valid_policies();
    test_faulty_bank();
    test_limits();
    test_validate();
    if (failures) {
        printf("FAIL %d/%d checks\n", failures, checks);
        return 1;
    }
    printf("OK %d checks\n", checks);
    return 0;
}