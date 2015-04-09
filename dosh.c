#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include "dosh.h"
#include "builtins.h"

static pid_t shell_pgid;
/* reading input from tty? */
static int interact;
bool from_file;
//struct termios tmodes;

/* list of stopped & background jobs */
static struct job {
    struct job *next;
    pid_t pgid;
    bool stopped;
} *jobs;

static void
job_add(pid_t pgid, bool stopped)
{
    struct job *new = malloc(sizeof *new);
    new->next = jobs;
    new->pgid = pgid;
    new->stopped = stopped;
    jobs = new;
}

static void
list_jobs(void)
{
    struct job *j;
    for (j=jobs; j; j=j->next) {
        printf("%d", j->pgid);
        if (j->stopped)
            puts("\tStopped");
        else
            putchar('\n');
    }
}

static int
run_fg(pid_t pgid, bool cont)
{
    int status = 0;
    tcsetpgrp(0, pgid);
    if (cont)
        kill(-pgid, SIGCONT);
    waitpid(pgid, &status, WUNTRACED);
    tcsetpgrp(0, shell_pgid);
    return status;
}

static void
check_status(int status, pid_t pgid)
{
    if (WIFSTOPPED(status)) {
        fprintf(stderr, "%d\tStopped\n", pgid);
        job_add(pgid, 1);
    } else {
        if (WIFSIGNALED(status))
            psignal(WTERMSIG(status), 0);
    }
}

static void
do_builtin(int kind, unsigned argc, char **argv)
{
    switch (kind) {
        struct job *next;
        char *path;
        int status;
        pid_t pgid;
    case BUILTIN_EXIT:
        /* TODO arg */
        exit(0);
        break;
    case BUILTIN_FG:
        if (jobs) {
            status = run_fg(pgid = jobs->pgid, jobs->stopped);
            next = jobs->next;
            free(jobs);
            jobs = next;
            check_status(status, pgid);
        }
        break;
    case BUILTIN_BG:
        if (jobs) {
            fprintf(stderr, "%d\n", jobs->pgid);
            if (jobs->stopped) {
                kill(-jobs->pgid, SIGCONT);
                jobs->stopped = 0;
            }
        }
        break;
    case BUILTIN_JOBS:
        list_jobs();
        break;
    case BUILTIN_HISTORY:
        show_history();
        break;
    case BUILTIN_CD:
        if (argc<2)
            path = getenv("HOME");
        else
            path = argv[1];
        if (chdir(path))
            perror(path);
    }
}

static void
add_arg(struct simple_cmd *cmd, char *arg)
{
    if (cmd->len == cmd->cap)
        cmd->argv = realloc(cmd->argv, (cmd->cap <<= 1) * sizeof(char *));
    cmd->argv[cmd->len++] = arg;
}

void
add_unit(struct simple_cmd *cmd, struct unit *u)
{
    switch (u->kind) {
    case U_WORD:
        add_arg(cmd, u->word);
        break;
    case U_FROM:
        free(cmd->from);
        cmd->from = u->word;
        break;
    case U_APPEND:
        cmd->append = 1;
        /* fallthrough */
    case U_INTO:
        free(cmd->into);
        cmd->into = u->word;
    }
}

