// SPDX-License-Identifier: Apache-2.0
/* Éclat — le courtier de capacités, sur QNX.
 *
 * Le courtier s'enregistre dans l'espace de noms de QNX et sert ensuite les imports des modules
 * par message synchrone : le module ne décide rien, il demande ; c'est le courtier qui applique
 * la politique et répond. C'est la frontière de sécurité du projet : la décision est prise dans un
 * processus unique, dont on peut retirer les abilities.
 *
 * ECL-11 : le courtier tourne comme gestionnaire de ressources et sert les imports par message.
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include "eclat_ipc.h"

#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/iomsg.h>
#include <sys/neutrino.h>
#include <sys/pathmgr.h>
#include <unistd.h>

/* Politique du module servi. Unique, en lecture seule après chargement. */
static eclat_policy_t g_policy;

/* Signal capteur simulé ET vivant : une sinusoïde de 1 Hz en milli-unités, centrée sur 3000,
 * lue à l'instant de la demande. Deux lectures à des instants différents donnent des valeurs
 * différentes — c'est un capteur, pas une constante. */
#define SENSOR_CENTRE    3000
#define SENSOR_AMPLITUDE 1000
#define SENSOR_HZ        1.0

static int32_t lire_capteur(void) {
    _Uint64t ns = 0;
    ClockTime(CLOCK_MONOTONIC, NULL, &ns);
    double t = (double)ns / 1000000000.0;
    double v = (double)SENSOR_CENTRE + (double)SENSOR_AMPLITUDE * sin(t * 2.0 * M_PI * SENSOR_HZ);
    return (int32_t)v;
}

/* Charge la politique depuis un fichier. Retourne 0 si elle est valide. */
static int charger_politique(const char *chemin) {
    static unsigned char tampon[256 * 1024];
    FILE *f = fopen(chemin, "rb");
    if (f == NULL) {
        fprintf(stderr, "courtier: politique illisible : %s\n", chemin);
        return -1;
    }
    size_t lu = fread(tampon, 1, sizeof(tampon), f);
    fclose(f);
    eclat_policy_error_t err;
    memset(&err, 0, sizeof(err));
    if (eclat_policy_parse(tampon, lu, &g_policy, &err) != 0) {
        fprintf(stderr, "courtier: politique refusée : %s : %s\n", err.field, err.message);
        return -1;
    }
    printf("courtier: politique « %.*s » chargée (%u règles allow, %u règles deny)\n",
           (int)g_policy.name_len, g_policy.name, g_policy.n_allows, g_policy.n_denies);
    return 0;
}

/* Évalue une requête de capacité et rend la décision. */
static int32_t evaluer(const eclat_ipc_request_t *req, int32_t *payload) {
    eclat_request_t r;
    memset(&r, 0, sizeof(r));
    r.cap = (eclat_cap_t)req->cap;
    r.id = req->id;
    r.value_milli = req->value_milli;
    eclat_decision_t d = eclat_decide(&g_policy, &r);
    *payload = 0;
    if (d == ECLAT_CAP_OK && req->cap == (uint32_t)ECLAT_CAP_READ_SENSOR) {
        *payload = lire_capteur();
    }
    return (int32_t)d;
}

/* Traite une requête et prépare la réponse. */
static void traiter(const eclat_ipc_request_t *req, eclat_ipc_reply_t *rep) {
    memset(rep, 0, sizeof(*rep));
    rep->hdr.type = ECLAT_IPC_TYPE;
    rep->hdr.length = (uint32_t)(sizeof(*rep) - sizeof(rep->hdr));
    rep->seq = req->seq;
    if (req->op == ECLAT_OP_PING) {
        rep->decision = ECLAT_CAP_OK;
    } else if (req->op == ECLAT_OP_REQUEST) {
        int32_t payload = 0;
        rep->decision = evaluer(req, &payload);
        rep->payload = payload;
    } else {
        rep->decision = ECLAT_CAP_BAD_ARG;
    }
}

