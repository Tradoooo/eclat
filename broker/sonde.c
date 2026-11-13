// SPDX-License-Identifier: Apache-2.0
/* Éclat — sonde de sécurité : ce qu'un processus peut joindre, mesuré sur la cible.
 *
 * Cette sonde ne connaît ni la politique ni le type qu'on lui a donné : c'est le noyau QNX qui
 * l'applique, parce que le processus a été démarré avec `on -T <type>`. Elle se contente de
 * tenter des choses et de rapporter le verdict du noyau.
 *
 * Trois usages :
 *   sonde essai <nom>...   tente de joindre chaque nom, puis tente de créer un processus ;
 *   sonde serve <nom>      publie un canal sous <nom> et répond — c'est le serveur témoin ;
 *   sonde abilities        abandonne TOUTES ses abilities (et les verrouille), puis mesure
 *                          ce qui devient impossible — c'est la deuxième couche d'isolation.
 *
 * Le serveur témoin existe pour que le refus soit attribuable à la politique et non à
 * l'absence du service : le nom est bien là, et la connexion échoue quand même.
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/procmgr.h>
#include <unistd.h>

/* Tente de joindre un nom et rapporte le verdict du noyau. */
static void essai_joindre(const char *nom) {
    errno = 0;
    int coid = name_open(nom, 0);
    int e = errno;
    if (coid >= 0) {
        printf("  name_open(%-10s) : OUVERT\n", nom);
        name_close(coid);
    } else {
        printf("  name_open(%-10s) : REFUSÉ (errno=%d %s)\n", nom, e, strerror(e));
    }
}

/* Tente de créer un processus : c'est la capacité « fork », distincte des canaux. */
static void essai_fork(void) {
    errno = 0;
    pid_t pid = fork();
    int e = errno;
    if (pid < 0) {
        printf("  fork()               : REFUSÉ (errno=%d %s)\n", e, strerror(e));
    } else if (pid == 0) {
        _exit(0); /* l'enfant meurt tout de suite */
    } else {
        printf("  fork()               : ACCORDÉ (enfant %d)\n", (int)pid);
    }
}

/* Tente de publier un nom dans l'espace de noms. */
static void essai_canal(void) {
    errno = 0;
    name_attach_t *a = name_attach(NULL, "sonde-self", 0);
    int e = errno;
    if (a != NULL) {
        printf("  name_attach()        : ACCORDÉ (chid %d)\n", a->chid);
        name_detach(a, 0);
    } else {
        printf("  name_attach()        : REFUSÉ (errno=%d %s)\n", e, strerror(e));
    }
}

/* Tente de mapper de la mémoire à la fois ÉCRIVABLE et EXÉCUTABLE.
 * C'est l'ability PRIVILÉGIÉE « prot_write_and_exec » : elle n'est pas accordée par défaut,
 * et c'est exactement le genre de droit qu'un processus qui exécute du code tiers ne doit
 * pas garder. */
static void essai_wexec(void) {
    errno = 0;
    void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_ANON | MAP_PRIVATE, -1, 0);
    int e = errno;
    if (p != MAP_FAILED) {
        printf("  mmap(PROT_WRITE|EXEC) : ACCORDÉ\n");
        munmap(p, 4096);
    } else {
        printf("  mmap(PROT_WRITE|EXEC) : REFUSÉ (errno=%d %s)\n", e, strerror(e));
    }
}

/* Abandonne toutes les abilities et les verrouille (idiome documenté de procmgr_ability). */
static void abandonner(void) {
    errno = 0;
    int rc = procmgr_ability(0,
                             PROCMGR_ADN_ROOT | PROCMGR_AOP_DENY | PROCMGR_AOP_LOCK |
                                 PROCMGR_AID_EOL);
    printf("  procmgr_ability(DENY|LOCK|EOL) : %s\n", rc == 0 ? "APPLIQUÉ" : strerror(errno));
    fflush(stdout);
}

static void mesurer(void) {
    essai_joindre("cap");
    essai_canal();
    essai_wexec();
    essai_fork();
    fflush(stdout);
}

/* Publie un canal sous un nom et répond à tout message — le serveur témoin. */
static int servir(const char *nom) {
    name_attach_t *a = name_attach(NULL, nom, 0);
    if (a == NULL) {
        printf("sonde serve: name_attach(%s) a échoué : %s\n", nom, strerror(errno));
        return 2;
    }
    printf("sonde: serveur témoin « %s » à l'écoute (chid %d)\n", nom, a->chid);
    fflush(stdout);
    for (;;) {
        char tampon[256];
        struct _msg_info info;
        int rc = MsgReceive(a->chid, tampon, sizeof(tampon), &info);
        if (rc > 0) {
            MsgReply(rc, 0, "ok", 2);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage : sonde {essai <nom>... | serve <nom> | abilities}\n");
        return 2;
    }
    if (strcmp(argv[1], "serve") == 0 && argc >= 3) {
        return servir(argv[2]);
    }
    if (strcmp(argv[1], "abilities") == 0) {
        printf("— AVANT l'abandon des abilities (processus root ordinaire) —\n");
        mesurer();
        abandonner();
        printf("— APRÈS l'abandon des abilities —\n");
        mesurer();
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        essai_joindre(argv[i]);
    }
    essai_wexec();
    essai_fork();
    return 0;
}