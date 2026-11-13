# Serveur de jeu Awalé

Projet de programmation répartie (INSA Lyon, 4IF) : un serveur TCP qui fait jouer des clients
à l'Awalé. Un seul processus, `select()`, protocole texte.

**Version finale.** Moteur de jeu testé, serveur et client avec pseudos, liste des joueurs,
défis, parties jouées avec plateau ASCII, observation des parties et classement Elo.
Le serveur tourne sous Linux, et sous Windows sans WSL (voir « Compilation »).

| Étape | Contenu | État |
|---|---|---|
| 0 | Moteur Awalé (plateau, coups, captures, fin, sauvegarde, affichage ASCII) | fait |
| 1 | Client/serveur, enregistrement par pseudo | fait |
| 2 | Liste des joueurs en ligne | fait |
| 3 | Défis (`CHALLENGE`, `ACCEPT`, `DECLINE`, `CANCEL`) | fait |
| 4 | Parties (`MOVE`, `BOARD`, fin de partie, forfait) | fait |
| 5 | Observateurs (`GAMES`, `WATCH`, `UNWATCH`) | fait |
| 6 | Classement Elo (`ELO`, mise à jour en fin de partie) | fait |
| 7 | Chat entre adversaires (`CHAT`) | fait |
| 8 | Comptes (pseudo + mot de passe) et sauvegarde de l'Elo entre les sessions (fichier CSV) | fait |
| 9 à 10 | Bio, mode privé, sauvegarde et reprise des parties | non réalisé |

## Compilation

### Windows sans WSL

Depuis PowerShell, dans le dossier `awale` :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\installer-windows.ps1
```

Le script installe Cygwin, GCC, Make et Bash dans `.outils/`, sans droits
administrateur ni modification permanente du PATH, puis compile et lance les tests.
Une connexion Internet est nécessaire. Cygwin fournit les fonctions POSIX utilisées
par le serveur et le client directement sous Windows ; WSL n'est pas nécessaire.

Ouvrir ensuite trois terminaux dans `awale` :

```powershell
.\serveur.cmd                       # terminal 1 : serveur sur le port 5000
.\client.cmd                        # terminal 2 : premier joueur
.\client.cmd                        # terminal 3 : second joueur
```

Dans le premier client, saisir `NICK alice`, puis `CHALLENGE bob` une fois Bob
connecté. Dans le second, saisir `NICK bob`, puis `ACCEPT`. Jouer avec `MOVE 1`
à `MOVE 6`, quitter avec `QUIT`, arrêter le serveur avec Ctrl-C.

Autre serveur ou autre port : `.\serveur.cmd -Port 6000` et
`.\client.cmd -Hote localhost -Port 6000`.
Pour recompiler et tester :
`powershell -NoProfile -ExecutionPolicy Bypass -File .\windows.ps1 test`.

### Linux

```
make              # bin/awale_serveur et bin/awale_client
make debug        # bin-debug/ : -g, détecteurs d'erreurs mémoire (ASan, UBSan)
make test         # tests du moteur + scénarios du protocole, des parties, de l'Elo, du chat et de la sauvegarde
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
| `REGISTER pseudo mdp` | Créer un compte et s'y connecter. Pseudo de 1 à 16 caractères parmi `A-Z a-z 0-9 _`, unique (la casse est ignorée) ; mot de passe de 6 à 64 caractères visibles, sans espace. |
| `LOGIN pseudo mdp` | Se connecter à son compte : on retrouve son Elo. |
| `NICK pseudo` | Jouer en **invité** (sans compte, Elo non sauvegardé). Refusé si le pseudo a un compte (`pseudo_protege`). |
| `WHO` | Liste des joueurs en ligne et leur état. |
| `ELO` | Afficher son Elo. |
| `ELO nombre` | Fixer librement son Elo à un entier (exemple : `ELO 900`). |
| `HELP` | Commandes utilisables dans la situation courante. |
| `CHALLENGE pseudo` | Défier un joueur libre. |
| `ACCEPT` | Accepter le défi reçu : la partie commence, le tirage au sort désigne qui joue en premier. |
| `DECLINE` | Refuser le défi reçu. |
| `CANCEL` | Retirer le défi qu'on a envoyé. |
| `MOVE n` | Jouer la case `n` (1 à 6, de gauche à droite dans son camp). |
| `CHAT message` | Envoyer un message privé à son adversaire pendant la partie (997 caractères maximum). |
| `BOARD` | Réafficher le plateau de sa partie. |
| `GAMES` | Liste des parties en cours (`numéro : joueurs`). |
| `WATCH n` | Regarder la partie `n` (joueur libre seulement) : plateau courant, puis chaque coup. |
| `UNWATCH` | Arrêter d'observer. |
| `QUIT` | Quitter (CTRL-D fait de même). |

