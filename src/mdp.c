/* Mots de passe : SHA-256 sale et itere (voir mdp.h) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mdp.h"

/* ---- SHA-256 (FIPS 180-4) ---- */

static const unsigned int K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROTD(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void traiter_bloc(unsigned int h[8], const unsigned char bloc[64])
{
  unsigned int w[64];
  for (int t = 0; t < 16; t++)
    w[t] = ((unsigned int)bloc[4 * t] << 24) | ((unsigned int)bloc[4 * t + 1] << 16) |
           ((unsigned int)bloc[4 * t + 2] << 8) | (unsigned int)bloc[4 * t + 3];
  for (int t = 16; t < 64; t++)
  {
    unsigned int s0 = ROTD(w[t - 15], 7) ^ ROTD(w[t - 15], 18) ^ (w[t - 15] >> 3);
    unsigned int s1 = ROTD(w[t - 2], 17) ^ ROTD(w[t - 2], 19) ^ (w[t - 2] >> 10);
    w[t] = w[t - 16] + s0 + w[t - 7] + s1;
  }
  unsigned int a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
  for (int t = 0; t < 64; t++)
  {
    unsigned int S1 = ROTD(e, 6) ^ ROTD(e, 11) ^ ROTD(e, 25);
    unsigned int ch = (e & f) ^ (~e & g);
    unsigned int t1 = hh + S1 + ch + K[t] + w[t];
    unsigned int S0 = ROTD(a, 2) ^ ROTD(a, 13) ^ ROTD(a, 22);
    unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
    unsigned int t2 = S0 + maj;
    hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void mdp_sha256(const unsigned char *msg, unsigned long long len, unsigned char sortie[32])
{
  unsigned int h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  unsigned long long reste = len;
  const unsigned char *p = msg;
  while (reste >= 64)
  {
    traiter_bloc(h, p);
    p += 64;
    reste -= 64;
  }
  /* dernier(s) bloc(s) : reste du message, octet 0x80, zeros, longueur en bits sur 8 octets */
  unsigned char fin[128];
  memset(fin, 0, sizeof(fin));
  memcpy(fin, p, (size_t)reste);
  fin[reste] = 0x80;
  size_t taille = (reste < 56) ? 64 : 128;
  unsigned long long bits = len * 8;
  for (int k = 0; k < 8; k++)
    fin[taille - 1 - k] = (unsigned char)(bits >> (8 * k));
  traiter_bloc(h, fin);
  if (taille == 128)
    traiter_bloc(h, fin + 64);
  for (int k = 0; k < 8; k++)
  {
    sortie[4 * k] = (unsigned char)(h[k] >> 24);
    sortie[4 * k + 1] = (unsigned char)(h[k] >> 16);
    sortie[4 * k + 2] = (unsigned char)(h[k] >> 8);
    sortie[4 * k + 3] = (unsigned char)h[k];
  }
}

/* ---- mots de passe ---- */

int mdp_valide(const char *mdp)
{
  size_t n = strlen(mdp);
  if (n < MDP_MIN || n > MDP_MAX)
    return 0;
  for (size_t k = 0; k < n; k++)
    if ((unsigned char)mdp[k] <= 32 || (unsigned char)mdp[k] >= 127)
      return 0;
  return 1;
}

static void vers_hex(const unsigned char *octets, size_t n, char *hex)
{
  static const char chiffres[] = "0123456789abcdef";
  for (size_t k = 0; k < n; k++)
  {
    hex[2 * k] = chiffres[octets[k] >> 4];
    hex[2 * k + 1] = chiffres[octets[k] & 15];
  }
  hex[2 * n] = '\0';
}

void mdp_nouveau_sel(char sel[SEL_HEX])
{
  unsigned char alea[16];
  size_t lus = 0;
  FILE *f = fopen("/dev/urandom", "r");
  if (f != NULL)
  {
    lus = fread(alea, 1, sizeof(alea), f);
    fclose(f);
  }
  if (lus != sizeof(alea)) /* secours (ne devrait pas arriver) : moins bon, mais le sel n'est pas secret */
  {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    for (size_t k = 0; k < sizeof(alea); k++)
      alea[k] = (unsigned char)(rand() >> 3);
  }
  vers_hex(alea, sizeof(alea), sel);
}

void mdp_hacher(const char *mdp, const char *sel, char hash[HASH_HEX])
{
  unsigned char tampon[32 + SEL_HEX + MDP_MAX + 1];
  unsigned char h[32];
  size_t ls = strlen(sel), lm = strlen(mdp);
  if (ls > SEL_HEX - 1)
    ls = SEL_HEX - 1;
  if (lm > MDP_MAX)
    lm = MDP_MAX;

  /* h0 = SHA256(sel + mdp) ; puis h = SHA256(h + sel + mdp), MDP_ITERATIONS fois */
  memcpy(tampon, sel, ls);
  memcpy(tampon + ls, mdp, lm);
  mdp_sha256(tampon, ls + lm, h);
  for (int k = 0; k < MDP_ITERATIONS; k++)
  {
    memcpy(tampon, h, 32);
    memcpy(tampon + 32, sel, ls);
    memcpy(tampon + 32 + ls, mdp, lm);
    mdp_sha256(tampon, 32 + ls + lm, h);
  }
  vers_hex(h, 32, hash);
}

int mdp_verifier(const char *mdp, const char *sel, const char *hash_attendu)
{
  char hash[HASH_HEX];
  mdp_hacher(mdp, sel, hash);
  /* on parcourt toujours tout : le temps ne depend pas de la premiere difference */
  unsigned char diff = 0;
  for (size_t k = 0; k < HASH_HEX - 1; k++)
    diff |= (unsigned char)(hash[k] ^ hash_attendu[k]);
  return diff == 0;
}

int mdp_est_hex(const char *s, unsigned n)
{
  if (strlen(s) != n)
    return 0;
  for (unsigned k = 0; k < n; k++)
    if (!((s[k] >= '0' && s[k] <= '9') || (s[k] >= 'a' && s[k] <= 'f')))
      return 0;
  return 1;
}
