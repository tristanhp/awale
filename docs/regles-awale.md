# L'Awalé et la règle retenue

## Le jeu en bref

Jeu de semis et de captures pour deux joueurs. Le plateau a **12 trous** (« cases »), 6 par
joueur, et **48 graines** : 4 par case au départ. Le but est de capturer **plus de 24 graines**.

```
        Nord (joueur 1)      cases 11 10  9  8  7  6    (affichées de gauche à droite)
                              ┌──┬──┬──┬──┬──┬──┐
                              │ 4│ 4│ 4│ 4│ 4│ 4│
                              ├──┼──┼──┼──┼──┼──┤
                              │ 4│ 4│ 4│ 4│ 4│ 4│
                              └──┴──┴──┴──┴──┴──┘
        Sud (joueur 0)       cases  0  1  2  3  4  5
```

Les graines circulent dans le sens des indices croissants : 0 → 1 → … → 5 → 6 → … → 11 → 0
(sens anti-horaire quand on regarde le plateau ci-dessus : Sud va vers la droite, puis
remonte côté Nord et revient vers la gauche).

## Un tour

1. Le joueur choisit **une de ses cases non vides**.
2. Il en retire **toutes les graines** et les **sème une par une** dans les cases suivantes
   (sens ci-dessus), à partir de la case qui suit la sienne.
3. Si la dernière graine tombe dans une case **adverse** qui contient alors **2 ou 3 graines**,
   le joueur **capture** ces graines. Il regarde ensuite la case **précédente** : si elle est
   aussi adverse et contient 2 ou 3 graines, il la capture aussi, et ainsi de suite en
   reculant (c'est la « remontée »). Dès qu'une case ne convient pas (autre nombre ou case
   à soi), la capture s'arrête.
4. C'est à l'adversaire.

### Exemple de capture

Sud joue sa case 5 qui contient 2 graines. Elles tombent en 6 et 7.
Avant le coup, la case 6 avait 1 graine et la case 7 avait 2 graines (elles passent à 2 et 3).
La dernière graine est en 7 (3 graines) : capturée (+3). On recule : la case 6 a 2 graines :
capturée (+2). On recule : la case 5 est à Sud, la remontée s'arrête. Total : 5 graines.

## Règle retenue dans ce projet (règle « officielle »)

| Point | Règle |
|---|---|
| Capture | Dernière graine dans une case adverse qui en contient 2 ou 3 → capture, avec **remontée** tant que les cases précédentes sont adverses avec 2 ou 3 graines. |
| Grosse case | Avec **12 graines ou plus**, on fait un tour complet : la case de départ est **sautée** (elle reste vide pendant le semis). |
| Grand chelem | Si un coup capturerait **toutes** les graines de l'adversaire, le coup est **joué mais ne capture rien**. |
| Famine | Si l'adversaire n'a **plus aucune graine**, le joueur doit **obligatoirement** jouer un coup qui lui en donne. Un coup qui ne l'approvisionne pas est **illégal**. |
| Fin de partie | (a) un joueur a **plus de 24** graines capturées (≥ 25) : il gagne ; (b) le joueur dont c'est le tour **n'a aucun coup légal** (plus de graines, ou impossible de nourrir l'adversaire) : chaque joueur **ajoute à son score les graines restant dans son camp**, puis on compare. 24 contre 24 = **match nul**. |

### Ce qui n'est pas géré (limite connue)

La règle de répétition (positions qui bouclent indéfiniment, où l'on partage les graines) n'est
pas implémentée : très rare, et la partie peut toujours être abandonnée.

## Variantes qui existent (pour la soutenance)

- **Capture** : certaines variantes capturent seulement 2 ou 3 sans remontée ; d'autres
  capturent aussi dans son propre camp.
- **Case d'origine** : certaines variantes ne sautent pas la case de départ à 12 graines.
- **Grand chelem** : autorisé avec capture dans certaines variantes ; interdit complètement
  (coup illégal) dans d'autres.
- **Nourrir** : certaines variantes ne l'obligent pas ; d'autres exigent de nourrir *si
  possible* (c'est le cas ici).
- **Fin** : certaines variantes terminent à 25 graines dès qu'elles sont atteintes, d'autres
  jouent jusqu'au blocage.

## Numérotation des coups dans le jeu

Chaque joueur numérote ses cases de **1 à 6** dans le sens du semis : `MOVE 1` = la première
case de son camp (indice 0 pour Sud, indice 6 pour Nord), `MOVE 6` = la dernière (indice 5 ou
11). À l'écran, Sud lit ses cases de gauche à droite, Nord de droite à gauche (l'affichage
ASCII de `awale_afficher` porte les numéros 1 à 6 au-dessus et au-dessous du plateau).
