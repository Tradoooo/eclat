# SPDX-License-Identifier: Apache-2.0
"""Éclat — tests du journal d'audit à hachage enchaîné (ECL-21).

Couvre : chaîne vide, ajout de lignes, détection d'une ligne modifiée, d'une ligne
supprimée, d'une ligne réordonnée, d'une mauvaise clé, forme des lignes,
sérialisation canonique, export/import JSONL et chargement de la clé partagée
(`host/keys.py`, unique lecteur de clé du projet).
"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audit  # noqa: E402
import keys  # noqa: E402

KEY = b"secret"
OTHER = b"autre-cle"
ZERO = "0" * 64
TEST_ENV = "ECLAT_TEST_KEY"


def build_chain(n=3, key=KEY, seq=0):
    """Chaîne de `n` décisions, numérotées à partir de `seq`."""
    chain = audit.AuditChain(key, seq)
    for i in range(n):
        chain.append({"module": "diag", "cap": "log", "decision": "CAP_OK", "i": i})
    return chain


def export(n=3, key=KEY):
    return audit.export_lines(build_chain(n, key))


def tamper(lines, index, field, value):
    """Remplace un champ d'une ligne exportée (attaque ciblée)."""
    out = list(lines)
    obj = json.loads(out[index])
    obj[field] = value
    out[index] = audit.dumps(obj)
    return out


class TestChaineVide(unittest.TestCase):
    """ECL-21 : une chaîne vide est valide et démarre sur 64 zéros."""

    def test_ECL21_chaine_vide_verifiee(self):
        ok, reason = audit.verify_chain([], KEY)
        self.assertTrue(ok)
        self.assertEqual(reason, "OK")

    def test_ECL21_chaine_vide_prev_hash_zero(self):
        self.assertEqual(build_chain(1).lines[0]["prev_hash"], ZERO)

    def test_ECL21_tete_de_chaine_vide(self):
        self.assertEqual(audit.AuditChain(KEY).head, ZERO)

    def test_ECL21_export_chaine_vide(self):
        self.assertEqual(audit.export_lines(build_chain(0)), [])


class TestAjoutDeLignes(unittest.TestCase):
    """ECL-21 : ajout, numérotation et scellement des lignes."""

    def test_ECL21_ajout_de_lignes(self):
        lines = export(5)
        self.assertEqual(len(lines), 5)
        self.assertTrue(audit.verify_chain(lines, KEY)[0])

    def test_ECL21_seq_incremente(self):
        self.assertEqual([l["seq"] for l in build_chain(3).lines], [0, 1, 2])

    def test_ECL21_seq_de_depart(self):
        self.assertEqual([l["seq"] for l in build_chain(2, seq=10).lines], [10, 11])
        self.assertTrue(audit.verify_chain(audit.export_lines(build_chain(2, seq=10)), KEY)[0])

    def test_ECL21_chaque_ligne_scelle_la_precedente(self):
        lines = build_chain(3).lines
        for prev, cur in zip(lines, lines[1:]):
            self.assertEqual(cur["prev_hash"], prev["hash"])

    def test_ECL21_hash_deterministique(self):
        a = audit.compute_hash(KEY, ZERO, {"cap": "log"})
        b = audit.compute_hash(KEY, ZERO, {"cap": "log"})
        self.assertEqual(a, b)
        self.assertTrue(audit.is_hex64(a))

    def test_ECL21_hash_depend_du_precedent(self):
        a = audit.compute_hash(KEY, ZERO, {"cap": "log"})
        b = audit.compute_hash(KEY, a, {"cap": "log"})
        self.assertNotEqual(a, b)

    def test_ECL21_enregistrement_recopie(self):
        record = {"cap": "log", "decision": "CAP_OK"}
        chain = audit.AuditChain(KEY)
        line = chain.append(record)
        record["decision"] = "CAP_DENIED_POLICY"  # mutation après scellement
        self.assertEqual(line["record"]["decision"], "CAP_OK")
        self.assertTrue(audit.verify_chain(chain.lines, KEY)[0])

    def test_ECL21_enregistrement_non_objet_refuse(self):
        with self.assertRaises(TypeError):
            audit.AuditChain(KEY).append("pas-un-objet")

    def test_ECL21_cle_non_octets_refusee(self):
        with self.assertRaises(TypeError):
            audit.AuditChain("pas-des-octets")


