# Passation du SDP QNX 8.0 — qui fait quoi

État au 12 novembre 2026, 12 h 40. **Aucun compte myQNX n'a été créé, aucune licence acceptée,
aucun identifiant saisi.** Le formulaire d'inscription a seulement été ouvert et cartographié
dans le navigateur d'automatisation, puis laissé tel quel.

## Ce que Baptiste fait lui-même (actes juridiques, donc humains)

1. **Créer le compte myQNX** — https://www.qnx.com/account/login.html?returnaddress=/products/everywhere/index.html&create#showcreate
   Trois champs : email, mot de passe, confirmation. Pas de captcha. Mot de passe **alphanumérique
   uniquement**. Le bouton *Create* fait accepter le *QNX Community Membership Agreement*.
2. **Choisir et accepter la licence** — c'est le point qui demande un jugement humain :

   | Licence | Où | Durée | Condition | Attention |
   |---|---|---|---|---|
   | QNX Everywhere (non-commerciale) | qnx.com/products/everywhere/ | sans limite affichée | « personal **non-commercial** use » | le hackathon verse des prix (4 000 $ / 2 000 $ / 3 500 $) : est-ce un usage commercial ? À trancher par toi, pas par moi |
   | Évaluation 30 j | qnx.com/products/evaluation/ | 30 jours | « for **commercial** projects only » | échéance du concours le 15/12 : une licence prise aujourd'hui expire ~12/12 |

3. **Récupérer le SDP 8.0** via myQNX License Manager (https://www.qnx.com/account/dashboard/) :
   *Accept License* → *Deploy* → télécharger l'archive, ou installer via QNX Software Center.
4. **Déposer l'archive ou l'arborescence installée** quelque part sur la machine et me donner le
   chemin (ou l'extraire toi-même : `tar xf …` dans `~/qnx800`).

## Ce que je fais dès que le chemin arrive (aucune action humaine requise)

```bash
source tools/setup-sdp.sh /chemin/vers/le/sdp   # localise qnxsdp-env.sh, vérifie qcc
tools/e2-probe.sh                                # ESSAI E2 : verdict par fichier, par cible
```

`e2-probe.sh` compile avec le **vrai qcc** les 11 fichiers de la couche d'abstraction POSIX de
WAMR (`core/shared/platform/common/posix/*.c`, `common/math/math.c`, `linux/platform_init.c`)
pour `nto x86_64` puis `nto aarch64le`, avec `-D_QNX_SOURCE -DWASM_HAVE_MREMAP=0` et les deux
répertoires d'en-têtes que le cmake amont utilise. Un verdict **par fichier**, conservé dans
`build/e2/` : E2 = OUI / PARTIEL / NON, jamais un verdict global inventé.

Puis, selon le verdict :

- **OUI** → j'écris `platform/qnx/` (le delta restant), `broker/` en gestionnaire de ressources,
  la politique `secpol`, et je lance le banc de latence sur cible (lots L7 à L9).
- **PARTIEL** → je résous chaque écart listé dans `build/e2/*.log` et je documente chacun dans
  `upstream/wamr/PORTING-NOTES.md` (c'est la valeur communautaire de la contribution, ECL-17).
- **NON** → j'applique le repli du cahier des charges (§9.1) : livrer la couche validée sur POSIX
  et le dire explicitement, ou basculer le temps restant sur Tamis + Glane.

## Ce que je ne ferai pas sans demande explicite

- saisir un email ou un mot de passe dans un navigateur ;
- accepter un contrat de licence, quel qu'il soit ;
- télécharger depuis un compte qui n'est pas le tien ;
- redistribuer un binaire QNX (le SDP est sous licence : il reste sur cette machine, jamais dans
  une image Docker, jamais dans le dépôt).