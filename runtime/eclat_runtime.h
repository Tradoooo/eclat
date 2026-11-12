// SPDX-License-Identifier: Apache-2.0
/* Éclat — intégration du runtime WebAssembly (WAMR) et résolution des imports.
 *
 * Le runtime ne connaît que le contrat de la couche plateforme (`eclat_platform.h`) : la même
 * intégration fonctionne sur POSIX et sur QNX, seule la couche plateforme change. Aucune
 * logique de décision n'est recopiée ici — tout passe par `eclat_decide` de `core/`. */
#ifndef ECLAT_RUNTIME_H
#define ECLAT_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "eclat/cap_budget.h"
#include "eclat/cap_decision.h"
#include "eclat/cap_policy.h"
#include "eclat_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bilan d'une exécution de module. Aucun chiffre n'est inventé : tout est mesuré. */
typedef struct eclat_run_result {
    uint32_t calls;   /* appels du module au courtier */
    uint32_t denials; /* refus rendus */
    uint64_t cpu_ns;  /* CPU consommé par le module, mesuré */
    uint64_t wall_ns; /* temps écoulé, mesuré */
    int killed;       /* 1 si le module a été tué par le budget */
    int module_rc;    /* code de retour de l'export « run », -1 si jamais atteint */
} eclat_run_result_t;

/* Motifs d'échec de eclat_runtime_run (valeurs strictement négatives). */
enum {
    ECLAT_RUN_OK = 0,
    ECLAT_RUN_ERR_ARG = -1,        /* argument invalide */
    ECLAT_RUN_ERR_DIGEST = -2,     /* empreinte du module ≠ politique (ECL-07) */
    ECLAT_RUN_ERR_LOAD = -3,       /* module illisible ou instanciation refusée */
    ECLAT_RUN_ERR_NO_ENTRY = -4,   /* export « run » absent */
    ECLAT_RUN_ERR_RUNTIME = -5     /* exception du runtime */
};

/* Vérifie l'empreinte, charge, instancie avec les imports `cap_*` soumis à politique, exécute
 * l'export « run », puis rend la main. `wasm` pointe sur le module en mémoire.
 *
 * Retourne ECLAT_RUN_OK si le module a été exécuté — y compris s'il a reçu des refus, car un
 * refus est un comportement normal de la politique. Retourne un code négatif sinon, et remplit
 * `err` avec le motif. `out` est renseigné dans tous les cas. */
int eclat_runtime_run(const uint8_t *wasm, size_t len, const eclat_policy_t *policy,
                      const eclat_platform_t *plat, eclat_run_result_t *out, char *err,
                      size_t errlen);

#ifdef __cplusplus
}
#endif

#endif /* ECLAT_RUNTIME_H */