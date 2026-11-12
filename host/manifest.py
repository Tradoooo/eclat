# SPDX-License-Identifier: Apache-2.0
"""Éclat — manifeste de module signé (ECL-07, chaîne d'approvisionnement).

Un manifeste décrit un module WebAssembly : nom, version, empreinte sha256,
capacités demandées, budget WCRT mesuré (µs) et taille. Le manifeste est signé
par HMAC-SHA256 avec une clé partagée : la signature atteste que le manifeste —
et donc l'empreinte du module — n'a pas été altéré en transit.

`canonical()` n'est pas redéfini ici : il vient de `host/audit.py` (source unique).
"""
import hashlib
import hmac
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from audit import canonical  # noqa: E402

#: Les 7 capacités du contrat (CONTRACT.md §1).
CAPS = ("time_ns", "log", "read_sensor", "write_actuator", "kv_get", "kv_put", "net_send")

#: Champs obligatoires d'un manifeste.
REQUIRED_FIELDS = ("name", "version", "sha256", "caps", "size", "signature")

HEX_LEN = 64
_HEX_DIGITS = "0123456789abcdefABCDEF"


def is_hex64(value):
    """Vrai si `value` est une chaîne de 64 caractères hexadécimaux."""
    if not isinstance(value, str) or len(value) != HEX_LEN:
        return False
    return all(c in _HEX_DIGITS for c in value)


def _is_int(value):
    """Vrai si `value` est un entier (booléen exclu)."""
    return isinstance(value, int) and not isinstance(value, bool)


def _unsigned(manifest):
    """Copie du manifeste sans le champ « signature » (ce qui est réellement scellé)."""
    return {k: v for k, v in manifest.items() if k != "signature"}


def build_manifest(module_bytes, name, caps, wcrt_us=None, version="0.1"):
    """Construit le manifeste (sans signature) d'un module.

    `caps` est triée ; `wcrt_us` vaut None quand le budget n'a pas été mesuré.
    """
    return {
        "name": name,
        "version": version,
        "sha256": hashlib.sha256(module_bytes).hexdigest(),
        "caps": sorted(caps),
        "wcrt_us": wcrt_us,
        "size": len(module_bytes),
    }


def sign_manifest(manifest, key):
    """Renvoie une **copie** du manifeste signée (champ « signature » ajouté)."""
    signed = _unsigned(manifest)
    signed["signature"] = hmac.new(key, canonical(signed), hashlib.sha256).hexdigest()
    return signed


def verify_manifest(manifest, key, module_bytes=None):
    """Vérifie un manifeste signé. Retourne `(ok, raison)`.

    Contrôle : champs obligatoires, signature, capacités connues, budget WCRT
    plausible, et — si `module_bytes` est fourni — empreinte sha256 du module.
    """
    if not isinstance(manifest, dict):
        return False, "manifeste invalide : objet JSON attendu"
    missing = [f for f in REQUIRED_FIELDS if f not in manifest]
    if missing:
        return False, "champ manquant : %s" % ", ".join(missing)
    err = _check_fields(manifest)
    if err is not None:
        return False, err
    expected = hmac.new(key, canonical(_unsigned(manifest)), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(expected, manifest["signature"]):
        return False, "signature invalide : le manifeste a été modifié"
    unknown = [c for c in manifest["caps"] if c not in CAPS]
    if unknown:
        return False, "capacité inconnue : %s" % ", ".join(sorted(unknown))
    err = _check_wcrt(manifest.get("wcrt_us"))
    if err is not None:
        return False, err
    if module_bytes is not None:
        if hashlib.sha256(module_bytes).hexdigest() != manifest["sha256"]:
            return False, "empreinte sha256 du module différente de celle du manifeste"
    return True, "OK"


def _check_fields(manifest):
    """Contrôle le type des champs obligatoires. Retourne None ou la raison."""
    if not isinstance(manifest["name"], str) or not manifest["name"]:
        return "champ name invalide : chaîne non vide attendue"
    if not isinstance(manifest["version"], str) or not manifest["version"]:
        return "champ version invalide : chaîne non vide attendue"
    if not is_hex64(manifest["sha256"]):
        return "champ sha256 invalide : 64 hex attendus"
    if not _is_int(manifest["size"]) or manifest["size"] < 0:
        return "champ size invalide : entier positif ou nul attendu"
    caps = manifest["caps"]
    if not isinstance(caps, list) or not all(isinstance(c, str) for c in caps):
        return "champ caps invalide : liste de chaînes attendue"
    if not is_hex64(manifest["signature"]):
        return "signature absente ou mal formée"
    return None


def _check_wcrt(wcrt_us):
    """Contrôle du budget WCRT : absent (None) ou entier strictement positif."""
    if wcrt_us is None:
        return None
    if not _is_int(wcrt_us) or wcrt_us <= 0:
        return "wcrt_us invalide : entier strictement positif ou absent attendu"
    return None
