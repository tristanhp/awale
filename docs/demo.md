# Scénario de démonstration (16 minutes)

Préparation : 4 terminaux. `make`, puis dans le terminal 1 : `bin/awale_serveur 5000`.
Version finale : comptes, défis, parties, chat, observation et classement Elo. (Sous Windows : `installer-windows.ps1`
puis `serveur.cmd` et `client.cmd`, voir le README.)

## 1. Architecture (1 min, au tableau ou en parlant)

Un seul processus, `select()` sur la socket d'écoute et toutes les sockets clients. Pas de
fork : le serveur doit voir tous les joueurs en mémoire.

## 2. Moteur (1 min)

`make test` : tests unitaires du moteur (captures multiples, grand chelem, famine, fin de
partie, 2000 parties aléatoires qui conservent les 48 graines).

## 3. Connexion et pseudos (2 min)

- Terminal 2 : `bin/awale_client localhost 5000` ; taper `WHO` → refusé (pas de pseudo).
- `REGISTER alice` → le mot de passe est demandé sans s'afficher (et confirmé) ; alice est inscrite.
- Terminal 3 : `NICK alice` → refusé (`pseudo_protege` : ce pseudo a un compte) ; `NICK bob` → OK
  en invité ; **le terminal 2 affiche l'arrivée de bob sans avoir rien tapé** (le client surveille
  clavier et socket).
- `NICK a b` → pseudo invalide ; `DANSER` → commande inconnue.

## 4. Liste des joueurs (1 min)

`WHO` depuis alice et bob. Terminal 4 : `nc localhost 5000` pour montrer que le protocole est
du texte ; taper `NICK carol`, `HELP`.

## 5. Défi et partie (3 min)

- Terminal 2 (alice) : `CHALLENGE bob`. Le terminal 3 (bob) affiche « alice vous défie ! ».
  `WHO` montre `alice:defi_envoye bob:defi_recu`.
- Terminal 4 : `NICK carol`, `CHALLENGE bob` → bob occupé ; `CHALLENGE carol` depuis alice →
  refusé (déjà en défi). Alice tape `CANCEL`, puis redéfie bob.
- Bob : `ACCEPT`. Les deux terminaux affichent le même plateau et qui commence (tirage au sort).
- Jouer quelques coups avec `MOVE n` ; montrer `MOVE` hors tour (refusé), `MOVE 9` (syntaxe),
  une case vide (coup illégal) : c'est le moteur qui juge, le serveur ne triche pas.
- `CHAT bien joué !` : seul l'adversaire le reçoit (les observateurs ne le voient pas).
- `BOARD` réaffiche le plateau. Fermer le terminal d'alice : bob gagne par forfait et
  redevient libre.
- Terminal 4 (carol) : `GAMES`, puis `WATCH 0` : elle voit le plateau et chaque coup en direct.
  `UNWATCH` pour arrêter ; si un joueur part, carol voit la fin de partie et redevient libre.
- `make test` : scénarios de parties, dont une partie aléatoire jouée jusqu'au bout avec
  contrôle des 48 graines, les observateurs et 32 parties simultanées.

## 6. Classement Elo et sauvegarde (3 min)

- `WHO` montre `pseudo (elo) (état)` pour chacun : tout le monde démarre à **600**. Les défis
  affichent aussi l'Elo de l'adversaire : « alice (600) vous défie ».
- `ELO` répond seulement à celui qui le tape ; `ELO 900` fixe son Elo à la main (utile pour
  montrer un écart de niveau). Rien n'est diffusé aux autres.
- Terminer une partie (ou faire abandonner un joueur : le forfait compte) puis `WHO` : à
  600 contre 600, le gagnant passe à 616 et le perdant à 584 ; contre un adversaire plus fort,
  la victoire rapporte davantage (formule d'Elo, coefficient 32).
- **Comptes** : `REGISTER alice` (le mot de passe est demandé sans s'afficher), `ELO 900`, `QUIT`.
  `cat joueurs.csv` : le mot de passe n'apparaît pas, seulement un sel et un hachage.
- **Persistance** : arrêter le serveur (CTRL-C), le relancer (« N compte(s) chargé(s) »), puis
  `LOGIN alice` : `ELO` redonne l'Elo d'avant. Un mauvais mot de passe est refusé ; `NICK alice`
  aussi (`pseudo_protege`). `NICK bob` reste possible en invité (Elo non sauvegardé).
- Limite à dire : pas de chiffrement de la connexion (le mot de passe circule en clair), et pas
  de commande pour changer de mot de passe.

## 7. Robustesse (3 min) — le point fort

- Fermer brutalement le terminal de bob (CTRL-C) : alice voit `bob est parti`, le serveur
  continue (regarder le log du terminal 1).
- Dans le terminal `nc` : taper `NI` puis `CK dave` sur deux envois (CTRL-D n'est pas
  nécessaire ; utiliser `printf 'NI'; sleep 1; printf 'CK dave\n'` | nc ...) : la ligne est
  reconstituée.
- `make test-debug` : scénarios de protocole, de parties et d'Elo, dont ligne de 2000 caractères, client qui ne lit jamais,
  70 connexions simultanées, tout sous ASan/UBSan.

## 8. Extensibilité (1 min)

Montrer `protocole.c` : la table des commandes. « Ajouter une commande = une fonction + une
ligne dans cette table. » `HELP` se met à jour tout seul.

## 9. Questions probables

- Pourquoi `select` et pas `fork` ? → mémoire non partagée après fork.
- Pourquoi un tampon par client ? → TCP est un flux, un `read` ≠ un message.
- Que se passe-t-il si `write` n'écrit qu'une partie ? → file de sortie + `writefds`.
- Et si un client ne lit jamais ? → file bornée pleine → déconnexion, les autres ne ralentissent pas.
- Et SIGPIPE ? → ignoré, `write` renvoie `EPIPE`, on déconnecte ce client.
