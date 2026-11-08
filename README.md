# Serveur de jeu Awalé

Projet de programmation répartie (INSA Lyon, 4IF) : un serveur TCP qui fait jouer des clients
à l'Awalé. Un seul processus, `select()`, protocole texte.

**Avancement : séance 2 (étapes 0 à 4).** Moteur de jeu testé, serveur et client avec pseudos,
liste des joueurs, défis et parties jouées avec plateau ASCII. L'observation des parties
(étape 5) vient ensuite (voir `docs/superpowers/plans/`).

| Étape | Contenu | État |
|---|---|---|
| 0 | Moteur Awalé (plateau, coups, captures, fin, sauvegarde, affichage ASCII) | fait |
| 1 | Client/serveur, enregistrement par pseudo | fait |
| 2 | Liste des joueurs en ligne | fait |
| 3 | Défis (`CHALLENGE`, `ACCEPT`, `DECLINE`, `CANCEL`) | fait |
| 4 | Parties (`MOVE`, `BOARD`, fin de partie, forfait) | fait |
| 5 | Observateurs (`GAMES`, `WATCH`) | à faire |
| 6 à 10 | Chat, bio, mode privé, sauvegarde des parties, extras | à faire |

## Compilation

```
make              # bin/awale_serveur et bin/awale_client
make debug        # bin-debug/ : -g, détecteurs d'erreurs mémoire (ASan, UBSan)
make test         # tests du moteur + scénarios du protocole + défis et parties
make test-debug   # idem avec les binaires de debug
make clean
```

Compilation avec `-Wall -Wextra` : aucun warning. Nécessite `gcc` et `make` ; les tests du
protocole utilisent `bash` (pas besoin de `nc`).

## Lancement

