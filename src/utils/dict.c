#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/dict.h>
#include <malloc.h>

#define HASHSIZE 101

static struct nlist *hashtab[HASHSIZE];

unsigned hash(char *s)
{
    ///
    unsigned hashval;

    for (hashval = 0; *s != '\0'; s++)
        hashval = *s + 31 * hashval;
    return hashval % HASHSIZE;
}

struct nlist *lookup(char *s)
{
    struct nlist *np;
    for (np = hashtab[hash(s)]; np != NULL; np = np->next)
        if (strcmp(np->name, s) == 0)
            return np;
    return NULL;
}

struct nlist *lookup(char *s);

char *mystrdup(char *s)
{
    char *p;
    p = (char *)malloc(strlen(s) + 1);
    if (p != NULL)
        strcpy(p, s);
    return p;
}

struct nlist *install(char *name, char *defn)
{
    struct nlist *np;
    unsigned hashval;

    if ((np = lookup(name)) == NULL)
    {
        np = (struct nlist *)malloc(sizeof(struct nlist));
        if (np == NULL || (np->name = mystrdup(name)) == NULL)
            return NULL;
        hashval = hash(name);
        np->next = hashtab[hashval]; ///
        hashtab[hashval] = np;
    }
    else
    {
        free((void *)np->defn);
    }

    ///
    if ((np->defn = mystrdup(defn)) == NULL)
        return NULL;
    return np;
}

void undef(char *name)
{
    struct nlist *np;
    unsigned hashval;
    hashval = hash(name);

    np = lookup(name);

    if (np == NULL)
        return;

    if (np == hashtab[hashval])
    {
        hashtab[hashval] = np->next;
        free(np);
    }

    else
    {
        struct nlist *p;
        for (p = hashtab[hashval]; p->next != np; p++)
            ;
        p->next = p->next->next;
        free(p->next);
    }
}

void replace(char *name)
{
    struct nlist *np;
    unsigned hashval;
    hashval = hash(name);

    np = lookup(name);

    if (np == NULL)
    {
        printf("%s", name);
        return;
    }
    else
    {
        printf("%s", np->defn);
    }
}
