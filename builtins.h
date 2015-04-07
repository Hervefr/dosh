struct builtin {
    char *name;
    enum {
        BUILTIN_FG = 1,
        BUILTIN_BG,
        BUILTIN_JOBS,
    } kind;
} *get_builtin(char *);
