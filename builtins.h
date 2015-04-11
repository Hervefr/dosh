struct builtin {
    char *name;
    enum {
        BUILTIN_EXIT,
        BUILTIN_FG,
        BUILTIN_BG,
        BUILTIN_JOBS,
        BUILTIN_HISTORY,
        BUILTIN_CD,
        BUILTIN_PWD,
        BUILTIN_PUTS,
    } kind;
} *get_builtin(char *);
