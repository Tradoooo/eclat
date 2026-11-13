#!/bin/ksh
# SPDX-License-Identifier: Apache-2.0
# Éclat — scénario de preuve ECL-14, exécuté DANS l'image QNX 8.0 au démarrage.
#
# Ce script ne fait rien de magique : il lance des processus avec des TYPES de sécurité
# (`on -T <type>`), puis mesure ce que le noyau autorise et refuse. Aucune sortie n'est
# retouchée ; la console série de la VM est la preuve archivée.

PATH=/proc/boot:/system/bin
export PATH
echo ""
echo "================================================================"
echo " ECL-14 - une politique de securite QNX interdit au runtime"
echo "          tout acces hors du courtier"
echo "================================================================"
echo ""

echo "--- [1] la politique source, telle qu'elle a ete compilee ---"
cat /proc/boot/politique.secpol
echo ""

echo "--- [2] poussee dans procnto ---"
secpolpush
echo "secpolpush rc=$?"
echo ""

echo "--- [3] le courtier : type eclat_broker, publie son canal ---"
on -T eclat_broker /proc/boot/eclat-broker /proc/boot/pol-broker.toml &
sleep 2
echo ""

echo "--- [4] le serveur temoin : type eclat_intrus, publie un AUTRE canal ---"
on -T eclat_intrus /proc/boot/eclat-sonde serve intrus &
sleep 2
echo ""

echo "--- [5] le runtime : type eclat_runtime, il tente les deux canaux + fork ---"
on -T eclat_runtime /proc/boot/eclat-sonde cap intrus
echo ""

echo "--- [6] le client du courtier, meme type de runtime ---"
on -T eclat_runtime /proc/boot/eclat-client
echo ""

echo "--- [7] COUCHE 2 : un processus root ordinaire qui abandonne TOUTES ses abilities ---"
/proc/boot/eclat-sonde abilities
echo ""

echo "================================================================"
echo " fin du scenario"
echo "================================================================"