# SPDX-License-Identifier: Apache-2.0
"""Éclat — tests du manifeste de module signé (ECL-07, chaîne d'approvisionnement).

Couvre : création, signature correcte, manifeste falsifié, empreinte qui ne
correspond plus au .wasm, capacité inconnue, budget WCRT invalide, champs
obligatoires, mauvaise clé et non-mutation de l'entrée.
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import keys  # noqa: E402
import manifest  # noqa: E402

MODULE = b"\x00asm\x01\x00\x00\x00eclat"
AUTRE_MODULE = b"\x00asm\x01\x00\x00\x00autre"
KEY = b"cle-manifeste"
OTHER = b"autre-cle"
TEST_ENV = "ECLAT_TEST_MANIFEST_KEY"


def build(caps=None, wcrt_us=250, module=MODULE, name="diag", version="0.1"):
    return manifest.build_manifest(
        module, name, caps if caps is not None else ["log", "time_ns"], wcrt_us, version)


def signed(caps=None, wcrt_us=250, module=MODULE, key=KEY, name="diag"):
    return manifest.sign_manifest(build(caps, wcrt_us, module, name), key)


class TestCreation(unittest.TestCase):
    """ECL-07 : construction du manifeste."""

    def test_ECL07_champs_du_manifeste(self):
        m = build()
        self.assertEqual(m["name"], "diag")
        self.assertEqual(m["version"], "0.1")
        self.assertEqual(m["size"], len(MODULE))
        self.assertIsNone(m.get("signature", None))

    def test_ECL07_caps_triees(self):
        self.assertEqual(build(caps=["net_send", "kv_get", "log"])["caps"],
                         ["kv_get", "log", "net_send"])

    def test_ECL07_empreinte_et_taille(self):
        import hashlib
        m = build()
        self.assertEqual(m["sha256"], hashlib.sha256(MODULE).hexdigest())
        self.assertTrue(manifest.is_hex64(m["sha256"]))

    def test_ECL07_wcrt_inconnu(self):
        self.assertIsNone(build(wcrt_us=None)["wcrt_us"])

    def test_ECL07_version_personnalisee(self):
        self.assertEqual(build(version="2.3")["version"], "2.3")

    def test_ECL07_sept_capacites_du_contrat(self):
        self.assertEqual(len(manifest.CAPS), 7)
        self.assertTrue(manifest.verify_manifest(signed(caps=list(manifest.CAPS)), KEY)[0])


class TestSignature(unittest.TestCase):
    """ECL-07 : signature HMAC et non-mutation."""

    def test_ECL07_signature_correcte(self):
        m = signed()
        self.assertTrue(manifest.is_hex64(m["signature"]))
        ok, reason = manifest.verify_manifest(m, KEY)
        self.assertTrue(ok)
        self.assertEqual(reason, "OK")

    def test_ECL07_sign_manifest_ne_mute_pas_l_entree(self):
        base = build()
        signed_m = manifest.sign_manifest(base, KEY)
        self.assertNotIn("signature", base)
        self.assertIsNot(signed_m, base)

    def test_ECL07_signature_deterministe(self):
        self.assertEqual(signed()["signature"], signed()["signature"])

    def test_ECL07_signature_depend_de_la_cle(self):
        self.assertNotEqual(signed()["signature"], signed(key=OTHER)["signature"])

    def test_ECL07_ordre_des_champs_indifferent(self):
        base = signed()
        reordered = {k: base[k] for k in reversed(list(base))}
        self.assertTrue(manifest.verify_manifest(reordered, KEY)[0])

    def test_ECL07_mauvaise_cle_refusee(self):
        ok, reason = manifest.verify_manifest(signed(), OTHER)
        self.assertFalse(ok)
        self.assertIn("signature invalide", reason)

    def test_ECL07_signature_absente_refusee(self):
        ok, reason = manifest.verify_manifest(build(), KEY)
        self.assertFalse(ok)
        self.assertIn("signature", reason)


class TestFalsification(unittest.TestCase):
    """ECL-07 : un manifeste altéré est détecté."""

    def test_ECL07_capacites_falsifiees(self):
        m = signed(caps=["log"])
        m["caps"] = ["log", "net_send"]
        ok, reason = manifest.verify_manifest(m, KEY)
        self.assertFalse(ok)
        self.assertIn("signature invalide", reason)

    def test_ECL07_empreinte_falsifiee(self):
        m = signed()
        m["sha256"] = "b" * 64
        self.assertFalse(manifest.verify_manifest(m, KEY)[0])

    def test_ECL07_nom_falsifie(self):
        m = signed()
        m["name"] = "cheval-de-troie"
        self.assertFalse(manifest.verify_manifest(m, KEY)[0])

    def test_ECL07_wcrt_falsifie(self):
        m = signed(wcrt_us=250)
        m["wcrt_us"] = 10
        self.assertFalse(manifest.verify_manifest(m, KEY)[0])

    def test_ECL07_empreinte_ne_correspond_plus_au_wasm(self):
        m = signed()
        ok, reason = manifest.verify_manifest(m, KEY, AUTRE_MODULE)
        self.assertFalse(ok)
        self.assertIn("empreinte sha256", reason)

    def test_ECL07_module_conforme_accepte(self):
        m = signed()
        self.assertTrue(manifest.verify_manifest(m, KEY, MODULE)[0])

    def test_ECL07_champ_supprime(self):
        m = signed()
        del m["version"]
        ok, reason = manifest.verify_manifest(m, KEY)
        self.assertFalse(ok)
        self.assertIn("champ manquant", reason)


class TestValidation(unittest.TestCase):
    """ECL-07 : validation des champs indépendamment de la signature."""

    def _refuse(self, mutate, attendu):
        m = signed()
        mutate(m)
        m = manifest.sign_manifest(m, KEY)  # re-signé : seule la validation peut le refuser
        ok, reason = manifest.verify_manifest(m, KEY)
        self.assertFalse(ok)
        self.assertIn(attendu, reason)

    def test_ECL07_capacite_inconnue_refusee(self):
        self._refuse(lambda m: m.update(caps=["log", "root_shell"]), "capacité inconnue")

    def test_ECL07_capacite_mal_orthographiee(self):
        self._refuse(lambda m: m.update(caps=["readsensor"]), "capacité inconnue")

    def test_ECL07_wcrt_nul_refuse(self):
        self._refuse(lambda m: m.update(wcrt_us=0), "wcrt_us")

    def test_ECL07_wcrt_negatif_refuse(self):
        self._refuse(lambda m: m.update(wcrt_us=-5), "wcrt_us")

    def test_ECL07_wcrt_non_entier_refuse(self):
        self._refuse(lambda m: m.update(wcrt_us="250"), "wcrt_us")

    def test_ECL07_wcrt_absent_accepte(self):
        m = manifest.sign_manifest(build(wcrt_us=None), KEY)
        self.assertTrue(manifest.verify_manifest(m, KEY)[0])

    def test_ECL07_sha256_court_refuse(self):
        m = signed()
        m["sha256"] = "aa"
        ok, reason = manifest.verify_manifest(m, KEY)
        self.assertFalse(ok)
        self.assertIn("sha256", reason)

    def test_ECL07_size_negatif_refuse(self):
        self._refuse(lambda m: m.update(size=-1), "size")

    def test_ECL07_caps_non_liste_refuse(self):
        self._refuse(lambda m: m.update(caps="log"), "caps")

    def test_ECL07_name_vide_refuse(self):
        self._refuse(lambda m: m.update(name=""), "name")

    def test_ECL07_manifeste_non_objet_refuse(self):
        ok, reason = manifest.verify_manifest(["pas", "un", "objet"], KEY)
        self.assertFalse(ok)
        self.assertIn("manifeste invalide", reason)

    def test_ECL07_signature_mal_formee(self):
        m = signed()
        m["signature"] = "court"
        ok, reason = manifest.verify_manifest(m, KEY)
        self.assertFalse(ok)
        self.assertIn("signature", reason)


class TestClePartagee(unittest.TestCase):
    """ECL-07 : la clé du manifeste vient de `host/keys.py` (zéro duplication)."""

    def setUp(self):
        self._saved = dict(os.environ)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._saved)

    def test_ECL07_cle_depuis_environnement(self):
        os.environ[TEST_ENV] = "cle-manifeste\n"
        key = keys.load_key(TEST_ENV)
        self.assertTrue(manifest.verify_manifest(signed(key=key), key)[0])

    def test_ECL07_cle_absente_refusee(self):
        os.environ.pop(TEST_ENV, None)
        with self.assertRaises(KeyError):
            keys.load_key(TEST_ENV)

    def test_ECL07_variable_standard(self):
        self.assertEqual(keys.MANIFEST_KEY_ENV, "ECLAT_MANIFEST_KEY")


if __name__ == "__main__":
    unittest.main(verbosity=2)