Serveur (n'importe quel port, > 1024 si l'on n'est pas root) :

```
bin/awale_serveur 5000
```

Il affiche les connexions et déconnexions sur sa sortie. `CTRL-C` l'arrête proprement. Il n'a
aucune commande interactive : il se pilote uniquement par les clients.

Client (une fenêtre par joueur) :

```
bin/awale_client localhost 5000
```

On peut aussi se connecter sans le client, par exemple avec `nc localhost 5000` : le protocole
est du texte, une commande par ligne.

## Commandes (côté client)

On tape les commandes du protocole, en majuscules ou minuscules. Le client affiche les
réponses du serveur et les événements (arrivée d'un joueur...) **même pendant la saisie**.

| Commande | Effet |
|---|---|
| `NICK pseudo` | S'enregistrer. Pseudo de 1 à 16 caractères parmi `A-Z a-z 0-9 _`, unique (la casse est ignorée). |
| `WHO` | Liste des joueurs en ligne et leur état. |
| `HELP` | Commandes utilisables dans la situation courante. |
| `CHALLENGE pseudo` | Défier un joueur libre. |
| `ACCEPT` | Accepter le défi reçu : la partie commence, le tirage au sort désigne qui joue en premier. |
| `DECLINE` | Refuser le défi reçu. |
| `CANCEL` | Retirer le défi qu'on a envoyé. |
| `MOVE n` | Jouer la case `n` (1 à 6, de gauche à droite dans son camp). |
| `BOARD` | Réafficher le plateau de sa partie. |
| `QUIT` | Quitter (CTRL-D fait de même). |

Le défiant joue Sud (en bas), le défié joue Nord (en haut). Le plateau et l'indication « à vous
de jouer » s'affichent après chaque coup. Si un joueur se déconnecte en pleine partie, son
adversaire gagne par forfait. Un joueur a au plus un défi en cours ; il ne peut ni défier ni
être défié tant qu'il n'est pas libre.

États affichés par `WHO` : `libre`, `defi_envoye`, `defi_recu`, `en_partie`.

Messages d'erreur : `pseudo_pris`, `pseudo_invalide`, `deja_inscrit`, `syntaxe`,
`commande_inconnue`, `etat_invalide` (par exemple `WHO` avant `NICK`, ou `MOVE` hors partie),
`serveur_plein`, `ligne_trop_longue`, `joueur_inconnu`, `defi_soi`, `joueur_occupe`,
`pas_votre_tour`, `coup_illegal`.

## Protocole (pour `nc` et pour le développement)

Un message = une ligne terminée par `\n` (`\r\n` accepté). Le serveur répond par `OK ...` ou
`ERR code`, et envoie des événements `EVT ...` à tout moment.

```
client> NICK alice            serveur> OK NICK alice
client> WHO                   serveur> OK WHO alice:libre bob:libre
                              serveur> EVT JOIN bob        (quand bob arrive)
                              serveur> EVT LEAVE bob       (quand bob part)
client> CHALLENGE bob         serveur> OK CHALLENGE bob     (bob reçoit : EVT CHALLENGE alice)
client> ACCEPT                serveur> EVT START 0 alice bob alice   (aux deux joueurs)
                              serveur> EVT BOARD 0 4 4 4 4 4 4 4 4 4 4 4 4 0 0 0
client> MOVE 3                serveur> EVT MOVE 0 alice 3 0  puis EVT BOARD ... (aux deux)
                              serveur> EVT END 0 alice 26 12 score   (fin de partie)
```

Événements de partie :

| Message | Sens |
|---|---|
| `EVT START <id> <sud> <nord> <premier>` | la partie commence (pseudos) |
| `EVT BOARD <id> <c0> ... <c11> <score_sud> <score_nord> <tour>` | plateau : 12 cases dans l'ordre du moteur (Sud 0 à 5, Nord 6 à 11), `tour` = 0 (Sud) ou 1 (Nord) |
| `EVT MOVE <id> <pseudo> <n> <captures>` | un coup a été joué |
| `EVT END <id> <gagnant ou NUL> <score_sud> <score_nord> <raison>` | `raison` : `score`, `blocage` ou `forfait` |
| `EVT DECLINED <pseudo>` / `EVT CANCELLED <pseudo>` | défi refusé, ou retiré (ou joueur parti) |

Limites : lignes de 1023 caractères au maximum (au-delà : `ERR ligne_trop_longue` et
déconnexion), 64 clients simultanés.

## Règle du jeu retenue

Règle « officielle » : capture de 2 ou 3 graines dans le camp adverse avec remontée, case
d'origine sautée à partir de 12 graines, grand chelem sans capture, obligation de nourrir,
fin à plus de 24 graines ou quand le joueur au trait ne peut plus jouer. Détail, exemples et
variantes écartées : [`docs/regles-awale.md`](docs/regles-awale.md).

## Architecture (résumé)

```
src/awale.[ch]       moteur pur (aucune E/S)
src/tampon.[ch]      tampon de lignes en entrée, file d'envoi en sortie (tailles fixes)
src/clients.[ch]     table des clients, états, pseudos, envoi de messages
src/parties.[ch]     parties en cours (32 au maximum), diffusion aux deux joueurs
src/protocole.[ch]   table des commandes + dispatcher
src/commandes.[ch]   une fonction par commande
src/server.[ch]      portabilité réseau, socket d’écoute + boucle select (structure de la V2)
src/client.c         client : select sur le clavier et la socket, affichage du plateau
```

**Ajouter une commande** : écrire `cmd_xxx(int i, char *args)` dans `commandes.c`, la déclarer
dans `commandes.h`, ajouter une ligne `{ "XXX", cmd_xxx, ETAT_... }` dans la table de
`protocole.c`. `HELP` la listera automatiquement.

Design complet : `docs/superpowers/specs/2026-11-01-awale-design.md`.

## Robustesse (ce qui est traité et testé)

Lignes coupées en plusieurs paquets ou plusieurs lignes par paquet, lignes trop longues,
client qui ne lit jamais (file de sortie pleine : il est déconnecté), déconnexion brutale,
pseudo pris ou invalide, commandes inconnues ou mal formées, commande dans le mauvais état,
serveur plein, forfait à la déconnexion, défi interrompu par un départ, `SIGPIPE` ignoré, descripteur ≥ `FD_SETSIZE` refusé, arrêt propre sur CTRL-C.

## Limites connues

- Une coupure silencieuse (câble débranché) n'est pas détectée : le client reste « en ligne ».
- IPv4 seulement, pas de chiffrement, le pseudo n'est pas un mot de passe.
- Règle de répétition d'Awalé non gérée.
- Un défi n'expire pas : on le retire avec `CANCEL`.
- Une partie n'est pas reprise après une déconnexion (forfait immédiat).
- Les 32 parties maximales occupent les 64 places : le refus `serveur_plein` d'`ACCEPT` ne
  peut pas se produire avec les tailles actuelles (pas testé).

## Travailler à deux

Une branche par étape (`etape-3-defis`...), merge dans `main` quand `make test-debug` passe.
Les tests doivent rester verts avant chaque merge.
