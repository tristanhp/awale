/* Mots de passe : jamais stockes en clair, seulement un hachage SHA-256 sale et itere.
 *
 *   hachage = SHA-256 repete MDP_ITERATIONS fois sur (sel + mot de passe)
 *
 * Le sel (16 octets aleatoires, ecrits en hexadecimal) est propre a chaque compte : deux
 * joueurs ayant le meme mot de passe ont des hachages differents, et une table de hachages
 * pre-calculee est inutilisable. Les repetitions ralentissent un attaquant qui essaierait
 * des millions de mots de passe sur un fichier vole.
 * SHA-256 est ecrit ici (pas de bibliotheque a installer, y compris sous Windows/Cygwin).
 * Ce n'est pas un algorithme dedie comme bcrypt ou argon2 : suffisant pour ce projet.
 */

#ifndef MDP_H
#define MDP_H

#define MDP_MIN 6
#define MDP_MAX 64
#define MDP_ITERATIONS 5000
#define SEL_HEX 33  /* 32 caracteres hexadecimaux + '\0' */
#define HASH_HEX 65 /* 64 caracteres hexadecimaux + '\0' */

/* 1 si le mot de passe est acceptable : MDP_MIN a MDP_MAX caracteres ASCII imprimables, sans espace */
int mdp_valide(const char *mdp);

/* SHA-256 brut d'un message (sortie : 32 octets) ; exporte pour les tests */
void mdp_sha256(const unsigned char *msg, unsigned long long len, unsigned char sortie[32]);

/* tire un sel aleatoire (/dev/urandom) et l'ecrit en hexadecimal */
void mdp_nouveau_sel(char sel[SEL_HEX]);

/* calcule le hachage (hexadecimal) du mot de passe avec ce sel */
void mdp_hacher(const char *mdp, const char *sel, char hash[HASH_HEX]);

/* 1 si le mot de passe donne ce hachage avec ce sel (comparaison en temps constant) */
int mdp_verifier(const char *mdp, const char *sel, const char *hash_attendu);

/* 1 si la chaine est exactement n caracteres hexadecimaux */
int mdp_est_hex(const char *s, unsigned n);

#endif
