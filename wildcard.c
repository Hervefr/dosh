#define _GNU_SOURCE /* for strchrnul() */
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "dosh.h"

static int
matches(char *pat, char *s, int whole)
{
    switch (*pat) {
        unsigned n, i;
    case 0:
        return !*s;
    case '*':
        if (whole && *s == '.')
            return 0;
        if (!pat[1])
            return 1;
        n = strlen(s);
        for (i=0; i<n; i++)
            if (matches(pat+1, s+i, 0))
                return 1;
        return 0;
    default:
        if (*pat == *s)
    case '?':
            return matches(pat+1, s+1, 0);
        return 0;
    }
}

struct strlist {
    char **data;
    unsigned len, cap;
};

static void
strlist_add(struct strlist *l, char *s)
{
    if (l->len == l->cap) {
        if (l->cap) {
            l->data = realloc(l->data, (l->cap <<= 1) * sizeof *l->data);
        } else {
            l->data = malloc((l->cap=8) * sizeof *l->data);
        }
    }
    l->data[l->len++] = s;
}

static char *
wildcard_ptr(char *s)
{
    for (;;) {
        switch (*s) {
        case 0:
            return 0;
        case '*': case '?':
            return s;
        }
        s++;
    }
}

static char *
substr(char *from, char *to)
{
    unsigned n = to-from;
    char *s = malloc(n+1);
    memcpy(s, from, n);
    s[n] = 0;
    return s;
}

static char *
join(char *s1, char *s2)
{
    unsigned n1 = strlen(s1), n2 = strlen(s2);
    char *ret = malloc(n1+n2+1);
    memcpy(ret, s1, n1);
    memcpy(ret+n1, s2, n2);
    ret[n1+n2] = 0;
    return ret;
}

static char **
expand_atdir(char *pat, unsigned *plen, char *path)
{
    struct strlist list = {};
    DIR *d = opendir(*path ? path : ".");
    struct dirent *dirent;
    if (d) {
        while (dirent = readdir(d)) {
            if (matches(pat, dirent->d_name, 1))
                strlist_add(&list, join(path, dirent->d_name));
        }
        closedir(d);
    }
    *plen = list.len;
    return list.data;
}

static void
try_expand(char *s, char *prepend, struct strlist *strlist)
{
    char *pwc;
    if (pwc = wildcard_ptr(s)) {
        char *p, *q;
        char **list;
        unsigned len, i;
        for (p=pwc; !(p == s || p[-1] == '/'); p--);
        q = strchrnul(pwc, '/');
        char *dir;
        if (prepend) {
            unsigned n = strlen(prepend);
            dir = malloc(n+(p-s)+1);
            memcpy(dir, prepend, n);
            memcpy(dir+n, s, p-s);
            dir[n+(p-s)] = 0;
        } else {
            dir = substr(s, p);
        }
        char *pat = substr(p, q);
        list = expand_atdir(pat, &len, dir);
        free(pat);
        free(dir);
        if (*q) {
            for (i=0; i<len; i++) {
                try_expand(q, list[i], strlist);
                free(list[i]);
            }
        } else {
            for (i=0; i<len; i++)
                strlist_add(strlist, list[i]);
        }
        free(list);
    } else {
        strlist_add(strlist, prepend ? join(prepend, s) : s);
    }
}

void
try_expand_user(char *s, struct unit *u)
{
    if (wildcard_ptr(s)) {
        struct strlist list = {};
        try_expand(s, 0, &list);
        free(s);
        u->kind = U_LIST;
        u->list = list.data;
        u->len = list.len;
    } else {
        u->kind = U_WORD;
        u->word = s;
    }
}