Le défiant joue Sud (en bas), le défié joue Nord (en haut). Le plateau et l'indication « à vous
de jouer » s'affichent après chaque coup. Si un joueur se déconnecte en pleine partie, son
adversaire gagne par forfait. Un joueur a au plus un défi en cours ; il ne peut ni défier ni
être défié tant qu'il n'est pas libre.

Un observateur ne peut ni jouer ni défier ni être défié (jusqu'à `UNWATCH`) ; la fin de la partie
lui est annoncée et il redevient libre. Une partie accepte 8 observateurs au plus.

`CHAT Salut !` affiche chez l'adversaire « Message de alice : Salut ! » et chez l'expéditeur
« Message envoyé : Salut ! ». Les observateurs ne reçoivent pas le chat. Protocole :
`EVT CHAT <pseudo> <message>` pour l'adversaire, `OK CHAT <message>` pour l'expéditeur. Un
message vide ou contenant des caractères de contrôle est refusé (`ERR syntaxe`), un message trop
long renvoie `ERR message_trop_long`.

États affichés par `WHO` : `libre`, `defi_envoye`, `defi_recu`, `en_partie`, `observe`.

Un nouveau joueur (compte ou invité) commence à **600 Elo**. L'Elo des autres joueurs s'affiche
entre parenthèses uniquement dans `WHO` et les messages de défi envoyé ou reçu.
La commande `ELO` répond uniquement au joueur qui l'utilise ; aucune annonce Elo
n'est diffusée lors des connexions, modifications ou fins de partie.
À la fin d'une partie (forfait compris), on calcule
`EA = 1 / (1 + 10^((RB - RA) / 400))`, puis la variation arrondie
`32 * (S - EA)` : `S = 1` pour une victoire, `0.5` pour un nul, `0` pour une défaite.
La variation est ajoutée à A et soustraite à B. Deux joueurs à 600 finissent donc à
616 et 584 après une victoire. Une victoire rapporte au minimum 1 point, retiré
au perdant, même lorsque l'écart Elo ferait arrondir la variation à zéro.
Les nouveaux scores sont visibles dans `WHO`.
Les flèches du plateau, les scores et le tour affichent les pseudos des joueurs.
Le plateau final affiche « Partie terminee » au lieu d'annoncer un nouveau tour.
`ELO` reste utilisable en partie ; le calcul utilise
les valeurs au moment de la fin. Les scores sont sauvegardés (voir « Sauvegarde »).
Les valeurs sont limitées aux entiers signés 32 bits pour éviter les débordements.

Protocole Elo : `OK ELO <pseudo> <elo>` répond à la commande. `WHO` fournit
`pseudo:etat:elo` pour chaque joueur. Les messages `OK CHALLENGE` et
`EVT CHALLENGE` contiennent le pseudo suivi de `(elo)`.

Messages d'erreur : `pseudo_pris`, `pseudo_invalide`, `deja_inscrit`, `syntaxe`,
`commande_inconnue`, `etat_invalide` (par exemple `WHO` avant `NICK`, ou `MOVE` hors partie),
`serveur_plein`, `ligne_trop_longue`, `joueur_inconnu`, `defi_soi`, `joueur_occupe`,
`pas_votre_tour`, `coup_illegal`, `partie_inconnue`, `trop_observateurs`, `pseudo_protege`,
`mdp_invalide`, `identifiants_invalides`, `trop_d_echecs`, `sauvegarde_impossible`,
`message_trop_long`.

## Protocole (pour `nc` et pour le développement)

Un message = une ligne terminée par `\n` (`\r\n` accepté). Le serveur répond par `OK ...` ou
`ERR code`, et envoie des événements `EVT ...` à tout moment.

```
client> NICK alice            serveur> OK NICK alice
client> WHO                   serveur> OK WHO alice:libre:600 bob:libre:600
                              serveur> EVT JOIN bob        (quand bob arrive)
                              serveur> EVT LEAVE bob       (quand bob part)
client> CHALLENGE bob         serveur> OK CHALLENGE bob (600)   (bob reçoit : EVT CHALLENGE alice (600))
client> ELO                   serveur> OK ELO alice 600
client> ACCEPT                serveur> EVT START 0 alice bob alice   (aux deux joueurs)
                              serveur> EVT BOARD 0 4 4 4 4 4 4 4 4 4 4 4 4 0 0 0 0
client> MOVE 3                serveur> EVT MOVE 0 alice 3 0  puis EVT BOARD ... (aux deux)
                              serveur> EVT END 0 alice 26 12 score   (fin de partie)
```

Événements de partie :

| Message | Sens |
|---|---|
| `EVT START <id> <sud> <nord> <premier>` | la partie commence (pseudos) |
| `EVT BOARD <id> <c0> ... <c11> <score_sud> <score_nord> <tour> <fini>` | plateau : 12 cases dans l'ordre du moteur (Sud 0 à 5, Nord 6 à 11), `tour` = 0 (Sud) ou 1 (Nord), `fini` = 1 quand la partie est terminée |
| `EVT MOVE <id> <pseudo> <n> <captures>` | un coup a été joué |
| `EVT END <id> <gagnant ou NUL> <score_sud> <score_nord> <raison>` | `raison` : `score`, `blocage` ou `forfait` |
| `OK GAMES 0:alice-bob 3:carol-dave` | réponse à `GAMES` (`OK GAMES` seul s'il n'y en a pas) |
| `OK WATCH <id> <sud> <nord>` | début d'observation, suivi d'un `EVT BOARD` ; ensuite `EVT MOVE`, `EVT BOARD` et `EVT END` comme pour les joueurs |
| `OK ELO <pseudo> <elo>` | réponse à `ELO` |
| `OK WHO <pseudo>:<état>:<elo> ...` | réponse à `WHO` |
| `EVT DECLINED <pseudo>` / `EVT CANCELLED <pseudo>` | défi refusé, ou retiré (ou joueur parti) |

Limites : lignes de 1023 caractères au maximum (au-delà : `ERR ligne_trop_longue` et
déconnexion), 64 clients simultanés.

## Comptes et sauvegarde (persistance entre les sessions)

Un joueur peut avoir un **compte** : un pseudo protégé par un mot de passe, qui conserve son Elo
d'une session à l'autre.

- `REGISTER alice monmotdepasse` crée le compte (Elo de départ : 600) et connecte.
- `LOGIN alice monmotdepasse` (plus tard, même après un redémarrage du serveur) reconnecte et
  redonne l'Elo d'alice. Le pseudo s'écrit comme à la création (alice et ALICE : même compte).
- `NICK bob` reste possible pour jouer en **invité** : pas de mot de passe, Elo à 600 et
  non sauvegardé. Un pseudo qui a un compte ne peut pas être pris par un invité.
- Dans un vrai terminal, `LOGIN alice` et `REGISTER alice` (sans mot de passe) le demandent
  **sans l'afficher** à l'écran (avec confirmation pour `REGISTER`). Avec `nc` ou un script, le
  mot de passe se met sur la ligne.
- Mauvais pseudo ou mauvais mot de passe : même réponse (`identifiants_invalides`), pour ne pas
  révéler quels pseudos existent. Après 5 échecs sur une connexion, le serveur la ferme.

Tout est dans un fichier texte au format CSV, une ligne par compte :

```
pseudo,elo,sel,hachage
alice,616,3fa9c0...,9c1e72...
```

- **Le mot de passe n'est jamais écrit en clair.** On stocke un hachage SHA-256 *salé* (16
  octets aléatoires propres à chaque compte) et *itéré* 5000 fois : deux joueurs ayant le même
  mot de passe ont des lignes différentes, et un fichier volé ne se déchiffre pas directement.
  SHA-256 est écrit dans `src/mdp.c` (testé sur les vecteurs officiels), sans bibliothèque
  externe, donc ça marche aussi sous Windows. Ce n'est pas bcrypt ou argon2, mais c'est suffisant
  pour ce projet.
