#!/bin/bash
# Fonctions communes aux scripts de test (a inclure avec : source "$(dirname "$0")/lib_proto.sh").
# Lance un vrai serveur sur un port aleatoire et parle avec des sockets brutes (/dev/tcp de bash).
#   BIN=bin-debug  pour tester la version compilee avec ASan/UBSan
BIN=${BIN:-bin}
PORT=$((20000 + RANDOM % 20000))
LOG=$(mktemp)
NB_OK=0
NB_KO=0

cleanup() { kill "$SERVEUR" 2>/dev/null; rm -f "$LOG"; }
trap cleanup EXIT
trap '' PIPE   # ecrire vers un serveur qui a ferme ne doit pas tuer le script

"$BIN/awale_serveur" "$PORT" >"$LOG" 2>&1 &
SERVEUR=$!
# attend que le serveur ecoute
for _ in $(seq 50); do
  (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break
  sleep 0.1
done

# ---- petites fonctions utilitaires ----

# ouvre une connexion ; le numero de descripteur est place dans la variable $1
ouvrir() { local fd; exec {fd}<>/dev/tcp/127.0.0.1/$PORT; printf -v "$1" '%s' "$fd"; }
# ferme la connexion dont le descripteur est dans la variable dont le nom est $1
fermer() { eval "exec ${!1}>&-"; }
# envoie le texte tel quel ($2 interprete par printf %b : \n, \r...)
envoyer() { printf '%b' "$2" >&"$1"; }
# lit des lignes jusqu'a en trouver une contenant $2 (3 s max) ; la ligne est dans $LIGNE
attendre() {
  local l
  while read -r -t 3 -u "$1" l 2>/dev/null; do
    l=${l%$'\r'}
    if [[ $l == *"$2"* ]]; then LIGNE=$l; return 0; fi
  done
  return 1
}
# 0 si la connexion est fermee par le serveur dans les 3 secondes (fin de fichier ou reset)
attendre_fermeture() {
  local l st
  while true; do
    read -r -t 3 -u "$1" l 2>/dev/null; st=$?
    [ $st -eq 0 ] && continue
    [ $st -le 128 ]   # 1 = fin de fichier / erreur ; > 128 = delai ecoule
    return
  done
}
serveur_vivant() { kill -0 "$SERVEUR" 2>/dev/null; }

# verifier "description" commande... : compte un succes si la commande renvoie 0
verifier() {
  local desc=$1; shift
  if "$@"; then NB_OK=$((NB_OK + 1)); echo "  ok   $desc"
  else NB_KO=$((NB_KO + 1)); echo "  ECHEC $desc"; fi
}

# arrete le serveur par CTRL-C et verifie le code de sortie et l'absence d'erreur memoire
arreter_serveur() {
  kill -INT "$SERVEUR"; wait "$SERVEUR"; local code=$?
  verifier "serveur arrete par CTRL-C avec le code 0" test "$code" -eq 0
  verifier "aucune erreur memoire (ASan/UBSan) dans les logs" bash -c "! grep -qE 'AddressSanitizer|runtime error' '$LOG'"
}

# affiche le bilan et sort avec un code non nul s'il y a eu un echec
bilan() {
  echo
  echo "$NB_OK succes, $NB_KO echec(s)"
  [ "$NB_KO" -eq 0 ]
}
