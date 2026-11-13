#!/bin/bash
source "$(dirname "$0")/lib_proto.sh"

ouvrir A; attendre "$A" 'EVT HELLO'
envoyer "$A" 'CHAT bonjour\n'
verifier 'chat sans pseudo refuse' attendre "$A" 'ERR etat_invalide'
envoyer "$A" 'NICK alice\n'; attendre "$A" 'OK NICK alice'
envoyer "$A" 'CHAT bonjour\n'
verifier 'chat hors partie refuse' attendre "$A" 'ERR etat_invalide'
ouvrir B; attendre "$B" 'EVT HELLO'
envoyer "$B" 'NICK bob\n'; attendre "$B" 'OK NICK bob'
envoyer "$A" 'CHALLENGE bob\n'; attendre "$B" 'EVT CHALLENGE'
envoyer "$B" 'ACCEPT\n'; attendre "$A" 'EVT START'; attendre "$B" 'EVT START'
ouvrir C; attendre "$C" 'EVT HELLO'
envoyer "$C" 'NICK spectateur\n'; attendre "$C" 'OK NICK spectateur'
envoyer "$C" 'WATCH 0\n'; attendre "$C" 'OK WATCH'
envoyer "$A" 'CHAT   \n'
verifier 'message vide refuse' attendre "$A" 'ERR syntaxe'
envoyer "$A" 'CHAT Salut bob, bien joue ! 100%\n'
verifier 'message avec espaces et pseudo' attendre "$B" 'EVT CHAT alice Salut bob, bien joue ! 100%'
verifier 'confirmation expediteur' attendre "$A" 'OK CHAT Salut bob, bien joue ! 100%'
envoyer "$B" 'chat Merci alice !\n'
verifier 'reponse adverse' attendre "$A" 'EVT CHAT bob Merci alice !'
envoyer "$A" 'CHAT \033[2J\n'
verifier 'controles du terminal refuses' attendre "$A" 'ERR syntaxe'
long=$(printf '%0998d' 0)
envoyer "$A" "CHAT $long\n"
verifier 'message trop long refuse' attendre "$A" 'ERR message_trop_long'
sans_chat() {
  local ligne
  envoyer "$1" 'WHO\n'
  while read -r -t 3 -u "$1" ligne; do
    [[ $ligne == 'EVT CHAT '* ]] && return 1
    [[ $ligne == 'OK WHO'* ]] && return 0
  done
  return 1
}
verifier 'chat prive invisible aux observateurs' sans_chat "$C"
envoyer "$C" 'CHAT salut\n'
verifier 'observateur ne peut pas chatter' attendre "$C" 'ERR etat_invalide'
envoyer "$B" 'QUIT\n'; attendre "$A" 'EVT END'
envoyer "$A" 'CHAT encore la ?\n'
verifier 'chat apres fin refuse' attendre "$A" 'ERR etat_invalide'
fermer A; fermer B; fermer C
arreter_serveur
bilan