class TestFalsification(unittest.TestCase):
    """ECL-21 : le vérificateur indépendant détecte toute altération."""

    def test_ECL21_ligne_modifiee_detectee(self):
        lines = export(3)
        record = dict(json.loads(lines[1])["record"], decision="CAP_DENIED_POLICY")
        ok, reason = audit.verify_chain(tamper(lines, 1, "record", record), KEY)
        self.assertFalse(ok)
        self.assertIn("hash invalide", reason)

    def test_ECL21_ligne_supprimee_detectee(self):
        lines = export(4)
        del lines[1]
        ok, reason = audit.verify_chain(lines, KEY)
        self.assertFalse(ok)
        self.assertIn("rupture de chaîne", reason)

    def test_ECL21_premiere_ligne_supprimee_detectee(self):
        lines = export(3)[1:]
        ok, reason = audit.verify_chain(lines, KEY)
        self.assertFalse(ok)
        self.assertIn("rupture de chaîne", reason)

    def test_ECL21_ligne_reordonnee_detectee(self):
        lines = export(3)
        lines[1], lines[2] = lines[2], lines[1]
        ok, reason = audit.verify_chain(lines, KEY)
        self.assertFalse(ok)
        self.assertIn("rupture de chaîne", reason)

    def test_ECL21_mauvaise_cle_detectee(self):
        ok, reason = audit.verify_chain(export(3), OTHER)
        self.assertFalse(ok)
        self.assertIn("hash invalide", reason)

    def test_ECL21_seq_discontinu_detecte(self):
        lines = tamper(export(3), 2, "seq", 7)
        ok, reason = audit.verify_chain(lines, KEY)
        self.assertFalse(ok)
        self.assertIn("séquence discontinue", reason)

    def test_ECL21_prev_hash_falsifie_detecte(self):
        lines = tamper(export(3), 1, "prev_hash", ZERO)
        ok, reason = audit.verify_chain(lines, KEY)
        self.assertFalse(ok)
        self.assertIn("rupture de chaîne", reason)

    def test_ECL21_hash_falsifie_detecte(self):
        lines = tamper(export(3), 1, "hash", "f" * 64)
        ok, reason = audit.verify_chain(lines, KEY)
        self.assertFalse(ok)
        self.assertIn("hash invalide", reason)


class TestFormeDesLignes(unittest.TestCase):
    """ECL-21 : lignes malformées refusées avec une raison explicite."""

    def test_ECL21_json_invalide(self):
        ok, reason = audit.verify_chain(["{pas-du-json"], KEY)
        self.assertFalse(ok)
        self.assertIn("JSON invalide", reason)

    def test_ECL21_ligne_non_objet(self):
        ok, reason = audit.verify_chain(["[1,2,3]"], KEY)
        self.assertFalse(ok)
        self.assertIn("objet JSON attendu", reason)

    def test_ECL21_champ_manquant(self):
        ok, reason = audit.verify_chain(['{"seq":0,"hash":"%s","record":{}}' % ZERO], KEY)
        self.assertFalse(ok)
        self.assertIn("champ manquant", reason)

    def test_ECL21_empreinte_non_hex(self):
        ok, reason = audit.verify_chain(['{"seq":0,"prev_hash":"zz","hash":"%s","record":{}}' % ZERO], KEY)
        self.assertFalse(ok)
        self.assertIn("empreinte mal formée", reason)

    def test_ECL21_seq_non_entier(self):
        ok, reason = audit.verify_chain(
            ['{"seq":"0","prev_hash":"%s","hash":"%s","record":{}}' % (ZERO, ZERO)], KEY)
        self.assertFalse(ok)
        self.assertIn("seq invalide", reason)

    def test_ECL21_record_non_objet(self):
        ok, reason = audit.verify_chain(
            ['{"seq":0,"prev_hash":"%s","hash":"%s","record":"x"}' % (ZERO, ZERO)], KEY)
        self.assertFalse(ok)
        self.assertIn("enregistrement invalide", reason)

    def test_ECL21_lignes_en_octets_acceptees(self):
        lines = [l.encode("utf-8") for l in export(2)]
        self.assertTrue(audit.verify_chain(lines, KEY)[0])


