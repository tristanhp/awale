# Serveur de jeu Awalé

Projet de programmation répartie (INSA Lyon, 4IF) : un serveur TCP qui fait jouer des clients
à l'Awalé. Un seul processus, `select()`, protocole texte.

**Avancement : séance 1 (étapes 0 à 2).** Moteur de jeu testé, serveur et client avec pseudos,
liste des joueurs. Les défis, les parties et l'observation (étapes 3 à 5) viennent ensuite
(voir `docs/superpowers/plans/`).

| Étape | Contenu | État |
|---|---|---|
| 0 | Moteur Awalé (plateau, coups, captures, fin, sauvegarde, affichage ASCII) | fait |
| 1 | Client/serveur, enregistrement par pseudo | fait |
| 2 | Liste des joueurs en ligne | fait |
| 3 à 5 | Défis, parties, observateurs | à faire |
| 6 à 10 | Chat, bio, mode privé, sauvegarde des parties, extras | à faire |

## Compilation

```
make              # bin/awale_serveur et bin/awale_client
make debug        # bin-debug/ : -g, détecteurs d'erreurs mémoire (ASan, UBSan)
make test         # tests du moteur + scénarios du protocole
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
| `QUIT` | Quitter (CTRL-D fait de même). |

Messages d'erreur : `pseudo_pris`, `pseudo_invalide`, `deja_inscrit`, `syntaxe`,
`commande_inconnue`, `etat_invalide` (par exemple `WHO` avant `NICK`), `serveur_plein`,
`ligne_trop_longue`.

## Protocole (pour `nc` et pour le développement)

Un message = une ligne terminée par `\n` (`\r\n` accepté). Le serveur répond par `OK ...` ou
`ERR code`, et envoie des événements `EVT ...` à tout moment.

```
client> NICK alice            serveur> OK NICK alice
client> WHO                   serveur> OK WHO alice:libre bob:libre
                              serveur> EVT JOIN bob        (quand bob arrive)
                              serveur> EVT LEAVE bob       (quand bob part)
```

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
src/clients.[ch]     table des clients, pseudos, envoi de messages
src/protocole.[ch]   table des commandes + dispatcher
src/commandes.[ch]   une fonction par commande
src/server.[ch]       portabilité réseau, socket d’écoute + boucle select (structure de la V2)
src/client.c         client : select sur le clavier et la socket
```

**Ajouter une commande** : écrire `cmd_xxx(int i, char *args)` dans `commandes.c`, la déclarer
dans `commandes.h`, ajouter une ligne `{ "XXX", cmd_xxx, ETAT_... }` dans la table de
`protocole.c`. `HELP` la listera automatiquement.

Design complet : `docs/superpowers/specs/2026-11-01-awale-design.md`.

## Robustesse (ce qui est traité et testé)

Lignes coupées en plusieurs paquets ou plusieurs lignes par paquet, lignes trop longues,
client qui ne lit jamais (file de sortie pleine : il est déconnecté), déconnexion brutale,
pseudo pris ou invalide, commandes inconnues ou mal formées, commande dans le mauvais état,
serveur plein, `SIGPIPE` ignoré, descripteur ≥ `FD_SETSIZE` refusé, arrêt propre sur CTRL-C.

## Limites connues

- Une coupure silencieuse (câble débranché) n'est pas détectée : le client reste « en ligne ».
- IPv4 seulement, pas de chiffrement, le pseudo n'est pas un mot de passe.
- Règle de répétition d'Awalé non gérée.

## Travailler à deux

Une branche par étape (`etape-3-defis`...), merge dans `main` quand `make test-debug` passe.
Les tests doivent rester verts avant chaque merge.
