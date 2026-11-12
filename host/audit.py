# SPDX-License-Identifier: Apache-2.0
"""Éclat — journal d'audit à hachage enchaîné (ECL-21).

Chaque décision journalisée produit une ligne :

    {"seq": 12, "prev_hash": "<64 hex>", "hash": "<64 hex>", "record": {...}}

avec

    hash = HMAC-SHA256(clé, prev_hash || canonical(record))

où `canonical()` est une sérialisation JSON déterministe (tri des clés,
séparateurs compacts, UTF-8). Elle est définie **une seule fois** ici et
réexportée par `host/manifest.py`.

Limite connue et assumée : une troncature *en fin de chaîne* est indétectable
sans ancrage externe (empreinte de tête publiée hors bande) ; toute
modification, suppression ou réordonnancement *interne* est détecté.
"""
import copy
import hashlib
import hmac
import json

#: Empreinte d'une chaîne vide (aucune ligne précédente) : 64 zéros.
ZERO_HASH = "0" * 64
HEX_LEN = 64
_HEX_DIGITS = "0123456789abcdefABCDEF"


def dumps(obj):
    """JSON canonique sous forme de texte (déterministe)."""
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def canonical(obj):
    """Sérialisation canonique d'un objet : octets UTF-8, ordre des clés stable."""
    return dumps(obj).encode("utf-8")


def is_hex64(value):
    """Vrai si `value` est une chaîne de 64 caractères hexadécimaux."""
    if not isinstance(value, str) or len(value) != HEX_LEN:
        return False
    return all(c in _HEX_DIGITS for c in value)


def compute_hash(key, prev_hash, record):
    """hex(HMAC-SHA256(key, prev_hash || canonical(record)))."""
    prev = bytes.fromhex(prev_hash)
    return hmac.new(key, prev + canonical(record), hashlib.sha256).hexdigest()


class AuditChain:
    """Chaîne de hachages : chaque ligne scelle la précédente.

    `seq` est le numéro de séquence de la **prochaine** ligne : une chaîne neuve
    (`seq=0`) démarre donc à 0, une chaîne relancée à la suite d'un export
    existant reprend au bon endroit.
    """

    def __init__(self, key, seq=0):
        if not isinstance(key, (bytes, bytearray)):
            raise TypeError("la clé doit être une suite d'octets")
        self._key = bytes(key)
        self._next_seq = int(seq)
        self._prev_hash = ZERO_HASH
        self._lines = []

    @property
    def seq(self):
        """Numéro de séquence de la prochaine ligne."""
        return self._next_seq

    @property
    def head(self):
        """Empreinte de la dernière ligne (64 zéros si la chaîne est vide)."""
        return self._prev_hash

    @property
    def lines(self):
        """Copie de la liste des lignes scellées."""
        return list(self._lines)

    def append(self, record):
        """Ajoute une décision et renvoie la ligne complète scellée.

        L'enregistrement est recopié : le muter ensuite ne peut pas invalider
        une ligne déjà scellée.
        """
        if not isinstance(record, dict):
            raise TypeError("un enregistrement doit être un objet")
        sealed = copy.deepcopy(record)
        seq = self._next_seq
        digest = compute_hash(self._key, self._prev_hash, sealed)
        line = {"seq": seq, "prev_hash": self._prev_hash, "hash": digest, "record": sealed}
        self._lines.append(line)
        self._prev_hash = digest
        self._next_seq = seq + 1
        return line


def decode_line(raw, index):
    """Décode une ligne d'export. Retourne (objet, None) ou (None, raison)."""
    if isinstance(raw, dict):
        return raw, None
    if isinstance(raw, (bytes, bytearray)):
        try:
            raw = raw.decode("utf-8")
        except UnicodeDecodeError as exc:
            return None, "ligne %d : UTF-8 invalide (%s)" % (index, exc)
    if not isinstance(raw, str):
        return None, "ligne %d : type de ligne non supporté" % index
    try:
        obj = json.loads(raw)
    except json.JSONDecodeError as exc:
        return None, "ligne %d : JSON invalide (%s)" % (index, exc.msg)
    if not isinstance(obj, dict):
        return None, "ligne %d : objet JSON attendu" % index
    return obj, None


def check_shape(obj, index):
    """Vérifie la forme d'une ligne. Retourne None si correcte, sinon la raison."""
    for field in ("seq", "prev_hash", "hash", "record"):
        if field not in obj:
            return "champ manquant à la ligne %d : %s" % (index, field)
    if not isinstance(obj["seq"], int) or isinstance(obj["seq"], bool):
        return "seq invalide à la ligne %d" % index
    if not is_hex64(obj["prev_hash"]) or not is_hex64(obj["hash"]):
        return "empreinte mal formée à la ligne %d" % index
    if not isinstance(obj["record"], dict):
        return "enregistrement invalide à la ligne %d" % index
    return None


def verify_chain(lines, key):
    """Vérificateur indépendant : recalcule la chaîne et compare chaque hash.

    Retourne `(ok, raison)`. Détecte une ligne modifiée, une ligne supprimée,
    une ligne réordonnée, une séquence discontinue et une mauvaise clé.
    """
    objs = []
    for index, raw in enumerate(lines):
        obj, reason = decode_line(raw, index)
        if obj is None:
            return False, reason
        err = check_shape(obj, index)
        if err is not None:
            return False, err
        objs.append(obj)
    prev = ZERO_HASH
    base = objs[0]["seq"] if objs else 0
    for index, obj in enumerate(objs):
        if obj["prev_hash"] != prev:
            return False, "rupture de chaîne à la ligne %d : prev_hash ne suit pas la ligne précédente" % index
        try:
            digest = compute_hash(key, obj["prev_hash"], obj["record"])
        except ValueError:
            return False, "prev_hash illisible à la ligne %d" % index
        if not hmac.compare_digest(digest, obj["hash"]):
            return False, "hash invalide à la ligne %d (ligne modifiée ou mauvaise clé)" % index
        if obj["seq"] != base + index:
            return False, "séquence discontinue à la ligne %d (%d au lieu de %d)" % (
                index, obj["seq"], base + index)
        prev = obj["hash"]
    return True, "OK"


def export_lines(chain):
    """Une ligne JSON par décision (JSONL), dans l'ordre de la chaîne."""
    objs = chain.lines if isinstance(chain, AuditChain) else list(chain)
    return [dumps(obj) for obj in objs]


def load_lines(text):
    """Relit un export JSONL : liste des lignes non vides, sans saut de ligne."""
    return [line.strip() for line in text.splitlines() if line.strip()]