/* Boucle de service : pulsations, poignée name_open, puis messages applicatifs. */
static void servir(int chid) {
    for (;;) {
        union {
            struct _pulse pulse;
            uint16_t type;
            struct {
                eclat_ipc_hdr_t hdr;
                /* Tampon large : un message _IO_CONNECT de connexion (name_open) est bien plus
                 * gros que notre charge utile, et MsgReceive refuse (EMSGSIZE) un tampon trop
                 * petit. 4 Ko, comme l'implémentation de référence de la PR amont #5200. */
                uint8_t data[4096];
            } app;
        } msg;
        struct _msg_info info;
        memset(&msg, 0, sizeof(msg));
        rcvid_t rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);
        if (rcvid < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "courtier: MsgReceive a échoué : %s\n", strerror(errno));
            return;
        }
        if (rcvid == 0) {
            /* Une pulsation : déconnexion d'un client ou déblocage. */
            if (msg.pulse.code == _PULSE_CODE_DISCONNECT) {
                ConnectDetach(msg.pulse.scoid);
            }
            continue;
        }
        if (msg.type == _IO_CONNECT) {
            /* Poignée de name_open() : on acquitte, le client peut envoyer ses messages. */
            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }
        if (msg.type != ECLAT_IPC_TYPE) {
            MsgError(rcvid, ENOSYS);
            continue;
        }
        eclat_ipc_request_t req;
        memset(&req, 0, sizeof(req));
        size_t n = (size_t)msg.app.hdr.length + sizeof(eclat_ipc_hdr_t);
        if (n > sizeof(req)) {
            n = sizeof(req);
        }
        memcpy(&req, &msg.app.hdr, n);
        eclat_ipc_reply_t rep;
        traiter(&req, &rep);
        MsgReply(rcvid, 0, &rep, sizeof(rep));
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage : eclat-broker <politique.toml>\n");
        return 2;
    }
    if (charger_politique(argv[1]) != 0) {
        return 2;
    }

    /* name_attach() crée LE CANAL et le rend dans name_attach_t.chid : c'est sur ce canal que le
     * gestionnaire de chemins enverra la poignée _IO_CONNECT, puis les messages des clients.
     * Créer son propre canal ici ne servirait à rien — c'est l'erreur classique. */
    name_attach_t *attach = name_attach(NULL, ECLAT_IPC_NAME, 0);
    if (attach == NULL) {
        fprintf(stderr, "courtier: name_attach(%s) a échoué : %s\n", ECLAT_IPC_NAME,
                strerror(errno));
        return 2;
    }
    int chid = attach->chid;
    printf("courtier: à l'écoute sur %s (canal %d)\n", ECLAT_IPC_NAME, chid);
    /* Publication du chemin /dev/cap : le gestionnaire de chemins fait pointer le chemin sur
     * NOTRE canal. C'est le mécanisme prévu pour un serveur qui publie un chemin, et il évite
     * d'écrire un gestionnaire de ressources complet (iofunc) pour un service de messages.
     * Le nom « cap » reste disponible en repli : si /dev n'est pas modifiable, le client
     * name_open() fonctionne toujours. */
    if (pathmgr_link(ECLAT_IPC_PATH, 0, getpid(), chid, 0, _FTYPE_ANY, 0) == 0) {
        printf("courtier: chemin publié : %s\n", ECLAT_IPC_PATH);
    } else {
        /* Le lien existe parfois même si le retour est négatif : on ne conclut pas, on dit ce
         * qu'on a tenté, et le nom « cap » demeure le repli inconditionnel. */
        printf("courtier: publication de %s : retour %d (errno %d) — repli sur le nom %s\n",
               ECLAT_IPC_PATH, -1, errno, ECLAT_IPC_NAME);
    }
    fflush(stdout);

    servir(chid);
    name_detach(attach, 0);
    return 0;
}