- Le fichier est `joueurs.csv` dans le dossier où le serveur est lancé ; une autre
  emplacement se choisit avec `AWALE_DONNEES=/chemin/scores.csv bin/awale_serveur 5000`. Il est
  ignoré par git (il contient les hachages).
- Il est chargé au démarrage (le serveur affiche le nombre de comptes chargés) et réécrit à
  chaque création de compte ou changement d'Elo : commande `ELO nombre` ou fin de partie, forfait
  compris. L'écriture passe par un fichier temporaire puis un renommage : un serveur tué en plein
  milieu ne laisse jamais un fichier à moitié écrit.
- Un fichier absent n'est pas une erreur. Les lignes invalides sont ignorées (l'ancien format
  sans mot de passe aussi). Si le fichier ne peut pas être écrit, `REGISTER` répond
  `sauvegarde_impossible` (pas de compte fantôme) et le serveur continue.
- Les tests utilisent un fichier temporaire : ils ne touchent jamais à ton `joueurs.csv`.
- **Limite importante :** la connexion n'est pas chiffrée (pas de TLS). Le mot de passe circule
  en clair sur le réseau, donc à n'utiliser que sur un réseau de confiance. Seul l'Elo est
  sauvegardé (pas les parties en cours), et il n'y a pas de commande pour changer de mot de passe.

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
src/parties.[ch]     parties en cours (32 au maximum), diffusion aux joueurs et observateurs
src/protocole.[ch]   table des commandes + dispatcher
src/commandes.[ch]   une fonction par commande
src/sauvegarde.[ch]  comptes des joueurs (pseudo, mot de passe haché, Elo) dans un fichier CSV
src/mdp.[ch]         SHA-256, sel, hachage et vérification des mots de passe
src/server.[ch]      portabilité réseau, socket d’écoute + boucle select (structure de la V2)
src/client.c         client : select sur le clavier et la socket, affichage du plateau
tests/               test_awale.c (moteur), test_mdp.c (SHA-256), test_proto.sh, test_parties.sh, test_elo.sh, test_chat.sh, test_sauvegarde.sh
windows.ps1, installer-windows.ps1, serveur.cmd, client.cmd   lancement sous Windows
```

**Ajouter une commande** : écrire `cmd_xxx(int i, char *args)` dans `commandes.c`, la déclarer
dans `commandes.h`, ajouter une ligne `{ "XXX", cmd_xxx, ETAT_... }` dans la table de
`protocole.c`. `HELP` la listera automatiquement.


## Robustesse (ce qui est traité et testé)

Lignes coupées en plusieurs paquets ou plusieurs lignes par paquet, lignes trop longues,
client qui ne lit jamais (file de sortie pleine : il est déconnecté), déconnexion brutale,
pseudo pris ou invalide, commandes inconnues ou mal formées, commande dans le mauvais état,
serveur plein, forfait à la déconnexion, défi interrompu par un départ, `SIGPIPE` ignoré, descripteur ≥ `FD_SETSIZE` refusé, arrêt propre sur CTRL-C.

## Limites connues

- Une coupure silencieuse (câble débranché) n'est pas détectée : le client reste « en ligne ».
- IPv4 seulement, pas de chiffrement de la connexion (le mot de passe circule en clair).
- Règle de répétition d'Awalé non gérée.
- Un défi n'expire pas : on le retire avec `CANCEL`.
- Une partie n'est pas reprise après une déconnexion (forfait immédiat) ; seul l'Elo est sauvegardé.
- Les 32 parties maximales occupent les 64 places : le refus `serveur_plein` d'`ACCEPT` ne
  peut pas se produire avec les tailles actuelles (pas testé).

## Travailler à deux

Une branche par étape (`etape-3-defis`...), merge dans `main` quand `make test-debug` passe.
Les tests doivent rester verts avant chaque merge.
