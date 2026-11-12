// SPDX-License-Identifier: Apache-2.0
/* Éclat — transport QNX : la décision est prise par le courtier, dans un AUTRE processus.
 *
 * C'est le mode qui porte la promesse du projet. Le module ne peut rien obtenir que la
 * politique n'accorde, parce que la politique n'est appliquée que d'un seul côté : dans le
 * courtier. Et l'on peut retirer au courtier toutes les abilities QNX sans que le module ne
 * perde sa capacité d'appel — il n'en a pas.
 *
 * QNX uniquement : ce fichier est compilé pour la cible, jamais pour l'hôte. */
#ifndef ECLAT_TRANSPORT_QNX_H
#define ECLAT_TRANSPORT_QNX_H

#include "eclat_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Contexte du transport QNX : la connexion au courtier et le compteur de séquence. */
typedef struct eclat_transport_qnx_ctx {
    int coid;      /* canal du courtier, < 0 si non connecté */
    uint32_t seq;  /* numéro de séquence des requêtes */
} eclat_transport_qnx_ctx_t;

/* Se connecte au courtier publié sous `nom` (par défaut « cap »). Retourne 0 si connecté. */
int eclat_transport_qnx_init(eclat_transport_qnx_ctx_t *qc, eclat_transport_t *t,
                              const char *nom);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_TRANSPORT_QNX_H */