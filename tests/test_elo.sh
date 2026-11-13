#!/bin/bash
source "$(dirname "$0")/lib_proto.sh"

# WHO sert de repere : aucune annonce Elo ne doit preceder sa reponse.
sans_annonce_elo() {
  local ligne annonce=0
  envoyer "$1" 'WHO\n'
  while read -r -t 3 -u "$1" ligne; do
    [[ $ligne == 'EVT ELO '* ]] && annonce=1
    if [[ $ligne == 'OK WHO'* ]]; then return "$annonce"; fi
  done
  return 1
}

ouvrir D; attendre "$D" 'EVT HELLO'
envoyer "$D" 'REGISTER observateur motdepasse1\n'; attendre "$D" 'OK NICK observateur'

ouvrir A; attendre "$A" 'EVT HELLO'
envoyer "$A" 'ELO 900\n'
verifier 'ELO avant inscription refuse' attendre "$A" 'ERR etat_invalide'
envoyer "$A" 'REGISTER alice motdepasse1\n'; attendre "$A" 'OK NICK alice'
envoyer "$A" 'ELO\n'
verifier 'Elo initial 600' attendre "$A" 'OK ELO alice 600'
envoyer "$A" 'ELO -200\n'
verifier 'Elo negatif autorise' attendre "$A" 'OK ELO alice -200'
verifier 'connexion et modification sans annonce Elo aux autres' sans_annonce_elo "$D"
for valeur in abc '900 extra' 999999999999999999999999; do
  envoyer "$A" "ELO $valeur\n"
  verifier "Elo invalide refuse : $valeur" attendre "$A" 'ERR syntaxe'
done
envoyer "$A" 'ELO 600\n'; attendre "$A" 'OK ELO alice 600'
ouvrir B; attendre "$B" 'EVT HELLO'
envoyer "$B" 'REGISTER bob motdepasse1\n'; attendre "$B" 'OK NICK bob'
envoyer "$A" 'CHALLENGE bob\n'
verifier 'defi recu avec Elo du defiant' attendre "$B" 'EVT CHALLENGE alice (600)'
verifier 'defi envoye avec Elo de la cible' attendre "$A" 'OK CHALLENGE bob (600)'
envoyer "$B" 'ACCEPT\n'; attendre "$A" 'EVT START'
envoyer "$B" 'QUIT\n'
verifier 'forfait : victoire alice' attendre "$A" 'EVT END 0 alice'
envoyer "$A" 'ELO\n'
verifier 'victoire contre Elo egal : +16' attendre "$A" 'OK ELO alice 616'
verifier 'fin de partie sans annonce Elo aux autres' sans_annonce_elo "$D"

ouvrir C; attendre "$C" 'EVT HELLO'
envoyer "$C" 'REGISTER carol motdepasse1\n'; attendre "$C" 'OK NICK carol'
envoyer "$C" 'ELO 1000\n'; attendre "$C" 'OK ELO carol 1000'
envoyer "$A" 'ELO 600\n'; attendre "$A" 'OK ELO alice 600'
envoyer "$A" 'CHALLENGE carol\n'; attendre "$C" 'EVT CHALLENGE alice'
envoyer "$C" 'ACCEPT\n'; attendre "$A" 'EVT START'
envoyer "$C" 'QUIT\n'; attendre "$A" 'EVT END 0 alice'
envoyer "$A" 'ELO\n'
verifier 'victoire contre 400 points de plus : +29' attendre "$A" 'OK ELO alice 629'
envoyer "$A" 'WHO\n'
verifier 'WHO contient Elo' attendre "$A" 'alice:libre:629'

ouvrir F; attendre "$F" 'EVT HELLO'
envoyer "$F" 'REGISTER favori motdepasse1\n'; attendre "$F" 'OK NICK favori'
envoyer "$F" 'ELO 3000\n'; attendre "$F" 'OK ELO favori 3000'
envoyer "$A" 'ELO 600\n'; attendre "$A" 'OK ELO alice 600'
envoyer "$A" 'CHALLENGE favori\n'; attendre "$F" 'EVT CHALLENGE alice'
envoyer "$F" 'ACCEPT\n'; attendre "$F" 'EVT START'
envoyer "$A" 'QUIT\n'; attendre "$F" 'EVT END 0 favori'
envoyer "$F" 'ELO\n'
verifier '3000 contre 600 : victoire au minimum +1' attendre "$F" 'OK ELO favori 3001'
envoyer "$D" 'WHO\n'
verifier 'Elo mis a jour conserve pour les autres joueurs' attendre "$D" 'favori:libre:3001'
fermer F
fermer A; fermer B; fermer C; fermer D
arreter_serveur
bilan
