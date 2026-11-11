# SPDX-License-Identifier: Apache-2.0
"""Éclat — tests unitaires du jumeau Python (lot L3, ECL-03).

Couvre : `capsim.decide` (les 7 étapes de CONTRACT.md §3), `capsim.Budget` (§4) et la
validation de `capsim.parse_policy` (§2), dont au moins 10 politiques fautives.
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capsim  # noqa: E402

SHA = "a" * 64


def policy(allows=None, denies=None):
    return {"name": "m", "sha256": bytes.fromhex(SHA), "allows": allows or [],
            "denies": denies or [], "cpu_budget_ms_per_s": 0, "mem_max_kib": 0,
            "calls_per_s": 0}


def allow(cap, ids=None, vr=None):
    return {"cap": cap, "ids": ids, "value_milli_range": vr}


class TestDecide(unittest.TestCase):
    """ECL-03 : les 7 étapes de décision, ordre strict."""

    def test_etape1_cap_inconnu(self):
        self.assertEqual(capsim.decide(policy(), "bogus", 0, 0), "CAP_DENIED_UNKNOWN")

    def test_etape2_deny_gagne(self):
        p = policy(allows=[allow("read_sensor", ids=(1, 7))], denies=["read_sensor"])
        self.assertEqual(capsim.decide(p, "read_sensor", 1, 0), "CAP_DENIED_POLICY")

    def test_etape3_time_log_non_gates(self):
        p = policy()
        self.assertEqual(capsim.decide(p, "time_ns", 0, 0), "CAP_OK")
        self.assertEqual(capsim.decide(p, "log", 0, 0), "CAP_OK")

    def test_etape4_pas_d_allow(self):
        self.assertEqual(capsim.decide(policy(), "net_send", 0, 0), "CAP_DENIED_POLICY")

    def test_etape5_id_hors_plage(self):
        p = policy(allows=[allow("read_sensor", ids=(1, 7))])
        self.assertEqual(capsim.decide(p, "read_sensor", 1, 0), "CAP_OK")
        self.assertEqual(capsim.decide(p, "read_sensor", 9, 0), "CAP_DENIED_RANGE")

    def test_etape6_value_hors_plage(self):
        p = policy(allows=[allow("write_actuator", ids=(3, 3), vr=(-1000, 1000))])
        self.assertEqual(capsim.decide(p, "write_actuator", 3, 500), "CAP_OK")
        self.assertEqual(capsim.decide(p, "write_actuator", 3, 5000), "CAP_DENIED_RANGE")

    def test_etape7_ok(self):
        p = policy(allows=[allow("kv_get")])
        self.assertEqual(capsim.decide(p, "kv_get", -1, 0), "CAP_OK")

    def test_ids_sans_contrainte(self):
        p = policy(allows=[allow("read_sensor")])  # ids None → pas de contrainte
        self.assertEqual(capsim.decide(p, "read_sensor", 999, 0), "CAP_OK")


class TestBudget(unittest.TestCase):
    """ECL-03 : budget CPU + quota, fenêtre glissante (§4)."""

    def test_dans_le_budget(self):
        b = capsim.Budget(cpu_budget_ms_per_s=50, calls_per_s=200)
        self.assertEqual(b.charge(0, 10_000_000, 1), 0)

    def test_cpu_depasse(self):
        b = capsim.Budget(cpu_budget_ms_per_s=10)
        self.assertEqual(b.charge(0, 20_000_000, 1), 1)

    def test_quota_depasse(self):
        b = capsim.Budget(calls_per_s=2)
        b.charge(0, 0, 1)
        b.charge(0, 0, 1)
        self.assertEqual(b.charge(0, 0, 1), 2)

    def test_quota_prime_sur_cpu(self):
        b = capsim.Budget(cpu_budget_ms_per_s=1, calls_per_s=1)
        self.assertEqual(b.charge(0, 50_000_000, 5), 2)

    def test_illimite(self):
        b = capsim.Budget(0, 0)
        self.assertEqual(b.charge(0, 10**12, 10**6), 0)

    def test_fenetre_repart(self):
        b = capsim.Budget(calls_per_s=1)
        self.assertEqual(b.charge(0, 0, 1), 0)
        self.assertEqual(b.charge(0, 0, 1), 2)  # latched dans la fenêtre
        self.assertEqual(b.charge(1_000_000_000, 0, 1), 0)  # nouvelle fenêtre

    def test_latched(self):
        b = capsim.Budget(cpu_budget_ms_per_s=5)
        self.assertEqual(b.charge(0, 10_000_000, 1), 1)
        self.assertEqual(b.charge(0, 0, 1), 1)  # reste levé dans la fenêtre


class TestParseValidation(unittest.TestCase):
    """ECL-03 : validation de politique, ≥ 10 cas fautifs."""

    def _bad(self, toml, field=None):
        pol, err = capsim.parse_policy(toml)
        self.assertIsNone(pol)
        self.assertIsNotNone(err)
        if field:
            self.assertEqual(err[0], field)

    def test_valide(self):
        toml = '[module]\nname = "m"\nsha256 = "%s"\n[[allow]]\ncap = "read_sensor"\nids = [1,2,7]\n' % SHA
        pol, err = capsim.parse_policy(toml)
        self.assertIsNone(err)
        self.assertEqual(pol["allows"][0]["ids"], (1, 7))

    def test_sha_absent(self):
        self._bad('[module]\nname = "m"\n', "sha256")

    def test_sha_court(self):
        self._bad('[module]\nname = "m"\nsha256 = "aa"\n', "sha256")

    def test_sha_non_hex(self):
        self._bad('[module]\nname = "m"\nsha256 = "%s"\n' % ("z" * 64), "sha256")

    def test_name_absent(self):
        self._bad('[module]\nsha256 = "%s"\n' % SHA, "name")

    def test_name_trop_long(self):
        self._bad('[module]\nname = "%s"\nsha256 = "%s"\n' % ("x" * 65, SHA), "name")

    def test_cap_inconnu_allow(self):
        self._bad('[module]\nname = "m"\nsha256 = "%s"\n[[allow]]\ncap = "x"\n' % SHA, "cap")

    def test_cap_inconnu_deny(self):
        self._bad('[module]\nname = "m"\nsha256 = "%s"\n[[deny]]\ncap = "x"\n' % SHA, "cap")

    def test_budget_negatif(self):
        self._bad('[module]\nname = "m"\nsha256 = "%s"\ncalls_per_s = -1\n' % SHA, "calls_per_s")

    def test_value_range_min_gt_max(self):
        self._bad('[module]\nname = "m"\nsha256 = "%s"\n[[allow]]\ncap = "write_actuator"\n'
                  'value_milli_range = [5, 1]\n' % SHA, "value_milli_range")

    def test_ids_vide(self):
        self._bad('[module]\nname = "m"\nsha256 = "%s"\n[[allow]]\ncap = "read_sensor"\nids = []\n'
                  % SHA, "ids")

    def test_trop_d_allow(self):
        body = "".join('[[allow]]\ncap = "log"\n' for _ in range(33))
        self._bad('[module]\nname = "m"\nsha256 = "%s"\n%s' % (SHA, body), "allow")

    def test_module_absent(self):
        self._bad('name = "m"\nsha256 = "%s"\n' % SHA, "module")


if __name__ == "__main__":
    unittest.main(verbosity=2)