class TestCanoniqueEtJsonl(unittest.TestCase):
    """ECL-21 : sérialisation déterministe et aller-retour JSONL."""

    def test_ECL21_canonical_ordre_des_cles(self):
        self.assertEqual(audit.canonical({"b": 1, "a": 2}), audit.canonical({"a": 2, "b": 1}))

    def test_ECL21_canonical_separateurs_compacts(self):
        self.assertEqual(audit.canonical({"a": 1, "b": [1, 2]}), b'{"a":1,"b":[1,2]}')

    def test_ECL21_canonical_utf8_non_echappe(self):
        self.assertEqual(audit.canonical({"m": "éclat"}), '{"m":"éclat"}'.encode("utf-8"))

    def test_ECL21_aller_retour_jsonl(self):
        lines = export(4)
        text = "\n".join(lines) + "\n"
        self.assertTrue(audit.verify_chain(audit.load_lines(text), KEY)[0])

    def test_ECL21_load_lines_ignore_les_vides(self):
        text = '{"a":1}\n\n   \n{"b":2}\n'
        self.assertEqual(audit.load_lines(text), ['{"a":1}', '{"b":2}'])

    def test_ECL21_export_une_ligne_par_decision(self):
        chain = build_chain(3)
        lines = audit.export_lines(chain)
        self.assertEqual(len(lines), 3)
        self.assertTrue(all("\n" not in l for l in lines))

    def test_ECL21_export_depuis_liste_de_lignes(self):
        lines = build_chain(2).lines
        self.assertEqual(audit.export_lines(lines), audit.export_lines(build_chain(2)))


class TestKeys(unittest.TestCase):
    """ECL-21 : chargement de la clé partagée, jamais de clé par défaut en dur."""

    def setUp(self):
        self._saved = dict(os.environ)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._saved)

    def test_ECL21_cle_depuis_environnement(self):
        os.environ[TEST_ENV] = "secret\n"
        self.assertEqual(keys.load_key(TEST_ENV), b"secret")

    def test_ECL21_cle_depuis_fichier(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "key.txt")
            with open(path, "wb") as fp:
                fp.write(b"cle-fichier\n")
            self.assertEqual(keys.load_key(TEST_ENV, path), b"cle-fichier")

    def test_ECL21_environnement_prioritaire(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "key.txt")
            with open(path, "wb") as fp:
                fp.write(b"fichier")
            os.environ[TEST_ENV] = "env"
            self.assertEqual(keys.load_key(TEST_ENV, path), b"env")

    def test_ECL21_cle_absente_leve_keyerror(self):
        os.environ.pop(TEST_ENV, None)
        with self.assertRaises(KeyError):
            keys.load_key(TEST_ENV)

    def test_ECL21_fichier_de_cle_absent(self):
        with self.assertRaises(KeyError):
            keys.load_key(TEST_ENV, "/non/existant/cle.txt")

    def test_ECL21_cle_vide_refusee(self):
        os.environ[TEST_ENV] = "   \n"
        with self.assertRaises(KeyError):
            keys.load_key(TEST_ENV)

    def test_ECL21_require_key_message_explicite(self):
        os.environ.pop(keys.AUDIT_KEY_ENV, None)
        with self.assertRaises(KeyError) as ctx:
            keys.require_key()
        self.assertIn(keys.AUDIT_KEY_ENV, str(ctx.exception))

    def test_ECL21_require_key_variables_distinctes(self):
        self.assertEqual(keys.AUDIT_KEY_ENV, "ECLAT_AUDIT_KEY")
        self.assertEqual(keys.MANIFEST_KEY_ENV, "ECLAT_MANIFEST_KEY")


if __name__ == "__main__":
    unittest.main(verbosity=2)