void
yyerror(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void
free_simple_cmd(struct simple_cmd *cmd)
{
    unsigned i;
    for (i=0; i<cmd->len; i++)
        free(cmd->argv[i]);
    free(cmd->argv);
    free(cmd->from);
    free(cmd->into);
    free(cmd);
}

static void
free_pipe_cmd(struct pipe_cmd *cmd)
{
    do {
        struct pipe_cmd *cmdsv = cmd;
        free_simple_cmd(cmd->leader);
        cmd = cmd->rest;
        free(cmdsv);
    } while (cmd);
}

void
free_list_cmd(struct list_cmd *cmd)
{
    do {
        struct list_cmd *cmdsv = cmd;
        free_pipe_cmd(cmd->tail);
        cmd = cmd->body;
        free(cmdsv);
    } while (cmd);
}

static int
open_dup2(char *file, int mode, int newfd)
{
    int fd = open(file, mode);
    if (fd<0) {
        perror("open");
        return fd;
    }
    dup2(fd, newfd);
    close(fd);
    return 0;
}

/* 0 indicates failure */
static pid_t
do_simple_cmd(struct simple_cmd *cmd, int pgid, int in, int out, bool fg)
{
    pid_t pid;

    //fprintf(stderr, "%s(%d,%d)\n", cmd->argv[0], in,out);

    /* ensure argv is NULL-terminated */
    add_arg(cmd, 0);

    struct builtin *bi = get_builtin(cmd->argv[0]);
    if (bi) {
        do_builtin(bi->kind, cmd->len-1, cmd->argv);
        return 0;
    }

    pid = fork();
    if (!pid) {
        /* child process */
        if (interact) {
            pid = getpid();
            if (!pgid)
                pgid = pid;
            setpgid(pid, pgid);
            //printf("%s:%d(%d) in=%d out=%d\n",
                   //cmd->argv[0], getpid(), getpgid(), in, out);
            if (fg)
                tcsetpgrp(0, pgid);
            /* set the handling for job control signals back to the default */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
        }
        /* redirect standard I/O */
        if (in != STDIN_FILENO) {
            dup2(in, STDIN_FILENO);
            //close(in);
        } else if (cmd->from) {
            if (open_dup2(cmd->from, O_RDONLY, 0))
                return 0;
        }
        if (out != STDOUT_FILENO) {
            dup2(out, STDOUT_FILENO);
            //close(out);
        } else if (cmd->into) {
            int mode = cmd->append ?
                O_WRONLY | O_CREAT | O_APPEND:
                O_WRONLY | O_CREAT | O_TRUNC;
            if (open_dup2(cmd->into, mode, 1))
                return 0;
        }
        execvp(cmd->argv[0], cmd->argv);
        err(1, "%s", cmd->argv[0]);
    } else {
        if (pid<0) {
            perror("fork");
        } else {
            if (interact) {
                if (!pgid)
                    pgid = pid;
                setpgid(pid, pgid);
            }
        }
    }

    return pid;
}

static pid_t
do_pipe_cmd(struct pipe_cmd *cmd, int pgid, int in, int out, int fg)
{
    if (!cmd->rest)
        return do_simple_cmd(cmd->leader, pgid, in, out, fg);

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) < 0) {
        perror("pipe");
        return 0;
    }

    pid_t pid = do_simple_cmd(cmd->leader, pgid, pipefd[0], out, fg);
    if (pid) {
        if (!pgid)
            pgid = pid;
        do_pipe_cmd(cmd->rest, pgid, in, pipefd[1], fg);
    } else {
        fputs("pipe command failed\n", stderr);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    return pgid;
}

static int
do_list_cmd(struct list_cmd *cmd, bool fg)
{
    switch (cmd->kind) {
        int status;
    case LIST_AND:
        if (status = do_list_cmd(cmd->body, 1))
            return status;
        break;
    case LIST_OR:
        if (!do_list_cmd(cmd->body, 1))
            return 0;
        break;
    case LIST_AMP:
        do_list_cmd(cmd->body, 0);
    }

    pid_t pgid = do_pipe_cmd(cmd->tail, 0, STDIN_FILENO, STDOUT_FILENO, fg);
    //printf("do_pipe_cmd -> %d\n", pgid);
    int status;

    if (interact) {
        if (fg) {
            check_status(status = run_fg(pgid, 0), pgid);
        } else {
            fprintf(stderr, "%d\n", pgid);
            job_add(pgid, 0);
        }
    } else {
        waitpid(pgid, &status, WUNTRACED);
    }

    return status;
}

static void
reap(void)
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, 0, WNOHANG)) > 0) {
        struct job *j, *next, *prev = (struct job *) &jobs;
        for (j=jobs; j; j=next) {
            next = j->next;
            if (j->pgid == pid) {
                fprintf(stderr, "%d\tDone\n", pid);
                free(j);
                prev->next = next;
            } else {
                prev = j;
            }
        }
    }
}

void
prompt(void)
{
    static char *text = "% ";
    if (interact) {
        reap();
        write(0, text, 2);
    }
}

void
run_cmd(struct list_cmd *cmd, bool fg)
{
    do_list_cmd(cmd, fg);
    free_list_cmd(cmd);
}

void
init(void)
{
    //printf("%d\n", getpid());
    if (interact = !from_file && isatty(0)) {
        while (tcgetpgrp(0) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0)
            err(1, "setpgid");

        tcsetpgrp(0, shell_pgid);
        //tcgetattr(0, &tmodes);
    }
}
