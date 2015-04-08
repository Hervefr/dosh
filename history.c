#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HIST_SIZE 16

static char *history[HIST_SIZE];

static int histp;
static int hist_head;

void
history_add(char *s)
{
    if (history[histp]) {
        free(history[histp]);
        hist_head = (histp+1)%HIST_SIZE;
    }
    history[histp] = s;
    histp = (histp+1)%HIST_SIZE;
}

void
show_history(void)
{
    int i;
    for (i=hist_head; i<HIST_SIZE && history[i]; i++)
        printf("%s", history[i]);
    for (i=0; i<hist_head && history[i]; i++)
        printf("%s", history[i]);
}
