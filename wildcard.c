#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include "dosh.h"

static int
matches(char *pat, char *s)
{
    switch (*pat) {
        unsigned n, i;
    case 0:
        return !*s;
    case '*':
        if (!pat[1])
            return 1;
        n = strlen(s);
        for (i=0; i<n; i++)
            if (matches(pat+1, s+i))
                return 1;
        return 0;
    default:
        if (*pat == *s)
    case '?':
            return matches(pat+1, s+1);
        return 0;
    }
}

static void
unit_list_init(struct unit *u)
{
    u->kind = U_LIST;
    u->list = 0;
    u->len = 0;
    u->cap = 0;
}

static void
unit_list_add(struct unit *u, char *s)
{
    if (u->len == u->cap) {
        if (u->cap) {
            u->list = realloc(u->list, (u->cap <<= 1) * sizeof *u->list);
        } else {
            u->list = malloc((u->cap=8) * sizeof *u->list);
        }
    }
    u->list[u->len++] = s;
}

void
try_expand(char *s, struct unit *u)
{
    bool is_wildcard = 0;
    unsigned n = strlen(s);
    unsigned i;
    for (i=0; i<n; i++) {
        switch (s[i]) {
        case '*': case '?':
            is_wildcard = 1;
        }
    }
    if (is_wildcard) {
        unit_list_init(u);
        DIR *d = opendir(".");
        struct dirent *dirent;
        if (d) {
            while (dirent = readdir(d)) {
                if (matches(s, dirent->d_name))
                    unit_list_add(u, strdup(dirent->d_name));
            }
            closedir(d);
        }
        free(s);
    } else {
        u->kind = U_WORD;
        u->word = s;
    }
}
