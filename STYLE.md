# STYLE.md — toy.c coding conventions

Rules for all code in toy.c and its test files.
cx is a C99 subset; not everything compiles.
Read LANGUAGE.md and the cx quirks section of
PLAN.md before writing anything.

---

## Hard rules

### Layout

79 characters per line. Not 80. Count them.

Zero blank lines inside function bodies. Vertical
whitespace goes between functions, never inside.

Functions fit in ~24 lines. When logic gets complex,
extract a named helper. Do not nest deeper.

### Control flow

One return per function, at the bottom. Use a mutable
result variable and nested ifs. No early returns, no
mid-function bailouts.

    int cmd_foo(int argc, char **argv) {
        int rc = 0;
        if (argc < 1) {
            cx_err("foo: missing arg\n");
            rc = 1;
        }
        if (!rc) {
            // ... do work ...
        }
        return rc;
    }

No break in loops (switch/case break is fine).
No continue. Express exit conditions through the
loop predicate or extract a helper.

    // good
    int i = 0;
    int done = 0;
    while (i < n && !done) {
        if (data[i] == '\n') { done = 1; }
        if (!done) { i++; }
    }

    // bad — break
    while (i < n) {
        if (data[i] == '\n') break;
        i++;
    }

### Comments

Comments explain WHY, never WHAT. If the code needs
a comment to explain what it does, rewrite the code.

    // bad
    // open the file
    int fd = open(path, 0);

    // good — non-obvious platform detail
    // macOS O_CREAT differs from Linux, see top of file
    int fd = open(path, OPEN_CREATE, 420);

### Naming

Short names where meaning is clear from context.
Do not write current_line_buffer when buf is obvious.

    n len fd rc buf pos cap — good
    currentFileDescriptor   — bad

Utility functions prefixed with cx_ to avoid
collisions with cx intrinsics:

    cx_strchr cx_strstr cx_atoi cx_itoa
    cx_puts cx_out cx_err cx_getline cx_openrd

Command functions prefixed with cmd_:

    cmd_cat cmd_head cmd_sort cmd_sh

### Variables

Declared near first use, one per line, initialized.

    // good
    int fd = cx_openrd(argv[0]);
    char buf[BUFSZ];
    int n = read(fd, buf, BUFSZ);

    // bad — block of declarations at top
    int fd, n, i, rc;
    char buf[BUFSZ];
    char line[BUFSZ];

---

## cx-specific rules

### I/O: never mix printf and write

cx buffers printf output separately from write().
Mixing them in one function produces garbled output.
Pick ONE per function and stick with it.

For toy.c commands: use write() exclusively via the
cx_puts cx_out cx_err cx_putint helpers. Format
numbers with cx_itoa/cx_itopad into a local buffer,
then write.

printf is acceptable ONLY in test files that do not
call write() at all.

### No \t or \r in printf format strings

cx printf outputs \t as literal 't'. Use the integer
value instead:

    char tab = 9;
    write(1, &tab, 1);

Or embed it in a buffer:

    buf[pos] = 9;  // tab

### Platform constants

All platform-varying values are defined once at the
top of toy.c inside #if os() blocks. Never hardcode
magic numbers in command functions.

    #if os(linux)
    #define OPEN_CREATE 0x42
    #endif
    #if os(apple)
    #define OPEN_CREATE 0x201
    #endif

### Error output

All error messages go to stderr (fd 2) via cx_err.
Never use printf for errors — it goes to stdout and
is buffered.

    void cx_err(char *msg) {
        write(2, msg, strlen(msg));
    }

### Memory

malloc + free. No realloc (cx does not have it).
To grow a buffer: malloc new, memcpy old, free old.

    while (len + n >= cap) {
        int nc = cap * 2;
        char *nd = (char *)malloc(nc);
        memcpy(nd, data, len);
        free(data);
        data = nd;
        cap = nc;
    }

### Strings

cx has: strcpy strcmp strlen strcat strncmp memcmp.
cx lacks: strstr strchr strrchr strdup atoi snprintf.
These are reimplemented as cx_ prefixed utilities in
toy.c. Use the cx_ versions, never assume a libc
function exists unless it is listed in LANGUAGE.md
intrinsics.

### printf arg limit

cx printf accepts at most 6 arguments (format + 5
values). For commands that need more, break into
multiple printf calls or use write with manual
formatting.

---

## File organization

toy.c is one file with sections in this order:

    1. Includes (system headers — ignored by cx)
    2. Platform constants (#if os blocks)
    3. Forward declarations (if needed)
    4. Utility functions (cx_strchr, cx_atoi, etc)
    5. Shared data structures (struct lines, etc)
    6. Command functions (cmd_cat, cmd_head, etc)
       Ordered simplest-first, same as PLAN stages.
    7. Dispatch table and registration
    8. main()

No #include "file" for toy.c itself — it is one
translation unit. Test files may #include headers.

---

## Diff discipline

When modifying toy.c, touch only the code relevant
to the change. Do not reformat untouched functions.
Do not reorder sections. Do not rename things that
work. The best patch is the smallest correct one.
