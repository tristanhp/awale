#!/bin/bash
# Comptes (pseudo + mot de passe) et persistance de l'Elo entre les sessions.
source "$(dirname "$0")/lib_proto.sh"

relancer_serveur() {
  kill -INT "$SERVEUR"; wait "$SERVEUR" 2>/dev/null
  "$BIN/awale_serveur" "$PORT" >>"$LOG" 2>&1 &
  SERVEUR=$!
  for _ in $(seq 50); do
    (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break
    sleep 0.1
  done
}
# invite VAR pseudo : NICK (sans compte) ; compte VAR pseudo mdp : REGISTER ; connecter VAR pseudo mdp : LOGIN
invite()    { ouvrir "$1"; attendre "${!1}" "EVT HELLO"; envoyer "${!1}" "NICK $2\n"; attendre "${!1}" "OK NICK $2"; }
compte()    { ouvrir "$1"; attendre "${!1}" "EVT HELLO"; envoyer "${!1}" "REGISTER $2 $3\n"; attendre "${!1}" "OK NICK $2"; }
connecter() { ouvrir "$1"; attendre "${!1}" "EVT HELLO"; envoyer "${!1}" "LOGIN $2 $3\n"; attendre "${!1}" "OK NICK $2"; }
# reponse_a FD "commande" "texte" : 0 si la commande recoit une reponse contenant ce texte
reponse_a() { envoyer "$1" "$2\n"; attendre "$1" "$3"; }
elo_de() { envoyer "$1" "ELO\n"; attendre "$1" "OK ELO $2 $3"; }

echo "== creation de compte =="
verifier "fichier vide au depart" test ! -s "$DONNEES"
ouvrir A; attendre $A "EVT HELLO"
verifier "REGISTER sans mot de passe : syntaxe" reponse_a $A "REGISTER alice" "ERR syntaxe"
verifier "REGISTER sans rien : syntaxe" reponse_a $A "REGISTER" "ERR syntaxe"
verifier "REGISTER avec 3 mots : syntaxe" reponse_a $A "REGISTER alice secret1 trois" "ERR syntaxe"
verifier "mot de passe trop court" reponse_a $A "REGISTER alice court" "ERR mdp_invalide"
verifier "pseudo invalide" reponse_a $A "REGISTER a-b secret1" "ERR pseudo_invalide"
verifier "mot de passe trop long (65)" reponse_a $A "REGISTER alice $(printf 'x%.0s' $(seq 65))" "ERR mdp_invalide"
verifier "aucun compte cree par ces erreurs" test ! -s "$DONNEES"
verifier "REGISTER valide" reponse_a $A "REGISTER alice secret1" "OK NICK alice"
verifier "REGISTER une deuxieme fois sur la meme connexion" reponse_a $A "REGISTER bob secret2" "ERR deja_inscrit"
verifier "LOGIN quand on est deja inscrit" reponse_a $A "LOGIN alice secret1" "ERR deja_inscrit"
verifier "ligne du fichier : pseudo,elo,sel,hachage" grep -Eq '^alice,600,[0-9a-f]{32},[0-9a-f]{64}$' "$DONNEES"
verifier "le mot de passe n'est jamais ecrit en clair" bash -c "! grep -q secret1 '$DONNEES'"
ouvrir B; attendre $B "EVT HELLO"
verifier "REGISTER d'un pseudo en ligne" reponse_a $B "REGISTER alice autre123" "ERR pseudo_pris"
verifier "NICK sur un pseudo qui a un compte (en ligne)" reponse_a $B "NICK alice" "ERR pseudo_protege"
verifier "LOGIN d'un compte deja en ligne (bon mot de passe)" reponse_a $B "LOGIN alice secret1" "ERR pseudo_pris"
fermer A; sleep 0.3
verifier "REGISTER d'un pseudo qui a un compte (casse ignoree)" reponse_a $B "REGISTER ALICE autre123" "ERR pseudo_pris"
verifier "NICK sur un pseudo qui a un compte (hors ligne)" reponse_a $B "NICK Alice" "ERR pseudo_protege"

echo "== connexion =="
verifier "mauvais mot de passe" reponse_a $B "LOGIN alice mauvais1" "ERR identifiants_invalides"
verifier "pseudo sans compte : meme erreur" reponse_a $B "LOGIN personne secret1" "ERR identifiants_invalides"
verifier "mot de passe de format invalide : meme erreur" reponse_a $B "LOGIN alice x" "ERR identifiants_invalides"
verifier "mot de passe majuscules/minuscules inversees refuse" reponse_a $B "LOGIN alice SECRET1" "ERR identifiants_invalides"
verifier "LOGIN sans mot de passe : syntaxe" reponse_a $B "LOGIN alice" "ERR syntaxe"
verifier "LOGIN valide, pseudo tape en majuscules" reponse_a $B "LOGIN ALICE secret1" "OK NICK alice"
ouvrir C; attendre $C "EVT HELLO"; envoyer $C "WHO\n"; attendre $C "ERR etat_invalide"
# 5 mauvais mots de passe : deconnexion
for k in 1 2 3 4; do envoyer $C "LOGIN alice faux$k$k$k\n"; attendre $C "ERR identifiants_invalides"; done
envoyer $C "LOGIN alice faux555\n"
verifier "5e echec : ERR trop_d_echecs" attendre $C "ERR trop_d_echecs"
verifier "5e echec : connexion fermee" attendre_fermeture $C
fermer B

echo "== invite (NICK) : rien n'est sauvegarde =="
ouvrir H; attendre $H "EVT HELLO"
envoyer $H "NICK hugo\n"
verifier "NICK repond 'invite' (Elo non sauvegarde)" attendre $H "OK NICK hugo invite"
fermer H
ouvrir H; attendre $H "EVT HELLO"; envoyer $H "REGISTER hugo2 secret1\n"; attendre $H "OK NICK hugo2"
verifier "REGISTER ne repond pas 'invite'" bash -c "[[ '$LIGNE' != *invite* ]]"
fermer H
invite G "gaston"
envoyer $G "ELO 1500\n"; attendre $G "OK ELO gaston 1500"
verifier "l'Elo d'un invite n'est pas ecrit" bash -c "! grep -q gaston '$DONNEES'"
fermer G; sleep 0.2
invite G "gaston"
verifier "un invite qui revient repart a 600" elo_de $G gaston 600
fermer G

echo "== l'Elo suit le compte, meme apres un redemarrage =="
connecter A "alice" "secret1"
envoyer $A "ELO 900\n"; attendre $A "OK ELO alice 900"
verifier "ELO ecrit dans le fichier" grep -Eq '^alice,900,' "$DONNEES"
compte B "bob" "hunter22"
envoyer $A "CHALLENGE bob\n"; attendre $B "EVT CHALLENGE"; envoyer $B "ACCEPT\n"
attendre $A "EVT START"; attendre $B "EVT START"
fermer B     # forfait : alice (900) bat bob (600)
attendre $A "forfait"
envoyer $A "ELO\n"; attendre $A "OK ELO alice"; ELO_A=${LIGNE##* }
fermer A; sleep 0.2
verifier "alice a gagne des points" test "$ELO_A" -gt 900
verifier "alice sauvegardee ($ELO_A)" grep -Eq "^alice,$ELO_A," "$DONNEES"
ELO_B=$(grep '^bob,' "$DONNEES" | cut -d, -f2)
verifier "bob (perdant, deja parti) sauvegarde avec moins de 600" test "$ELO_B" -lt 600
SEL_A=$(grep '^alice,' "$DONNEES" | cut -d, -f3); HASH_A=$(grep '^alice,' "$DONNEES" | cut -d, -f4)
verifier "l'Elo change mais pas le mot de passe (sel et hachage inchanges)" test -n "$SEL_A" -a -n "$HASH_A"

relancer_serveur
verifier "le serveur annonce les comptes charges" grep -q "3 compte(s) charge(s)" "$LOG"
ouvrir A; attendre $A "EVT HELLO"
verifier "apres redemarrage : l'ancien mot de passe fonctionne" reponse_a $A "LOGIN alice secret1" "OK NICK alice"
verifier "alice retrouve son Elo" elo_de $A alice "$ELO_A"
ouvrir B; attendre $B "EVT HELLO"
verifier "apres redemarrage : mauvais mot de passe toujours refuse" reponse_a $B "LOGIN bob hunter23" "ERR identifiants_invalides"
verifier "bob se connecte et retrouve son Elo" reponse_a $B "LOGIN bob hunter22" "OK NICK bob"
verifier "bob retrouve son Elo" elo_de $B bob "$ELO_B"
fermer A; fermer B

echo "== sel : deux comptes avec le meme mot de passe =="
compte A "carol" "memepass1"; compte B "dave" "memepass1"
fermer A; fermer B; sleep 0.2
verifier "sels differents" test "$(grep '^carol,' "$DONNEES" | cut -d, -f3)" != "$(grep '^dave,' "$DONNEES" | cut -d, -f3)"
verifier "hachages differents" test "$(grep '^carol,' "$DONNEES" | cut -d, -f4)" != "$(grep '^dave,' "$DONNEES" | cut -d, -f4)"

echo "== fichier abime =="
ZOE_SEL=$(printf '0%.0s' $(seq 32))
ZOE_HASH=$(printf 'a%.0s' $(seq 64))
{
  echo "pseudo,elo,sel,hachage"
  echo "zoe,777,$ZOE_SEL,$ZOE_HASH"
  echo "n importe quoi"
  echo "ancien,500"                       # ancien format, sans mot de passe
  echo "max,abc,$ZOE_SEL,$ZOE_HASH"        # Elo faux
  echo "tom,5,court,$ZOE_HASH"             # sel faux
  echo "bad pseudo,5,$ZOE_SEL,$ZOE_HASH"
  echo "zoe,1,$ZOE_SEL,$ZOE_HASH"          # doublon
  echo "leo,-50,$ZOE_SEL,$ZOE_HASH"
} > "$DONNEES"
relancer_serveur
verifier "serveur vivant malgre les lignes invalides" serveur_vivant
verifier "seules les lignes valides sont chargees (zoe, leo)" grep -q "2 compte(s) charge(s)" "$LOG"
ouvrir A; attendre $A "EVT HELLO"
verifier "compte a l'ancien format : inutilisable" reponse_a $A "LOGIN ancien secret1" "ERR identifiants_invalides"
verifier "le pseudo de l'ancien format est libre" reponse_a $A "NICK ancien" "OK NICK ancien"
fermer A

echo "== sauvegarde impossible =="
kill -INT "$SERVEUR"; wait "$SERVEUR" 2>/dev/null
AWALE_DONNEES=/nonexistant/dossier/joueurs.csv "$BIN/awale_serveur" "$PORT" >>"$LOG" 2>&1 &
SERVEUR=$!
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break; sleep 0.1; done
ouvrir A; attendre $A "EVT HELLO"
verifier "REGISTER impossible a enregistrer : erreur claire" reponse_a $A "REGISTER alice secret1" "ERR sauvegarde_impossible"
verifier "serveur toujours vivant" serveur_vivant
verifier "pas de compte fantome" reponse_a $A "LOGIN alice secret1" "ERR identifiants_invalides"
verifier "mais on peut jouer en invite" reponse_a $A "NICK alice" "OK NICK alice"
fermer A

arreter_serveur
bilan
