#!/bin/bash
# Tests du protocole (inscription, robustesse reseau) : lance un vrai serveur et lui parle
# avec des sockets brutes.
#   tests/test_proto.sh            (ou : make test)
#   BIN=bin-debug tests/test_proto.sh   pour tester la version compilee avec ASan/UBSan
source "$(dirname "$0")/lib_proto.sh"

# ---- tests ----

echo "== inscription =="
ouvrir A; attendre $A "EVT HELLO"; verifier "message de bienvenue" true
envoyer $A "WHO\n";               verifier "WHO avant connexion refuse" attendre $A "ERR etat_invalide"
envoyer $A "NICK alice\n";         verifier "NICK n'existe plus (compte obligatoire)" attendre $A "ERR commande_inconnue"
envoyer $A "REGISTER\n";          verifier "REGISTER sans pseudo : syntaxe" attendre $A "ERR syntaxe"
envoyer $A "REGISTER a b motdepasse1\n"; verifier "pseudo avec espace : syntaxe" attendre $A "ERR syntaxe"
envoyer $A "REGISTER a-b motdepasse1\n";          verifier "pseudo avec tiret invalide" attendre $A "ERR pseudo_invalide"
envoyer $A "REGISTER abcdefghijklmnopq motdepasse1\n"; verifier "pseudo de 17 caracteres invalide" attendre $A "ERR pseudo_invalide"
envoyer $A "register alice motdepasse1\n"; verifier "commande en minuscules acceptee" attendre $A "OK NICK alice"
envoyer $A "REGISTER autre motdepasse1\n"; verifier "deuxieme inscription refusee" attendre $A "ERR deja_inscrit"

ouvrir B; attendre $B "EVT HELLO"
envoyer $B "REGISTER ALICE motdepasse1\n";        verifier "pseudo deja pris (casse ignoree)" attendre $B "ERR pseudo_pris"
envoyer $B "REGISTER bob motdepasse1\n"; verifier "bob s'inscrit" attendre $B "OK NICK bob"
verifier "alice voit arriver bob" attendre $A "EVT JOIN bob"

echo "== liste des joueurs =="
envoyer $B "WHO\n";               verifier "WHO contient alice et bob" attendre $B "OK WHO"
[[ $LIGNE == *alice:libre* && $LIGNE == *bob:libre* ]] && verifier "WHO : alice:libre bob:libre" true || verifier "WHO : alice:libre bob:libre" false
envoyer $B "HELP\n";              verifier "HELP liste les commandes" attendre $B "OK HELP REGISTER LOGIN WHO HELP QUIT"

echo "== commandes invalides =="
envoyer $A "DANSER\n";            verifier "commande inconnue" attendre $A "ERR commande_inconnue"
envoyer $A "\n   \n";             envoyer $A "WHO\n"
verifier "lignes vides ignorees" attendre $A "OK WHO"
envoyer $A "WHO\r\n";             verifier "fin de ligne \\r\\n acceptee" attendre $A "OK WHO"

echo "== flux TCP : lignes coupees et rafales =="
ouvrir C; attendre $C "EVT HELLO"
envoyer $C "REGI"; sleep 0.2; envoyer $C "STER ca"; sleep 0.2; envoyer $C "rol motdepasse1\nWH"; sleep 0.2
verifier "REGISTER coupe en 3 morceaux" attendre $C "OK NICK carol"
envoyer $C "O\n";                 verifier "WHO coupe entre deux envois" attendre $C "OK WHO"
envoyer $C "WHO\nWHO\nHELP\nDANSER\n"
verifier "rafale : 1re reponse" attendre $C "OK WHO"
verifier "rafale : 2e reponse"  attendre $C "OK WHO"
verifier "rafale : 3e reponse"  attendre $C "OK HELP"
verifier "rafale : 4e reponse"  attendre $C "ERR commande_inconnue"

