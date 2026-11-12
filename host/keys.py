# SPDX-License-Identifier: Apache-2.0
"""Éclat — chargement de la clé partagée (journal d'audit ECL-21, manifeste ECL-07).

Ce module est l'**unique** point de lecture d'une clé dans le projet : aucun autre
fichier ne lit d'environnement ni de fichier de clé.

Choix de format : la clé est du **texte brut** ; les espaces en bout de chaîne
(saut de ligne final d'un fichier, espaces de fin) sont retirés.

Sécurité : **aucune clé par défaut n'est câblée en dur**. Une clé par défaut
rendrait la signature décorative (tout le monde pourrait forger une signature
valide) ; l'absence de clé est donc une erreur explicite, pas un repli silencieux.
"""
import os

#: Variables d'environnement standard (utilisées par les CLI).
AUDIT_KEY_ENV = "ECLAT_AUDIT_KEY"
MANIFEST_KEY_ENV = "ECLAT_MANIFEST_KEY"


def load_key(env_var, path=None):
    """Retourne la clé (octets) lue dans la variable d'environnement `env_var`.

    Ordre de recherche : variable d'environnement, puis fichier `path` (si fourni).
    Lève `KeyError` si aucune source n'est disponible, si la source est vide ou
    si le fichier est illisible.
    """
    raw = os.environ.get(env_var)
    if raw is not None:
        key = raw.strip().encode("utf-8")
        if not key:
            raise KeyError("clé vide dans la variable d'environnement %s" % env_var)
        return key
    if path is not None:
        key = _read_key_file(path)
        if not key:
            raise KeyError("clé vide dans le fichier %s" % path)
        return key
    raise KeyError("clé introuvable : définissez %s ou fournissez un fichier de clé" % env_var)


def require_key(env_var=AUDIT_KEY_ENV, path=None):
    """Comme `load_key`, mais avec un message d'erreur explicitement actionnable."""
    try:
        return load_key(env_var, path)
    except KeyError as exc:
        raise KeyError(
            "%s — usage : export %s='<secret>' (ou --key-file <fichier>) ; "
            "aucune clé par défaut n'est câblée en dur" % (message_of(exc), env_var)
        ) from exc


def message_of(exc):
    """Texte d'une `KeyError` sans les guillemets ajoutés par son `repr`."""
    return exc.args[0] if exc.args else str(exc)


def _read_key_file(path):
    """Lit un fichier de clé (texte brut, espaces de fin retirés)."""
    try:
        with open(path, "rb") as fp:
            return fp.read().strip()
    except OSError as exc:
        raise KeyError("clé introuvable : fichier %s illisible (%s)" % (path, exc.strerror)) from exc
