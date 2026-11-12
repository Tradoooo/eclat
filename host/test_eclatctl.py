# SPDX-License-Identifier: Apache-2.0
"""Éclat — tests de la console `eclatctl` (idée D44, ECL-02/ECL-03/ECL-05).

Vérifie les quatre commandes et, surtout, la **non-divergence** : la décision annoncée par
`explain` est toujours celle que `capsim.decide` (donc le contrat) produit.
"""
import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capsim  # noqa: E402
import eclatctl  # noqa: E402

SHA = "ab" * 32
POLICY = """[module]
name = "diag-partner"
sha256 = "%s"
cpu_budget_ms_per_s = 50
calls_per_s = 200
[[allow]]
cap = "read_sensor"
ids = [1, 7]
[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]
[[deny]]
cap = "net_send"
""" % SHA


def run(argv):
    """Exécute la CLI en capturant stdout/stderr. Retourne (rc, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = eclatctl.main(["eclatctl"] + argv)
    return rc, out.getvalue(), err.getvalue()


class CliTestCase(unittest.TestCase):
    def setUp(self):
        fd, self.policy_path = tempfile.mkstemp(suffix=".toml")
        with os.fdopen(fd, "w", encoding="utf-8") as fp:
            fp.write(POLICY)
        fd, self.bad_path = tempfile.mkstemp(suffix=".toml")
        with os.fdopen(fd, "w", encoding="utf-8") as fp:
            fp.write('[module]\nname = "m"\n')  # sha256 absent
        fd, self.req_path = tempfile.mkstemp(suffix=".txt")
        with os.fdopen(fd, "w", encoding="utf-8") as fp:
            fp.write("read_sensor 1 -\nread_sensor 99 -\nnet_send - -\ntime_ns - -\n"
                     "write_actuator 3 500\nwrite_actuator 3 9999\nbogus - -\n")

    def tearDown(self):
        for path in (self.policy_path, self.bad_path, self.req_path):
            if os.path.exists(path):
                os.unlink(path)


class TestPolicyCmd(CliTestCase):
    def test_ECL02_valide(self):
        rc, out, _ = run(["policy", self.policy_path])
        self.assertEqual(rc, 0)
        self.assertIn("diag-partner", out)
        self.assertIn(SHA[:16], out)
        self.assertIn("read_sensor", out)
        self.assertIn("net_send", out)

    def test_ECL02_invalide(self):
        rc, _, err = run(["policy", self.bad_path])
        self.assertEqual(rc, 2)
        self.assertTrue(err.startswith("ERROR:sha256:"), err)

    def test_ECL02_fichier_absent(self):
        rc, _, err = run(["policy", "/tmp/n-existe-pas-eclat.toml"])
        self.assertEqual(rc, 2)
        self.assertIn("ERROR:", err)

    def test_ECL02_budgets_illimites(self):
        fd, path = tempfile.mkstemp(suffix=".toml")
        with os.fdopen(fd, "w", encoding="utf-8") as fp:
            fp.write('[module]\nname = "m"\nsha256 = "%s"\n' % SHA)
        try:
            rc, out, _ = run(["policy", path])
            self.assertEqual(rc, 0)
            self.assertIn("illimité", out)
        finally:
            os.unlink(path)


class TestSimulateCmd(CliTestCase):
    def test_ECL05_decisions(self):
        rc, out, _ = run(["simulate", self.policy_path, self.req_path])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_OK", out)
        self.assertIn("CAP_DENIED_RANGE", out)
        self.assertIn("CAP_DENIED_POLICY", out)
        self.assertIn("CAP_DENIED_UNKNOWN", out)

    def test_ECL05_comptage(self):
        rc, out, _ = run(["simulate", self.policy_path, self.req_path])
        self.assertEqual(rc, 0)
        self.assertIn("7 requêtes", out)
        self.assertIn("4 refus", out)

    def test_ECL02_politique_invalide(self):
        rc, _, err = run(["simulate", self.bad_path, self.req_path])
        self.assertEqual(rc, 2)
        self.assertIn("ERROR:", err)

    def test_ECL02_requetes_absentes(self):
        rc, _, err = run(["simulate", self.policy_path, "/tmp/n-existe-pas-eclat.txt"])
        self.assertEqual(rc, 2)
        self.assertIn("ERROR:requests:", err)


class TestExplainCmd(CliTestCase):
    def test_ECL05_refus_par_politique(self):
        rc, out, _ = run(["explain", self.policy_path, "kv_get"])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_DENIED_POLICY", out)
        self.assertIn("étape 4", out)

    def test_ECL05_refus_par_plage_id(self):
        rc, out, _ = run(["explain", self.policy_path, "read_sensor", "99"])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_DENIED_RANGE", out)
        self.assertIn("étape 5", out)
        self.assertIn("hors [1, 7]", out)

    def test_ECL05_refus_par_plage_valeur(self):
        rc, out, _ = run(["explain", self.policy_path, "write_actuator", "3", "9999"])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_DENIED_RANGE", out)
        self.assertIn("étape 6", out)

    def test_ECL05_deny_gagne(self):
        rc, out, _ = run(["explain", self.policy_path, "net_send"])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_DENIED_POLICY", out)
        self.assertIn("étape 2", out)

    def test_ECL05_cap_inconnu(self):
        rc, out, _ = run(["explain", self.policy_path, "bogus"])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_DENIED_UNKNOWN", out)
        self.assertIn("étape 1", out)

    def test_ECL05_facilite_hote(self):
        rc, out, _ = run(["explain", self.policy_path, "time_ns"])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_OK", out)
        self.assertIn("étape 3", out)

    def test_ECL05_autorise(self):
        rc, out, _ = run(["explain", self.policy_path, "read_sensor", "4"])
        self.assertEqual(rc, 0)
        self.assertIn("CAP_OK", out)
        self.assertIn("étape 7", out)

    def test_ECL03_absent_en_argument(self):
        """« - » en argument doit donner id=-1 et valeur=0, comme le harnais C (§6)."""
        rc, out, _ = run(["explain", self.policy_path, "read_sensor", "-", "-"])
        self.assertEqual(rc, 0)
        self.assertIn("id=-1 valeur=0", out)

    def test_ECL03_non_divergence_avec_le_contrat(self):
        """La trace ne doit JAMAIS annoncer autre chose que capsim.decide."""
        policy, err = capsim.parse_policy(POLICY)
        self.assertIsNone(err)
        for cap in list(capsim.CAPS) + ["bogus", ""]:
            for id_ in (-5, -1, 0, 1, 3, 7, 8, 99):
                for val in (-5000, -1000, 0, 1000, 5000):
                    rc, out, _err = run(["explain", self.policy_path, cap, str(id_), str(val)])
                    attendu = capsim.decide(policy, cap, id_, val)
                    self.assertEqual(rc, 0)
                    self.assertIn("décision : %s" % attendu, out,
                                  "divergence explain/decide cap=%r id=%d val=%d" % (cap, id_, val))


class TestBudgetCmd(unittest.TestCase):
    def test_ECL06_dans_le_budget(self):
        rc, out, _ = run(["budget", "50", "200", "0", "10000000", "1"])
        self.assertEqual(rc, 0)
        self.assertIn("dans le budget", out)

    def test_ECL06_cpu_depasse(self):
        rc, out, _ = run(["budget", "10", "0", "0", "20000000", "1"])
        self.assertEqual(rc, 0)
        self.assertIn("BUDGET CPU DÉPASSÉ", out)

    def test_ECL06_quota_depasse(self):
        rc, out, _ = run(["budget", "0", "2", "0", "0", "5"])
        self.assertEqual(rc, 0)
        self.assertIn("QUOTA D'APPELS DÉPASSÉ", out)


class TestHelpers(unittest.TestCase):
    def test_parse_request_absents(self):
        self.assertEqual(eclatctl._parse_request("read_sensor - -"), ("read_sensor", -1, 0))

    def test_parse_request_ligne_vide(self):
        self.assertIsNone(eclatctl._parse_request("   \n"))

    def test_parse_request_complet(self):
        self.assertEqual(eclatctl._parse_request("write_actuator 3 -500"),
                         ("write_actuator", 3, -500))

    def test_fmt(self):
        self.assertEqual(eclatctl._fmt(None), "libre")
        self.assertEqual(eclatctl._fmt((1, 7)), "[1, 7]")

    def test_convertisseur_absent(self):
        self.assertEqual(eclatctl._absent(-1)("-"), -1)
        self.assertEqual(eclatctl._absent(0)("-"), 0)
        self.assertEqual(eclatctl._absent(-1)("7"), 7)


if __name__ == "__main__":
    unittest.main(verbosity=2)