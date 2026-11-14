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
 *                          ce qui devient impossible — c'est la deuxième couche d'isolation ;
 *   sonde ciblees          applique le retrait CIBLÉ (la couche d'abilities du runtime),
 *                          puis mesure ce qui reste possible ET ce qui devient impossible.
 *
 * Le serveur témoin existe pour que le refus soit attribuable à la politique et non à
 * l'absence du service : le nom est bien là, et la connexion échoue quand même.
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/procmgr.h>
#include <unistd.h>

#include "eclat_abilities.h"

/* Fichier lu pour montrer que le runtime garde son accès au disque : le module .wasm et la
 * politique se lisent ainsi. Dans l'image de démonstration, c'est ce fichier qui existe. */
#define FICHIER_SONDE "/proc/boot/pol-broker.toml"

/* Ce que la sonde a mesuré : 1 = possible, 0 = refusé. */
typedef struct sonde_mesure {
    int canal;   /* name_attach : publier un canal */
    int wx;      /* mmap PROT_WRITE|PROT_EXEC */
    int fork_;   /* créer un processus */
    int lecture; /* lire un fichier */
} sonde_mesure_t;

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
static int essai_fork(void) {
    errno = 0;
    pid_t pid = fork();
    int e = errno;
    if (pid < 0) {
        printf("  fork()               : REFUSÉ (errno=%d %s)\n", e, strerror(e));
        return 0;
    }
    if (pid == 0) {
        _exit(0); /* l'enfant meurt tout de suite */
    }
    printf("  fork()               : ACCORDÉ (enfant %d)\n", (int)pid);
    return 1;
}

/* Tente de publier un nom dans l'espace de noms. */
static int essai_canal(void) {
    errno = 0;
    name_attach_t *a = name_attach(NULL, "sonde-self", 0);
    int e = errno;
    if (a != NULL) {
        printf("  name_attach()        : ACCORDÉ (chid %d)\n", a->chid);
        name_detach(a, 0);
        return 1;
    }
    printf("  name_attach()        : REFUSÉ (errno=%d %s)\n", e, strerror(e));
    return 0;
}

/* Tente de mapper de la mémoire à la fois ÉCRIVABLE et EXÉCUTABLE.
 * C'est l'ability PRIVILÉGIÉE « prot_write_and_exec » : elle n'est pas accordée par défaut,
 * et c'est exactement le genre de droit qu'un processus qui exécute du code tiers ne doit
 * pas garder. */
static int essai_wexec(void) {
    errno = 0;
    void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    int e = errno;
    if (p != MAP_FAILED) {
        printf("  mmap(PROT_WRITE|EXEC) : ACCORDÉ\n");
        munmap(p, 4096);
        return 1;
    }
    printf("  mmap(PROT_WRITE|EXEC) : REFUSÉ (errno=%d %s)\n", e, strerror(e));
    return 0;
}

/* Tente de lire un fichier : c'est ce que le runtime fait pour charger son module. */
static int essai_lecture(const char *chemin) {
    errno = 0;
    int fd = open(chemin, O_RDONLY);
    int e = errno;
    if (fd < 0) {
        printf("  lecture %-12s : REFUSÉE (errno=%d %s)\n", chemin, e, strerror(e));
        return 0;
    }
    char tampon[64];
    ssize_t n = read(fd, tampon, sizeof(tampon));
    close(fd);
    if (n < 0) {
        printf("  lecture %-12s : REFUSÉE (errno=%d %s)\n", chemin, errno, strerror(errno));
        return 0;
    }
    printf("  lecture %-12s : ACCORDÉE (%d octets)\n", chemin, (int)n);
    return 1;
}

/* Abandonne toutes les abilities et les verrouille (idiome documenté de procmgr_ability). */
static void abandonner(void) {
    char err[160];
    int rc = eclat_abilities_drop_all(err, sizeof(err));
    printf("  procmgr_ability(DENY|LOCK|EOL) : %s\n", rc == 0 ? "APPLIQUÉ" : err);
    fflush(stdout);
}

/* Mesure l'état « avant » : ce que le processus peut faire tant qu'il n'a rien retiré. */
static sonde_mesure_t mesurer(void) {
    sonde_mesure_t m;
    essai_joindre("cap");
    m.canal = essai_canal();
    m.wx = essai_wexec();
    m.fork_ = essai_fork();
    m.lecture = essai_lecture(FICHIER_SONDE);
    fflush(stdout);
    return m;
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

/* Applique le retrait CIBLÉ, ability par ability, et dit ce qui a pris et ce qui n'a pas pris.
 *
 * La liste vient de la MÊME couche que le runtime (platform/qnx/eclat_abilities_qnx.c) et le
 * retrait passe par la MÊME primitive : la mesure porte donc sur le code qui tournera
 * dans eclat-run, pas sur une copie. */
static void retirer_ciblees(void) {
    const eclat_ability_ciblee_t *liste = eclat_abilities_ciblees();
    size_t n = eclat_abilities_ciblees_count();
    size_t prises = 0;
    size_t refusees = 0;
    for (size_t i = 0; i < n; i++) {
        char detail[128];
        int refus = 0;
        if (eclat_abilities_drop_one(liste[i].id, &refus, detail, sizeof(detail)) == 0) {
            prises++;
            printf("    %-22s : RETIRÉE\n", liste[i].nom);
        } else {
            refusees++;
            printf("    %-22s : REFUSÉE (errno=%d %s)\n", liste[i].nom, refus, detail);
        }
    }
    printf("  retrait ciblé : %zu visées, %zu retirées, %zu refusées\n", n, prises, refusees);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage : sonde {essai <nom>... | serve <nom> | abilities | ciblees}\n");
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
    if (strcmp(argv[1], "ciblees") == 0) {
        printf("— AVANT le retrait ciblé —\n");
        mesurer();
        retirer_ciblees();
        printf("— APRÈS le retrait ciblé —\n");
        sonde_mesure_t apres = mesurer();
        printf("  VERDICT-CIBLE : wx=%s fork=%s lecture=%s\n",
               apres.wx ? "ENCORE_ACCESSIBLE" : "REFUSEE", apres.fork_ ? "ACCORDE" : "REFUSE",
               apres.lecture ? "ACCORDEE" : "REFUSEE");
        fflush(stdout);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        essai_joindre(argv[i]);
    }
    essai_wexec();
    essai_fork();
    return 0;
}
