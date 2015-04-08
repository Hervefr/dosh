struct builtin {
    char *name;
    enum {
        BUILTIN_EXIT,
        BUILTIN_FG,
        BUILTIN_BG,
        BUILTIN_JOBS,
        BUILTIN_HISTORY,
    } kind;
} *get_builtin(char *);
