/* Tampons de communication (voir tampon.h) */

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "tampon.h"

void entree_init(TamponEntree *t)
{
  t->len = 0;
}

int entree_remplir(TamponEntree *t, int fd)
{
  /* tampon plein : read() avec une taille 0 renverrait 0, qu'on prendrait pour une fermeture */
  if (t->len >= ENTREE_MAX)
    return ENTREE_ERREUR;

  ssize_t n = read(fd, t->data + t->len, ENTREE_MAX - t->len);
  if (n > 0)
  {
    t->len += (size_t)n;
    return ENTREE_OK;
  }
  if (n == 0)
    return ENTREE_FERME;
  /* socket non bloquante : rien a lire ou signal recu, ce n'est pas une erreur */
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    return ENTREE_OK;
  return ENTREE_ERREUR;
}

int entree_extraire_ligne(TamponEntree *t, char *ligne, size_t taille)
{
  char *fin = memchr(t->data, '\n', t->len);
  if (fin == NULL)
    return 0;

  size_t long_ligne = (size_t)(fin - t->data);
  size_t reste = t->len - long_ligne - 1;

  /* la ligne est copiee (tronquee si ligne est trop petite), sans '\r' final */
  size_t copie = long_ligne;
  if (copie > 0 && t->data[copie - 1] == '\r')
    copie--;
  if (copie > taille - 1)
    copie = taille - 1;
  memcpy(ligne, t->data, copie);
  ligne[copie] = '\0';

  /* on decale le reste (debut de la ligne suivante) au debut du tampon */
  memmove(t->data, fin + 1, reste);
  t->len = reste;
  return 1;
}

int entree_pleine(const TamponEntree *t)
{
  return t->len >= ENTREE_MAX && memchr(t->data, '\n', t->len) == NULL;
}

void sortie_init(FileSortie *f)
{
  f->debut = 0;
  f->len = 0;
}

int sortie_ajouter(FileSortie *f, const char *texte)
{
  size_t n = strlen(texte);

  /* il faut de la place pour le texte et le '\n' */
  if (n + 1 > SORTIE_MAX - f->len)
    return -1;

  /* si la fin du tableau est atteinte, on ramene les octets en attente au debut */
  if (f->debut + f->len + n + 1 > SORTIE_MAX)
  {
    memmove(f->data, f->data + f->debut, f->len);
    f->debut = 0;
  }
  memcpy(f->data + f->debut + f->len, texte, n);
  f->data[f->debut + f->len + n] = '\n';
  f->len += n + 1;
  return 0;
}

int sortie_vider(FileSortie *f, int fd)
{
  if (f->len == 0)
    return 0;

  ssize_t n = write(fd, f->data + f->debut, f->len);
  if (n < 0)
  {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return 0;
    return -1;
  }

  /* ecriture partielle : on avance simplement le debut, la suite partira au prochain tour */
  f->debut += (size_t)n;
  f->len -= (size_t)n;
  if (f->len == 0)
    f->debut = 0;
  return 0;
}

int sortie_vide(const FileSortie *f)
{
  return f->len == 0;
}