echo "== lignes trop longues =="
ouvrir D; attendre $D "EVT HELLO"
L1023=$(head -c 1023 /dev/zero | tr '\0' 'x')
envoyer $D "$L1023\n";            verifier "ligne de 1023 caracteres : acceptee" attendre $D "ERR commande_inconnue"
L2000=$(head -c 2000 /dev/zero | tr '\0' 'x')
envoyer $D "$L2000";              verifier "2000 caracteres sans \\n : refuse" attendre $D "ERR ligne_trop_longue"
verifier "...puis connexion fermee" attendre_fermeture $D
verifier "serveur toujours vivant" serveur_vivant

echo "== deconnexions =="
envoyer $C "QUIT\n";              verifier "QUIT : OK QUIT" attendre $C "OK QUIT"
verifier "QUIT : connexion fermee" attendre_fermeture $C
verifier "alice voit partir carol" attendre $A "EVT LEAVE carol"
# depart brutal : fermeture sans QUIT
ouvrir E; attendre $E "EVT HELLO"; envoyer $E "REGISTER eve motdepasse1\n"; attendre $E "OK NICK eve"
attendre $A "EVT JOIN eve"
fermer E
verifier "depart brutal : alice voit partir eve" attendre $A "EVT LEAVE eve"
# l'ancien pseudo est de nouveau libre
ouvrir F; attendre $F "EVT HELLO"; envoyer $F "LOGIN eve motdepasse1\n"
verifier "pseudo reutilisable apres depart (LOGIN)" attendre $F "OK NICK eve"
fermer F; attendre $A "EVT LEAVE eve"
# connexion fermee sans rien envoyer
ouvrir G; fermer G; sleep 0.2
verifier "client muet qui part : serveur vivant" serveur_vivant

echo "== client lent qui ne lit jamais =="
ouvrir H; attendre $H "EVT HELLO"; envoyer $H "REGISTER lent motdepasse1\n"; attendre $H "OK NICK lent"
attendre $A "EVT JOIN lent"
# 400000 commandes envoyees sans jamais lire les reponses : les files se remplissent
yes HELP | head -n 400000 >&$H 2>/dev/null
sleep 0.5
verifier "alice voit partir le client lent (deconnecte par le serveur)" attendre $A "EVT LEAVE lent"
verifier "serveur toujours vivant" serveur_vivant
fermer H

echo "== serveur plein =="
sleep 0.5
PLEINS=()
for k in $(seq 1 70); do ouvrir "P$k"; PLEINS+=("P$k"); done
NB_PLEIN=0
for p in "${PLEINS[@]}"; do
  read -r -t 2 -u "${!p}" l 2>/dev/null   # premiere ligne : bienvenue, ou refus
  [[ $l == *serveur_plein* ]] && NB_PLEIN=$((NB_PLEIN + 1))
done
verifier "au-dela de 64 clients : ERR serveur_plein ($NB_PLEIN refus)" test "$NB_PLEIN" -ge 1
verifier "serveur toujours vivant" serveur_vivant
for p in "${PLEINS[@]}"; do fermer "$p"; done
sleep 0.5
ouvrir Z; verifier "nouvelles connexions acceptees apres liberation" attendre $Z "EVT HELLO"
fermer Z

echo "== client interactif (awale_client) =="
SORTIE=$(printf 'REGISTER zed motdepasse1\nWHO\nDANSER\n' | timeout 5 "$BIN/awale_client" 127.0.0.1 "$PORT")
verifier "client : inscription affichee" grep -q "connecte sous le pseudo zed" <<<"$SORTIE"
verifier "client : liste des joueurs affichee" grep -q "  - zed (600) (libre)" <<<"$SORTIE"
verifier "client : erreur traduite" grep -q "Erreur : commande inconnue" <<<"$SORTIE"
verifier "client : se termine quand le serveur ferme" grep -q "Connexion fermee par le serveur" <<<"$SORTIE"

echo "== arret propre =="
arreter_serveur
bilan
