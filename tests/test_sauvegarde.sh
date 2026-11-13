#!/bin/bash
# Persistance de l'Elo entre les sessions (fichier CSV, retrouve par le pseudo).
source "$(dirname "$0")/lib_proto.sh"

# arrete le serveur (sans verifier les erreurs memoire) puis le relance sur le meme fichier
relancer_serveur() {
  kill -INT "$SERVEUR"; wait "$SERVEUR" 2>/dev/null
  "$BIN/awale_serveur" "$PORT" >>"$LOG" 2>&1 &
  SERVEUR=$!
  for _ in $(seq 50); do
    (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break
    sleep 0.1
  done
}
# inscrire VAR pseudo : ouvre une connexion et s'inscrit
inscrire() { ouvrir "$1"; attendre "${!1}" "EVT HELLO"; envoyer "${!1}" "NICK $2\n"; attendre "${!1}" "OK NICK $2"; }
# elo_de FD pseudo valeur : 0 si ELO donne bien cette valeur
elo_de() { envoyer "$1" "ELO\n"; attendre "$1" "OK ELO $2 $3"; }

echo "== premiere session =="
verifier "fichier absent ou vide au depart : aucun joueur connu" test ! -s "$DONNEES"
inscrire A "alice"
verifier "nouveau joueur : 600" elo_de $A alice 600
envoyer $A "ELO 900\n"; attendre $A "OK ELO alice 900"
verifier "ELO ecrit dans le fichier" grep -qx "alice,900" "$DONNEES"
verifier "le fichier a un en-tete CSV" test "$(head -1 "$DONNEES")" = "pseudo,elo"
inscrire B "bob"
envoyer $A "CHALLENGE bob\n"; attendre $B "EVT CHALLENGE"; envoyer $B "ACCEPT\n"
attendre $A "EVT START"; attendre $B "EVT START"
fermer B     # forfait : alice (900) bat bob (600), les deux Elo changent
attendre $A "forfait"
envoyer $A "ELO\n"; attendre $A "OK ELO alice"; ELO_A=${LIGNE##* }
envoyer $A "QUIT\n"; attendre $A "OK QUIT"
verifier "fin de partie : alice gagne des points" test "$ELO_A" -gt 900
verifier "fin de partie : alice sauvegardee ($ELO_A)" grep -qx "alice,$ELO_A" "$DONNEES"
verifier "fin de partie : bob sauvegarde (perdant)" grep -Eq "^bob,[0-9]+$" "$DONNEES"
ELO_B=$(grep '^bob,' "$DONNEES" | cut -d, -f2)
verifier "bob a perdu des points" test "$ELO_B" -lt 600

echo "== apres redemarrage du serveur =="
relancer_serveur
verifier "le serveur annonce les joueurs charges" grep -q "2 joueur(s) charge(s)" "$LOG"
inscrire A "alice"
verifier "alice retrouve son Elo" elo_de $A alice "$ELO_A"
inscrire B "BOB"
verifier "pseudo retrouve sans tenir compte de la casse" elo_de $B BOB "$ELO_B"
inscrire C "carol"
verifier "joueur inconnu : 600" elo_de $C carol 600
envoyer $C "ELO 1234\n"; attendre $C "OK ELO carol 1234"
fermer C; sleep 0.2
inscrire C "carol"
verifier "carol retrouve ELO 1234 apres reconnexion (meme session serveur)" elo_de $C carol 1234
fermer A; fermer B; fermer C

echo "== fichier abime =="
printf 'pseudo,elo\nzoe,777\nn importe quoi\nmax,abc\n,12\nbad pseudo,5\ntom,99999999999999999999\nzoe,1\nleo,-50\n' > "$DONNEES"
relancer_serveur
verifier "serveur vivant malgre les lignes invalides" serveur_vivant
verifier "seules les lignes valides sont chargees (zoe, leo)" grep -q "2 joueur(s) charge(s)" "$LOG"
inscrire A "zoe";  verifier "zoe : premiere ligne gardee, doublon ignore" elo_de $A zoe 777
inscrire B "leo";  verifier "Elo negatif sauvegarde" elo_de $B leo -50
inscrire C "max";  verifier "ligne invalide : max repart a 600" elo_de $C max 600
fermer A; fermer B; fermer C

echo "== sauvegarde impossible =="
# le serveur doit continuer a fonctionner si le fichier ne peut plus etre ecrit
kill -INT "$SERVEUR"; wait "$SERVEUR" 2>/dev/null
AWALE_DONNEES=/nonexistant/dossier/joueurs.csv "$BIN/awale_serveur" "$PORT" >>"$LOG" 2>&1 &
SERVEUR=$!
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break; sleep 0.1; done
inscrire A "alice"
envoyer $A "ELO 700\n"
verifier "echec d'ecriture : la commande repond quand meme" attendre $A "OK ELO alice 700"
verifier "echec d'ecriture : serveur toujours vivant" serveur_vivant
fermer A

arreter_serveur
bilan
