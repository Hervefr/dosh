struct simple_cmd {
    char **argv;
    char *from;
    char *into;
    unsigned len;
    unsigned cap;
    bool append;
    bool bg;
};

/* a job */
struct pipe_cmd {
    struct simple_cmd *leader;
    struct pipe_cmd *rest;
};

struct list_cmd {
    enum list_op {
        LIST_UNIT,
        LIST_AND,
        LIST_OR,
        LIST_AMP,
    } kind;
    struct list_cmd *body;
    struct pipe_cmd *tail;
};

struct unit {
    enum {
        U_WORD,
        U_LIST,
        U_FROM,
        U_INTO,
        U_APPEND,
    } kind;
    union {
        /* weak ref */
        char *word;
        struct {
            /* element weak ref */
            char **list;
            unsigned len;
            unsigned cap;
        };
    };
};

struct redir {
    enum redir_kind {
        REDIR_FROM,
        REDIR_INTO,
        REDIR_APPEND,
    } kind;
    char *word;
};

void run_cmd(struct list_cmd *, bool);
