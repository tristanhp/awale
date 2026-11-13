/* Tests du module mdp : SHA-256 (vecteurs officiels), sel, hachage, verification */
#include <stdio.h>
#include <string.h>
#include "../src/mdp.h"

static int nb_ok = 0, nb_ko = 0;
#define CHECK(c) do { if (c) nb_ok++; else { nb_ko++; printf("ECHEC %s:%d : %s\n", __FILE__, __LINE__, #c); } } while (0)

static void sha_hex(const char *msg, char *hex)
{
  unsigned char h[32];
  mdp_sha256((const unsigned char *)msg, strlen(msg), h);
  for (int k = 0; k < 32; k++)
    sprintf(hex + 2 * k, "%02x", h[k]);
}

int main(void)
{
  char hex[65];

  /* vecteurs de test du standard (FIPS 180-4) */
  sha_hex("", hex);
  CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
  sha_hex("abc", hex);
  CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
  /* 56 octets : le remplissage deborde sur un deuxieme bloc */
  sha_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", hex);
  CHECK(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);

  /* sel : 32 caracteres hexadecimaux, different a chaque tirage */
  char sel1[SEL_HEX], sel2[SEL_HEX];
  mdp_nouveau_sel(sel1);
  mdp_nouveau_sel(sel2);
  CHECK(mdp_est_hex(sel1, 32) && mdp_est_hex(sel2, 32));
  CHECK(strcmp(sel1, sel2) != 0);

  /* hachage : deterministe, depend du mot de passe ET du sel */
  char h1[HASH_HEX], h2[HASH_HEX], h3[HASH_HEX], h4[HASH_HEX];
  mdp_hacher("secret1", sel1, h1);
  mdp_hacher("secret1", sel1, h2);
  mdp_hacher("secret2", sel1, h3);
  mdp_hacher("secret1", sel2, h4);
  CHECK(mdp_est_hex(h1, 64));
  CHECK(strcmp(h1, h2) == 0);
  CHECK(strcmp(h1, h3) != 0);
  CHECK(strcmp(h1, h4) != 0);
  CHECK(strstr(h1, "secret1") == NULL);

  CHECK(mdp_verifier("secret1", sel1, h1));
  CHECK(!mdp_verifier("secret2", sel1, h1));
  CHECK(!mdp_verifier("secret1", sel2, h1));
  CHECK(!mdp_verifier("", sel1, h1));

  /* format accepte */
  CHECK(mdp_valide("secret"));
  CHECK(mdp_valide("Tr0ub4dor&3"));
  CHECK(!mdp_valide("court"));                 /* 5 caracteres */
  CHECK(!mdp_valide("avec espace"));
  CHECK(!mdp_valide("tab\tulation"));
  CHECK(!mdp_valide("accentu\xc3\xa9s"));
  CHECK(mdp_valide("0123456789012345678901234567890123456789012345678901234567890123"));    /* 64 */
  CHECK(!mdp_valide("01234567890123456789012345678901234567890123456789012345678901234")); /* 65 */

  CHECK(!mdp_est_hex("zz", 2) && !mdp_est_hex("ab", 3) && !mdp_est_hex("AB", 2));

  printf("%d verifications, %d echec(s)\n", nb_ok + nb_ko, nb_ko);
  return nb_ko != 0;
}
