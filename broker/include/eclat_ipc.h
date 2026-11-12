// SPDX-License-Identifier: Apache-2.0
/* Éclat — protocole du courtier de capacités, entre le module et le courtier QNX.
 *
 * Le module ne décide rien : chaque import devient un message synchrone vers le courtier, qui
 * applique la politique et répond. C'est la frontière de sécurité : si le courtier est dans un
 * processus sans abilities, le module ne peut rien obtenir que la politique n'accorde.
 *
 * CONTRAINTE QNX (vérifiée sur QNX 8.0, et lue dans l'implémentation de référence de la PR
 * amont #5200) : un message reçu par MsgReceive commence par le type de message du noyau. Les
 * types du noyau vont jusqu'à _IO_MAX (0x1ff) ; une application doit donc mettre SON type
 * AU-DESSUS de cette borne, sinon le noyau ou le gestionnaire de chemins interprète le
 * message à sa façon. D'où le champ `type` en tête de chaque structure, et l usage de 0xec01.
 *
 * Le protocole est volontairement trivial : deux structures de taille fixe, aucun pointeur,
 * aucune allocation, aucune donnée de taille variable sur le fil. Un module malveillant ne
 * peut donc ni désynchroniser le courtier, ni lui faire écrire hors de son tampon.
 *
 * Ce fichier est le SEUL contrat entre le courtier et ses clients. Il ne dépend que de
 * <stdint.h> : il se compile à l'identique sur la cible et côté hôte. */
#ifndef ECLAT_IPC_H
#define ECLAT_IPC_H

#include <stdint.h>

/* Type de message applicatif. Doit être > _IO_MAX (0x1ff) pour ne pas être confondu avec un
 * message du noyau ou du gestionnaire de chemins. */
#define ECLAT_IPC_TYPE 0xec01u

/* Opérations. */
enum {
    ECLAT_OP_PING = 0,   /* « es-tu là ? » — sert aussi de test de vivacité */
    ECLAT_OP_REQUEST = 1 /* demande de capacité : c'est le chemin chaud */
};

/* Nom sous lequel le courtier s'enregistre auprès du gestionnaire de chemins de QNX.
 *
 * Un `name_attach()` n'accepte qu'un NOM simple : un chemin avec des « / » est refusé
 * (EINVAL, vérifié sur QNX 8.0). D'où deux formes :
 *   ECLAT_IPC_NAME  — « cap », enregistré dès aujourd'hui dans l'espace de noms de QNX ;
 *   ECLAT_IPC_PATH  — « /dev/cap », qui exige `resmgr_attach()` et le jeu complet de fonctions
 *                     iofunc : c'est l'étape suivante du lot L8.
 * Le protocole ne change pas entre les deux : seule la façon de publier le nom change. */
#define ECLAT_IPC_NAME "cap"
#define ECLAT_IPC_PATH "/dev/cap"

/* En-tête commun : type, sous-type, longueur. */
typedef struct eclat_ipc_hdr {
    uint16_t type;   /* ECLAT_IPC_TYPE */
    uint16_t subtype; /* réservé, 0 */
    uint32_t length; /* octets de charge utile qui suivent */
} eclat_ipc_hdr_t;

typedef struct eclat_ipc_request {
    eclat_ipc_hdr_t hdr;
    uint32_t op;        /* enum eclat_ipc_op */
    uint32_t cap;       /* index de capability, aligné sur core/include/eclat/cap_decision.h */
    int32_t id;         /* identifiant capteur/actionneur/canal, -1 si sans objet */
    int32_t value_milli;/* valeur demandée (actionneur) */
    uint32_t seq;       /* numéro de séquence, pour corréler requête et réponse */
} eclat_ipc_request_t;

typedef struct eclat_ipc_reply {
    eclat_ipc_hdr_t hdr;
    int32_t decision; /* code de décision (CAP_OK, CAP_DENIED_POLICY, ...) */
    int32_t payload;  /* valeur de retour, ex. l'échantillon du capteur */
    uint32_t seq;     /* même numéro de séquence que la requête */
} eclat_ipc_reply_t;

#endif /* ECLAT_IPC_H */