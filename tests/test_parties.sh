#!/bin/bash
# Tests des defis et des parties (CHALLENGE, ACCEPT, DECLINE, CANCEL, MOVE, BOARD, forfait).
#   tests/test_parties.sh            (ou : make test)
#   BIN=bin-debug tests/test_parties.sh   pour tester la version compilee avec ASan/UBSan
source "$(dirname "$0")/lib_proto.sh"

# ---- utilitaires propres aux parties ----

# inscrire VAR pseudo : ouvre une connexion et s'inscrit
inscrire() { ouvrir "$1"; attendre "${!1}" "EVT HELLO"; envoyer "${!1}" "NICK $2\n"; attendre "${!1}" "OK NICK $2"; }

# etat_de FD pseudo etat : 0 si WHO (demande sur FD) donne pseudo:etat
etat_de() { envoyer "$1" "WHO\n"; attendre "$1" "OK WHO" && [[ $LIGNE == *" $2:$3"* ]]; }

# demarrer A B : A defie B, B accepte ; attend l'EVT START des deux ; PREMIER = pseudo qui commence
# (A et B sont des NOMS de variables ; nA et nB leurs pseudos, donnes en 3e et 4e arguments)
demarrer() {
  envoyer "${!1}" "CHALLENGE $4\n"; attendre "${!2}" "EVT CHALLENGE $3" || return 1
  envoyer "${!2}" "ACCEPT\n"
  attendre "${!1}" "EVT START" || return 1; PREMIER=${LIGNE##* }; ID=$(cut -d" " -f3 <<<"$LIGNE")
  attendre "${!2}" "EVT START" || return 1
  attendre "${!1}" "EVT BOARD" && attendre "${!2}" "EVT BOARD"
}

raison_ok() { [[ $1 == *" score" || $1 == *" blocage" ]]; }
contient() { [[ $1 == *"$2"* ]]; }

# renvoie 0 si aucune ligne contenant $2 n'arrive sur $1 pendant 0,3 s
rien_recu() {
  local l
  while read -r -t 0.3 -u "$1" l 2>/dev/null; do [[ $l == *"$2"* ]] && return 1; done
  return 0
}

echo "== defis =="
inscrire A "alice"; inscrire B "bob"; inscrire C "carol"
envoyer $A "CHALLENGE\n";        verifier "CHALLENGE sans argument : syntaxe" attendre $A "ERR syntaxe"
envoyer $A "CHALLENGE zorro\n";  verifier "defier un inconnu" attendre $A "ERR joueur_inconnu"
envoyer $A "CHALLENGE alice\n";  verifier "se defier soi-meme" attendre $A "ERR defi_soi"
envoyer $A "CHALLENGE ALICE\n";  verifier "se defier soi-meme (casse ignoree)" attendre $A "ERR defi_soi"
ouvrir N; attendre $N "EVT HELLO"
envoyer $N "CHALLENGE alice\n";  verifier "CHALLENGE sans pseudo inscrit refuse" attendre $N "ERR etat_invalide"
envoyer $A "ACCEPT\n";           verifier "ACCEPT sans defi" attendre $A "ERR etat_invalide"
envoyer $A "DECLINE\n";          verifier "DECLINE sans defi" attendre $A "ERR etat_invalide"
envoyer $A "CANCEL\n";           verifier "CANCEL sans defi" attendre $A "ERR etat_invalide"
envoyer $A "CHALLENGE bob\n"
verifier "le defiant recoit OK CHALLENGE" attendre $A "OK CHALLENGE bob"
verifier "le defie recoit EVT CHALLENGE" attendre $B "EVT CHALLENGE alice"
verifier "WHO : alice defi_envoye" etat_de $C "alice" "defi_envoye"
verifier "WHO : bob defi_recu" etat_de $C "bob" "defi_recu"
envoyer $A "CHALLENGE carol\n";  verifier "deuxieme defi pendant un defi" attendre $A "ERR etat_invalide"
envoyer $C "CHALLENGE bob\n";    verifier "defier un joueur deja defie" attendre $C "ERR joueur_occupe"
envoyer $C "CHALLENGE alice\n";  verifier "defier un joueur qui a defie" attendre $C "ERR joueur_occupe"
envoyer $B "DECLINE\n"
verifier "DECLINE : OK pour le defie" attendre $B "OK DECLINE"
verifier "DECLINE : EVT DECLINED pour le defiant" attendre $A "EVT DECLINED bob"
verifier "apres DECLINE : alice libre" etat_de $C "alice" "libre"
verifier "apres DECLINE : bob libre" etat_de $C "bob" "libre"
envoyer $A "CHALLENGE bob\n"; attendre $B "EVT CHALLENGE alice"
envoyer $A "CANCEL\n"
verifier "CANCEL : OK pour le defiant" attendre $A "OK CANCEL"
verifier "CANCEL : EVT CANCELLED pour le defie" attendre $B "EVT CANCELLED alice"
verifier "apres CANCEL : bob libre" etat_de $C "bob" "libre"
# depart pendant un defi
inscrire D "dave"
envoyer $D "CHALLENGE carol\n"; attendre $C "EVT CHALLENGE dave"
fermer D
verifier "le defiant part : le defie est prevenu" attendre $C "EVT CANCELLED dave"
verifier "le defiant part : carol de nouveau libre" etat_de $C "carol" "libre"
inscrire D "dave"
envoyer $C "CHALLENGE dave\n"; attendre $D "EVT CHALLENGE carol"
fermer D
verifier "le defie part : le defiant est prevenu" attendre $C "EVT DECLINED dave"
verifier "le defie part : carol de nouveau libre" etat_de $C "carol" "libre"
fermer C; fermer N

echo "== demarrage d'une partie =="
verifier "alice defie bob, bob accepte" demarrer A B alice bob
verifier "le premier joueur est alice ou bob" test "$PREMIER" = alice -o "$PREMIER" = bob
inscrire C "carol"
verifier "WHO : alice en_partie" etat_de $C "alice" "en_partie"
verifier "WHO : bob en_partie" etat_de $C "bob" "en_partie"
envoyer $C "CHALLENGE alice\n"; verifier "defier un joueur en partie" attendre $C "ERR joueur_occupe"
envoyer $A "CHALLENGE carol\n"; verifier "defier pendant une partie" attendre $A "ERR etat_invalide"
envoyer $A "ACCEPT\n";          verifier "ACCEPT pendant une partie" attendre $A "ERR etat_invalide"
envoyer $C "MOVE 1\n";          verifier "MOVE sans etre en partie" attendre $C "ERR etat_invalide"
envoyer $C "BOARD\n";           verifier "BOARD sans etre en partie" attendre $C "ERR etat_invalide"

echo "== coups =="
if [ "$PREMIER" = alice ]; then J1=$A; J2=$B; else J1=$B; J2=$A; fi
envoyer $J2 "MOVE 1\n";  verifier "MOVE hors de son tour" attendre $J2 "ERR pas_votre_tour"
for mauvais in "MOVE" "MOVE 0" "MOVE 7" "MOVE abc" "MOVE 1 2" "MOVE 3x" "MOVE -1"; do
  envoyer $J1 "$mauvais\n"; verifier "'$mauvais' : syntaxe" attendre $J1 "ERR syntaxe"
done
envoyer $J1 "BOARD\n"
attendre $J1 "EVT BOARD"
verifier "plateau de depart : 4 graines partout" test "$(cut -d' ' -f4-15 <<<"$LIGNE")" = "4 4 4 4 4 4 4 4 4 4 4 4"
verifier "BOARD ne repond qu'a celui qui le demande" rien_recu $J2 "EVT BOARD"
envoyer $J1 "MOVE 1\n"
verifier "le joueur voit EVT MOVE" attendre $J1 "EVT MOVE"
verifier "l'adversaire voit EVT MOVE" attendre $J2 "EVT MOVE"
attendre $J1 "EVT BOARD"; PLATEAU1=$LIGNE; attendre $J2 "EVT BOARD"; PLATEAU2=$LIGNE
verifier "les deux joueurs recoivent le meme plateau" test "$PLATEAU1" = "$PLATEAU2"
# si Sud joue MOVE 1 : case 0 vide, cases 1 a 4 a 5 ; si Nord joue MOVE 1 : case 6 vide, cases 7 a 10 a 5
if [ "$PREMIER" = alice ]; then ATTENDU="0 5 5 5 5 4 4 4 4 4 4 4"; else ATTENDU="4 4 4 4 4 4 0 5 5 5 5 4"; fi
verifier "premier coup : graines semees dans les bonnes cases" test "$(cut -d' ' -f4-15 <<<"$PLATEAU1")" = "$ATTENDU"
verifier "le tour passe a l'adversaire" test "${PLATEAU1##* }" = "$([ "$PREMIER" = alice ] && echo 1 || echo 0)"
envoyer $J1 "MOVE 2\n"; verifier "rejouer deux fois de suite" attendre $J1 "ERR pas_votre_tour"

echo "== partie complete au hasard =="
# a chaque tour : un MOVE aleatoire du joueur dont c'est le tour ; en cas de coup illegal on recommence.
# A chaque plateau, les 12 cases et les 2 scores totalisent toujours 48 graines.
TOUR=$([ "$PREMIER" = alice ] && echo 1 || echo 0)   # apres le premier coup joue plus haut
if [ "$PREMIER" = alice ]; then SUD=$A; NORD=$B; else SUD=$B; NORD=$A; fi
# (le premier joueur a ete Sud ou Nord selon le tirage : on reconstruit qui joue quoi)
[ "$PREMIER" = alice ] && { SUD=$A; NORD=$B; } || { SUD=$B; NORD=$A; }
# Sud = le defiant = alice ; Nord = bob (le tirage ne change que le premier a jouer)
SUD=$A; NORD=$B
FIN=0; MAUVAISES_SOMMES=0; NB_PLATEAUX=0
for coup in $(seq 600); do
  [ "$TOUR" = 0 ] && M=$SUD || M=$NORD
  [ "$TOUR" = 0 ] && O=$NORD || O=$SUD
  envoyer $M "MOVE $((RANDOM % 6 + 1))\n"
  # reponse du joueur : un plateau (coup accepte) ou une erreur (coup illegal)
  REP=""
  while read -r -t 3 -u $M l 2>/dev/null; do
    l=${l%$'\r'}
    if [[ $l == "EVT BOARD"* || $l == "ERR coup_illegal" ]]; then REP=$l; break; fi
  done
  [ "$REP" = "ERR coup_illegal" ] && continue
  [ -z "$REP" ] && break
  # meme plateau cote adversaire
  while read -r -t 3 -u $O l 2>/dev/null; do [[ $l == "EVT BOARD"* ]] && break; done
  read -ra CH <<<"$REP"; SOMME=0
  for k in $(seq 3 16); do SOMME=$((SOMME + CH[k])); done
  NB_PLATEAUX=$((NB_PLATEAUX + 1)); [ "$SOMME" -ne 48 ] && MAUVAISES_SOMMES=$((MAUVAISES_SOMMES + 1))
  TOUR=${CH[17]}
  # la fin de partie arrive juste apres le dernier plateau
  if read -r -t 0.1 -u $M l 2>/dev/null && [[ $l == "EVT END"* ]]; then FIN=1; FINLIGNE=${l%$'\r'}; break; fi
done
verifier "partie jouee jusqu'a la fin ($NB_PLATEAUX plateaux)" test "$FIN" -eq 1
verifier "somme des graines toujours egale a 48" test "$MAUVAISES_SOMMES" -eq 0
verifier "le joueur recoit EVT END ($FINLIGNE)" contient "$FINLIGNE" "EVT END"
verifier "l'adversaire recoit le meme EVT END" attendre $O "EVT END"
verifier "raison de fin : score ou blocage" raison_ok "$FINLIGNE"
verifier "apres la fin : alice libre" etat_de $C "alice" "libre"
verifier "apres la fin : bob libre" etat_de $C "bob" "libre"
envoyer $A "MOVE 1\n"; verifier "MOVE apres la fin de partie" attendre $A "ERR etat_invalide"
envoyer $A "CHALLENGE bob\n"; verifier "les joueurs peuvent se redefier" attendre $A "OK CHALLENGE bob"
envoyer $A "CANCEL\n"; attendre $A "OK CANCEL"

echo "== qui commence : tirage au sort =="
declare -A VU
for k in $(seq 20); do
  inscrire X "xa$k"; inscrire Y "xb$k"
  demarrer X Y "xa$k" "xb$k" && { [ "$PREMIER" = "xa$k" ] && VU[defiant]=1 || VU[defie]=1; }
  fermer X; fermer Y
done
verifier "sur 20 parties, les deux joueurs ont commence au moins une fois" test "${#VU[@]}" -eq 2

echo "== forfait =="
inscrire F "fanny"; inscrire G "gaston"
demarrer F G fanny gaston
fermer F
verifier "l'adversaire recoit EVT END ... forfait" attendre $G "forfait"
verifier "le gagnant du forfait est l adversaire" contient "$LIGNE" " gaston "
verifier "apres le forfait : gaston libre" etat_de $G "gaston" "libre"
envoyer $G "MOVE 1\n"; verifier "MOVE apres un forfait" attendre $G "ERR etat_invalide"
fermer G

echo "== deux joueurs partent en meme temps =="
inscrire F "fanny"; inscrire G "gaston"
demarrer F G fanny gaston
fermer F; fermer G; sleep 0.3
verifier "serveur toujours vivant" serveur_vivant
inscrire F "fanny"; inscrire G "gaston"
verifier "une nouvelle partie demarre ensuite" demarrer F G fanny gaston
fermer F; fermer G

echo "== deux parties en meme temps =="
inscrire P1 "p1a"; inscrire P2 "p1b"; inscrire Q1 "p2a"; inscrire Q2 "p2b"
demarrer P1 P2 p1a p1b; demarrer Q1 Q2 p2a p2b
# le coup d'une partie n'apparait pas dans l'autre
if [ "$PREMIER" = p2a ]; then QJ=$Q1; else QJ=$Q2; fi
envoyer $QJ "MOVE 1\n"
verifier "le coup est vu dans sa partie" attendre $Q1 "EVT MOVE"
verifier "le coup n'est pas vu dans l'autre partie" rien_recu $P1 "EVT MOVE"
fermer P1; fermer P2; fermer Q1; fermer Q2

echo "== observateurs =="
inscrire O "oscar"
envoyer $O "GAMES\n"; attendre $O "OK GAMES"
verifier "GAMES sans partie : liste vide" test "$LIGNE" = "OK GAMES"
inscrire G1 "ga"; inscrire G2 "gb"
demarrer G1 G2 ga gb; PARTIE=$ID
envoyer $O "GAMES\n"; attendre $O "OK GAMES"
verifier "GAMES liste la partie ($LIGNE)" contient "$LIGNE" "$PARTIE:ga-gb"
for mauvais in "WATCH" "WATCH abc" "WATCH 1 2" "WATCH 0x"; do
  envoyer $O "$mauvais\n"; verifier "'$mauvais' : syntaxe" attendre $O "ERR syntaxe"
done
envoyer $O "WATCH 31\n";  verifier "WATCH d'une partie qui n'existe pas" attendre $O "ERR partie_inconnue"
envoyer $O "WATCH -1\n";  verifier "WATCH d'un numero negatif" attendre $O "ERR partie_inconnue"
envoyer $O "UNWATCH\n";   verifier "UNWATCH sans observer" attendre $O "ERR etat_invalide"
envoyer $G1 "WATCH $PARTIE\n"; verifier "un joueur ne peut pas observer" attendre $G1 "ERR etat_invalide"
envoyer $O "WATCH $PARTIE\n"
verifier "WATCH : OK avec les noms des joueurs" attendre $O "OK WATCH $PARTIE ga gb"
verifier "WATCH : le plateau courant est envoye" attendre $O "EVT BOARD"
verifier "WHO : oscar observe" etat_de $C "oscar" "observe"
envoyer $O "MOVE 1\n";  verifier "un observateur ne peut pas jouer" attendre $O "ERR etat_invalide"
envoyer $O "CHALLENGE carol\n"; verifier "un observateur ne peut pas defier" attendre $O "ERR etat_invalide"
envoyer $O "WATCH $PARTIE\n"; verifier "WATCH deux fois" attendre $O "ERR etat_invalide"
envoyer $C "CHALLENGE oscar\n"; verifier "defier un observateur" attendre $C "ERR joueur_occupe"
envoyer $O "BOARD\n"; verifier "BOARD pour un observateur" attendre $O "EVT BOARD"
# un coup de la partie arrive a l'observateur
if [ "$PREMIER" = ga ]; then JP=$G1; else JP=$G2; fi
envoyer $JP "MOVE 1\n"
verifier "l'observateur voit EVT MOVE" attendre $O "EVT MOVE"
verifier "l'observateur voit EVT BOARD" attendre $O "EVT BOARD"
# UNWATCH : plus aucun evenement
envoyer $O "UNWATCH\n"; verifier "UNWATCH : OK" attendre $O "OK UNWATCH"
verifier "apres UNWATCH : oscar libre" etat_de $C "oscar" "libre"
if [ "$PREMIER" = ga ]; then JS=$G2; else JS=$G1; fi
envoyer $JS "MOVE 1\n"; attendre $JS "EVT BOARD"
verifier "apres UNWATCH : plus d'evenements de la partie" rien_recu $O "EVT MOVE"
# un observateur qui part ne gene pas la partie
envoyer $O "WATCH $PARTIE\n"; attendre $O "OK WATCH"
fermer O; sleep 0.2
verifier "un observateur part : serveur vivant" serveur_vivant
envoyer $JP "MOVE 2\n"
verifier "un observateur part : la partie continue" attendre $JP "EVT MOVE"
# la fin de la partie (forfait) est vue par les observateurs, qui redeviennent libres
inscrire O "oscar"; inscrire O2 "olga"
envoyer $O "WATCH $PARTIE\n"; attendre $O "OK WATCH"
envoyer $O2 "WATCH $PARTIE\n"; attendre $O2 "OK WATCH"
fermer G1
verifier "fin de partie : oscar recoit EVT END forfait" attendre $O "forfait"
verifier "fin de partie : olga recoit EVT END forfait" attendre $O2 "forfait"
verifier "fin de partie : oscar de nouveau libre" etat_de $C "oscar" "libre"
verifier "fin de partie : olga de nouveau libre" etat_de $C "olga" "libre"
envoyer $O "GAMES\n"; attendre $O "OK GAMES"
verifier "GAMES : la partie terminee a disparu" test "$LIGNE" = "OK GAMES"
fermer G2; fermer O2

echo "== limite d'observateurs =="
inscrire G1 "ga"; inscrire G2 "gb"
demarrer G1 G2 ga gb; PARTIE=$ID
NB_OBS=0
for k in $(seq 8); do
  inscrire "W$k" "w$k";
  eval "WFD=\$W$k"; envoyer $WFD "WATCH $PARTIE\n"; attendre $WFD "OK WATCH" && NB_OBS=$((NB_OBS + 1))
done
verifier "8 observateurs acceptes ($NB_OBS)" test "$NB_OBS" -eq 8
envoyer $O "WATCH $PARTIE\n"
verifier "le 9e observateur est refuse" attendre $O "ERR trop_observateurs"
for k in $(seq 8); do fermer "W$k"; done
fermer G1; fermer G2; fermer C; fermer O

echo "== 32 parties simultanees =="
# 32 parties = 64 clients = toute la table du serveur : on libere d'abord les connexions restantes
fermer A; fermer B; fermer C; sleep 1
NB_DEMARREES=0
for k in $(seq 32); do
  inscrire "S${k}a" "s${k}a"; inscrire "S${k}b" "s${k}b"
  demarrer "S${k}a" "S${k}b" "s${k}a" "s${k}b" && NB_DEMARREES=$((NB_DEMARREES + 1))
done
verifier "32 parties en cours en meme temps ($NB_DEMARREES demarrees)" test "$NB_DEMARREES" -eq 32
verifier "serveur toujours vivant" serveur_vivant
for k in $(seq 32); do fermer "S${k}a"; fermer "S${k}b"; done
sleep 0.5
inscrire F "fanny"; inscrire G "gaston"
verifier "apres liberation, une nouvelle partie demarre" demarrer F G fanny gaston

echo "== arret propre =="
arreter_serveur
bilan
