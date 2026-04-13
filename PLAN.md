# toy.c — busybox-style multi-call binary for cx

A single `toy.c` file compiled by cx that provides Unix
commands and a minimal shell. Invoked as:

    ./build/cx toy.c <command> [args...]

Or via wrapper scripts in ./bin:

    ./build/cx toy.c install ./bin
    export PATH=./bin:$PATH
    cat file.txt

The shell (`toy.c sh`) dispatches builtins in-process.
External commands spawn `cx toy.c <cmd>` via system/popen.

See STYLE.md for all coding conventions.

---

## cx runtime inventory

### What cx provides (32 intrinsics)

    File I/O   : open read write close lseek
    Memory     : malloc free alloca memset memcmp memcpy memmove
    Strings    : strcpy strcmp strlen strcat strncmp
    Process    : system popen pclose fread
    Mmap       : mmap munmap msync ftruncate
    File ops   : rename
    Convenience: memread memclose memwrite (whole-file helpers)
    Control    : printf (max 6 args) exit assert

### Predefined symbols

    O_RDONLY=0  O_WRONLY=1  O_RDWR=2
    O_CREAT=0x200  O_TRUNC=0x400       (macOS values)
    SEEK_SET=0  SEEK_CUR=1  SEEK_END=2
    PROT_READ=1  PROT_WRITE=2
    MAP_SHARED=1  MAP_PRIVATE=2  MAP_ANON=0x1000

### Language features that work

    structs, unions, typedefs, function pointers (incl callbacks)
    fixed-size arrays (local + global), array initializers
    #ifdef __linux__ / #ifdef __APPLE__ / #ifdef _WIN32
    #include "file", #define NAME value, #ifdef/#ifndef
    for(int i=0;...) scoping, mid-block declarations

### Known cx quirks (discovered empirically)

1. printf is buffered. write() bypasses the buffer.
   Mixing printf and write in one command produces
   out-of-order output. Use ONLY write() for I/O in
   toy.c commands, format numbers manually.

2. \t in printf format strings outputs literal 't'.
   Use char literal 9 for tab.

3. O_CREAT and O_TRUNC symbol values are macOS-only.
   On Linux the values differ. Use OS-specific blocks

   to define portable constants.

4. No variadic functions. printf is a special intrinsic.
   Cannot write our own printf-like functions.

5. No unsigned types. Bit manipulation and byte values
   need care (cast to int, mask with 0xFF).

6. No float/double. Irrelevant for Unix commands.

7. No snprintf/sprintf/sscanf/atoi/strstr/strchr.
   Must be implemented in toy.c as utility functions.

---

## What cx is MISSING for Unix commands

### Critical — no filesystem metadata

    stat lstat fstat      — file type, size, mode, timestamps
    opendir readdir closedir — directory listing
    unlink                — delete file
    mkdir rmdir           — create/remove directory
    chmod chown           — change permissions/ownership
    link symlink readlink — hard/soft links
    getcwd chdir          — working directory
    getenv                — environment variables
    utimes                — set file timestamps
    access                — test file accessibility
    isatty                — detect terminal
    dup2                  — redirect file descriptors

### Memory primitive

    realloc(ptr, size)    — grow/shrink an existing allocation [DONE]

Needed for any command that builds an unbounded buffer
(sort's line array, sh's token table, grep -r's result
list). Today these commands preallocate a fixed ceiling
(64 tokens, 256 lines, etc.) and silently truncate. A
single realloc intrinsic removes every one of those
ceilings without touching individual commands. One line
in the VM switch, one intrinsic() registration.

---

## Reference codebases

    toybox   — C implementations, POSIX-correct, tied to
               heavy lib layer (dirtree, loopfiles, FLAG macros).
               Use for algorithm reference and edge cases.

    cash     — ES6 implementations with vorpal CLI framework.
               Clean command logic under framework boilerplate.
               Good reference for cat, cp, rm, mv, touch, grep,
               sort, tail, head, ls, mkdir, which, kill.

    shelljs  — Node.js implementations, simpler than cash.
               Best reference for sed (95 lines, line-by-line
               replace), grep, sort, uniq, cp, rm, mv, find.
               Shows how far you can get without full POSIX.

    shx      — Thin CLI wrapper around shelljs. Minimal value
               beyond confirming the command surface area.

---

## Architecture

### File structure

One file: `toy.c`. Sections:

    1. Platform constants (#if os blocks, once at top)
    2. Utility library (strchr strstr atoi itoa getline etc)
    3. Command implementations (cmd_cat cmd_head ... cmd_sh)
    4. Dispatch table (function pointer array)
    5. main() — lookup argv[1], call command function

### Error output

All error messages go to stderr (fd 2) from Stage 0
onward. The rt_err helper writes directly via rt_write()
to avoid printf buffering issues. Every command uses
rt_err for diagnostics, never printf.

    void rt_err(char *msg) {
        rt_write(2, msg, strlen(msg));
    }

This is a foundation utility, not an afterthought.

### Dispatch model

    typedef int (*cmd_fn)(int argc, char **argv);
    struct cmd { char *name; cmd_fn fn; };

main() extracts command name from argv[1] (or from
argv[0] basename if invoked via symlink), looks up
in the table, calls with remaining args.

### Install command

    ./build/cx toy.c install ./bin

Creates ./bin directory and generates one wrapper
script per registered command:

    #!/bin/sh
    exec /path/to/cx /path/to/toy.c cat "$@"

The shell (toy.c sh) prepends ./bin to its PATH
automatically so installed commands are available
without manual PATH setup.

### Shell (sh) in-process model

When `toy.c sh` runs, the shell parses input lines
and dispatches builtins by calling cmd_xxx() directly
in the same cx VM process. No recompile cost per
command. Pipes between builtins use temp files (see
Decisions below). External commands go through system().

---

## Testing

### Philosophy

Every stage ships with tests. No command lands in
toy.c without a corresponding test. Tests catch
regressions across stages — a Stage 2 change must
not break Stage 1 commands.

### File naming

    tests/toys_<shortname>.c

One test file per stage or per logical group of
commands. Short, descriptive names:

    tests/toys_foundation.c  — Stage 0: dispatch, utils
    tests/toys_stream.c      — Stage 1: cat head tail etc
    tests/toys_text.c        — Stage 2: sort grep sed etc
    tests/toys_intrinsics.c  — Stage 3: stat readdir etc
    tests/toys_filesystem.c  — Stage 4: ls cp mv rm etc
    tests/toys_shell.c       — Stage 5: sh parsing, pipes
    tests/toys_selfhost.c    — self-hosting: cx→toy.c sh→cx

### Test runner

    tests/toys_tests.c

Executed as:

    ./build/cx tests/toys_tests.c

Runs all toy test files in stage order. Stops and
reports on first failure. Returns 0 if all pass.

The runner is a cx program that calls each test file
via system():

    int run(char *test) {
        char cmd[256];
        strcpy(cmd, "./build/cx ");
        strcat(cmd, test);
        printf("--- %s ---\n", test);
        int rc = system(cmd);
        if (rc != 0) {
            printf("FAIL: %s (status %d)\n", test, rc);
        }
        return rc;
    }

    int main() {
        int fail = 0;
        int total = 0;
        // Stage 0
        total++; if (run("tests/toys_foundation.c")) fail++;
        // Stage 1
        total++; if (run("tests/toys_stream.c")) fail++;
        // Stage 2
        total++; if (run("tests/toys_text.c")) fail++;
        // ... added as stages land
        printf("\n%d/%d passed\n", total-fail, total);
        return fail ? -1 : 0;
    }

New test files are appended to the runner as stages
are completed. The runner is the single entry point
for CI and manual verification.

### Test structure

Each test file is a standalone cx program. It uses
popen to invoke toy.c commands, captures output, and
compares against expected strings. Returns 0 if all
checks pass, -1 on first failure.

Test helper pattern (inside each test file):

    int pass = 0;
    int fail = 0;

    // capture output of a toy.c command
    int check(char *cmd, char *expect) {
        int ok = 0;
        char buf[4096];
        memset(buf, 0, 4096);
        int fp = popen(cmd, "r");
        if (fp) {
            fread(buf, 1, 4095, fp);
            pclose(fp);
            if (strcmp(buf, expect) == 0) {
                ok = 1;
            }
        }
        if (ok) {
            printf("  OK: %s\n", cmd);
            pass++;
        } else {
            printf("FAIL: %s\n", cmd);
            printf("  expected: %s\n", expect);
            printf("  got:      %s\n", buf);
            fail++;
        }
        return ok;
    }

    // check exit code only (no output compare)
    int check_rc(char *cmd, int expect) {
        int ok = 0;
        int rc = system(cmd);
        if (rc == expect) {
            ok = 1;
        }
        if (ok) {
            printf("  OK: %s -> %d\n", cmd, rc);
            pass++;
        } else {
            printf("FAIL: %s -> %d (want %d)\n",
                cmd, rc, expect);
            fail++;
        }
        return ok;
    }

Each test file ends with:

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;

### Test inputs

Tests create their own input files in build/ using
system("echo ... > build/test_input.txt") or by
writing via open/write/close. Tests clean up after
themselves. Never depend on files from previous tests.

All temp files go to build/ (gitignored), not /tmp/.
This avoids permission issues and keeps everything
inside the project tree.

### Stage 0 tests: tests/toys_foundation.c

    check_rc("./build/cx toy.c true", 0)
    check_rc("./build/cx toy.c false", 256)
      (system() returns exit<<8 on macOS)
    check("./build/cx toy.c echo hello", "hello\n")
    check("./build/cx toy.c echo -n hi", "hi")
    check("./build/cx toy.c echo a b c", "a b c\n")
    check("./build/cx toy.c basename /a/b/c", "c\n")
    check("./build/cx toy.c basename f.c .c", "f\n")
    check("./build/cx toy.c dirname /a/b/c", "/a/b\n")
    check("./build/cx toy.c dirname foo", ".\n")
    check("./build/cx toy.c seq 3", "1\n2\n3\n")
    check("./build/cx toy.c seq 2 4", "2\n3\n4\n")
    check("./build/cx toy.c seq 0 2 6", "0\n2\n4\n6\n")

### Stage 1 tests: tests/toys_stream.c

Create a test file via system(), then:

    check("./build/cx toy.c cat <testfile>",
          <expected contents>)
    check("./build/cx toy.c head -n 2 <testfile>",
          "line1\nline2\n")
    check("./build/cx toy.c tail -n 2 <testfile>",
          "line4\nline5\n")
    check("./build/cx toy.c wc <testfile>",
          "5 5 30 <testfile>\n")
    check("echo hello | ./build/cx toy.c rev",
          "olleh\n")
    check("echo -e 'a\nb\nc' | ./build/cx toy.c tac",
          "c\nb\na\n")
    check("./build/cx toy.c nl <testfile>",
          "     1\tline1\n     2\tline2\n...")

Plus edge cases: empty input, single line, no
trailing newline, binary data passthrough for cat.

### Stage 2 tests: tests/toys_text.c

    check("echo -e 'c\na\nb' | ./build/cx toy.c sort",
          "a\nb\nc\n")
    check("echo -e 'c\na\nb' | ./build/cx toy.c sort -r",
          "c\nb\na\n")
    check("echo -e 'a\na\nb' | ./build/cx toy.c uniq",
          "a\nb\n")
    check("echo -e 'a\na\nb' | ./build/cx toy.c uniq -c",
          "   2 a\n   1 b\n")
    check("echo 'a:b:c' | ./build/cx toy.c cut -d: -f2",
          "b\n")
    check("echo abc | ./build/cx toy.c tr abc xyz",
          "xyz\n")
    check("echo 'hello world' | ./build/cx toy.c "
          "grep hello", "hello world\n")
    check_rc("echo 'hello' | ./build/cx toy.c "
             "grep nope", 256)
    check("echo 'hello' | ./build/cx toy.c "
          "sed s/hello/bye/", "bye\n")

### Stage 3 tests: tests/toys_intrinsics.c

Tests for each new cx.c intrinsic. These are plain
cx test files (not toy.c tests) that directly call
the new intrinsics and verify return values:

    stat("cx.c", &st) returns 0, st.st_size > 0
    opendir(".") returns non-null
    readdir returns known filenames
    mkdir("build/test_dir", 0755) succeeds
    access("build/test_dir", F_OK) == 0
    rmdir("build/test_dir") succeeds
    getcwd returns non-empty string
    getenv("PATH") returns non-null
    unlink removes a file created by open+write

### Stage 4 tests: tests/toys_filesystem.c

    // setup
    system("./build/cx toy.c touch build/t1.txt")
    check_rc("./build/cx toy.c touch build/t2.txt", 0)
    check  ("./build/cx toy.c ls build/", ...)
    check_rc("./build/cx toy.c cp build/t1.txt "
             "build/t3.txt", 0)
    check_rc("./build/cx toy.c mv build/t3.txt "
             "build/t4.txt", 0)
    check_rc("./build/cx toy.c rm build/t4.txt", 0)
    check_rc("./build/cx toy.c mkdir build/td", 0)
    check_rc("./build/cx toy.c rmdir build/td", 0)
    check_rc("./build/cx toy.c mkdir -p "
             "build/a/b/c", 0)
    check_rc("./build/cx toy.c rm -r build/a", 0)

### Stage 5 tests: tests/toys_shell.c

    check("echo 'echo hello' | "
          "./build/cx toy.c sh",
          "hello\n")
    check("echo 'echo a; echo b' | "
          "./build/cx toy.c sh",
          "a\nb\n")
    check("echo 'true && echo yes' | "
          "./build/cx toy.c sh",
          "yes\n")
    check("echo 'false && echo no' | "
          "./build/cx toy.c sh",
          "")
    check("echo 'false || echo fallback' | "
          "./build/cx toy.c sh",
          "fallback\n")
    check("echo 'X=world; echo hello $X' | "
          "./build/cx toy.c sh",
          "hello world\n")

### Self-hosting test: tests/toys_selfhost.c

Verifies the full recursive chain works:
cx compiles toy.c → toy.c sh reads a script →
script invokes cx toy.c commands via system().

    // write a small script
    system("echo 'echo hello from sh' > "
           "build/selfhost_test.sh")
    check("echo 'source build/selfhost_test.sh' | "
          "./build/cx toy.c sh",
          "hello from sh\n")

    // deeper: sh invoking a pipeline
    check("echo 'seq 5 | sort -r | head -n 3' | "
          "./build/cx toy.c sh",
          "5\n4\n3\n")

Speed is not a concern here. Each spawned command
recompiles toy.c. This is acceptable for correctness
testing. Performance optimization (caching compiled
bytecode, keeping the VM warm) is a future concern
noted in the stretch goals.

### Running tests

Build cx, then run the toy test suite:

    cc -o build/cx cx.c
    ./build/cx tests/toys_tests.c

Or run a single stage's tests:

    ./build/cx tests/toys_foundation.c

The full cx test suite (tests/all.c) remains
separate and is not affected by toy.c tests.

---

## Staged plan

### Stage 0: Foundation

**Goal:** Skeleton that compiles under cx, dispatches
commands, handles the printf/write quirk.

**Deliverables:**
- toy.c with platform constants for O_CREAT etc
  using #ifdef __linux__ / #ifdef __APPLE__
- Utility library: cx_strchr cx_strrchr cx_strstr
  cx_atoi cx_itoa cx_itopad cx_putint cx_isdigit
  cx_isspace cx_isalpha cx_tolower cx_getline
  cx_openrd rt_puts rt_out rt_err readall
- rt_err writes to stderr (fd 2) from day one.
  All commands use rt_err for error messages.
- Dispatch table and main()
- Trivial commands: true false echo yes
  basename dirname seq
- tests/toys_foundation.c — all utils + commands tested
- tests/toys_tests.c — runner with Stage 0 entry

**Acceptance:**

    ./build/cx toy.c               # lists commands
    ./build/cx toy.c echo hello    # prints "hello"
    ./build/cx toy.c true; echo $? # 0
    ./build/cx toy.c false; echo $? # 1
    ./build/cx tests/toys_tests.c   # all tests pass

**Estimated size:** ~200 lines toy.c + ~80 lines tests

---

### Stage 1: Stream commands

**Goal:** Commands that transform stdin/files to stdout
using only open/read/write/close. No filesystem
metadata needed.

**Commands (11):**

    cat   — copy files to stdout
    head  — first N lines (-n)
    tail  — last N lines (-n)
    wc    — count lines/words/bytes (-l -w -c)
    tee   — copy stdin to stdout + files
    rev   — reverse each line
    tac   — reverse line order
    nl    — number lines
    fold  — wrap long lines (-w width)
    expand — tabs to spaces (-t tabstop)
    paste — merge lines from files (-d delim)

**Deliverables:**
- All 11 commands in toy.c
- tests/toys_stream.c — tests for every command
- tests/toys_tests.c — updated with Stage 1 entry

**Reference code:**
- toybox: cat.c (68 lines), head.c (74), echo.c (62),
  basename.c (47), dirname.c (25), nl.c (79), fold.c (97)
- shelljs: cat.js (76), head.js (107), tail.js (90)

**Key utility needed:** lines_t struct (dynamic array of
strings) with init/add/read/free. Reused by sort, uniq,
tac, tail.

**Acceptance:**

    ./build/cx tests/toys_tests.c   # all tests pass
    echo "hello world" | ./build/cx toy.c rev
    ./build/cx toy.c seq 10 | ./build/cx toy.c tac
    ./build/cx toy.c cat file | ./build/cx toy.c wc

**Estimated size:** +400 lines toy.c + ~120 lines tests

---

### Stage 2: Text processors

**Goal:** Commands that do string matching, field
extraction, sorting. Still no filesystem metadata.

**Commands (7):**

    sort   — sort lines (-r reverse)
    uniq   — deduplicate adjacent lines (-c count)
    cut    — extract fields (-d delim -f field)
    tr     — translate characters (set1 set2)
    grep   — fixed-string pattern match (-v -c -n -i)
    sed    — s/old/new/[g] only (no regex, no hold space)
    printf — format string with args

**Deliverables:**
- All 7 commands in toy.c
- tests/toys_text.c — tests for every command
- tests/toys_tests.c — updated with Stage 2 entry

**Reference code:**
- shelljs: sed.js (95 lines — just line.replace per line),
  grep.js (198), sort.js (98), uniq.js (93)
- cash: sort.js, grep.js (both clean implementations)
- toybox: sort.c (400), grep.c (548), sed.c (1117 — full
  POSIX sed, we implement ~10% of it)

**Design notes:**
- grep: fixed-string via cx_strstr. No regex engine.
  -i flag via tolower comparison. Sufficient for most use.
- sed: parse s/old/new/[g] expressions only. The shelljs
  approach (per-line string replacement) is the right model.
  Full POSIX sed (addresses, hold/pattern space, labels,
  branches) is a Stage 6+ stretch goal.
- sort: insertion sort is fine for typical inputs.
  Replace with merge sort if perf matters.

**Acceptance:**

    ./build/cx tests/toys_tests.c   # all tests pass
    echo -e "b\na\nc" | ./build/cx toy.c sort
    ./build/cx toy.c grep foo file.txt
    echo "hello world" | ./build/cx toy.c sed s/hello/goodbye/
    echo "a:b:c" | ./build/cx toy.c cut -d: -f2

**Estimated size:** +350 lines toy.c + ~100 lines tests

---

### Stage 3: cx intrinsics for filesystem

**Goal:** Add minimal intrinsics to cx.c to unlock
filesystem-manipulating commands. Each is one line
in the VM switch plus one intrinsic() registration.

**New intrinsics (batch 1 — unlock Stage 4):**

    stat(path, buf)       — populate struct with st_mode
                            st_size st_mtime
    opendir(path)         — returns opaque DIR handle
    readdir(dirp)         — returns name string (or 0)
    closedir(dirp)        — close DIR handle
    unlink(path)          — delete file
    mkdir(path, mode)     — create directory
    rmdir(path)           — remove empty directory
    getcwd(buf, size)     — get working directory
    chdir(path)           — change directory
    getenv(name)          — read environment variable
    access(path, mode)    — test file permissions

**New intrinsics (batch 2 — unlock Stage 5 shell):**

    dup2(old, new)        — redirect file descriptors
    pipe(fds)             — create pipe pair

**New intrinsics (batch 3 — nice to have):**

    chmod(path, mode)     — change permissions
    link(old, new)        — hard link
    symlink(old, new)     — symbolic link
    readlink(path, buf, size) — read symlink target
    utimes(path, times)   — set timestamps

**Predefined symbols to add:**

    S_IFMT S_IFDIR S_IFREG S_IFLNK   (stat mode masks)
    S_ISDIR(m) S_ISREG(m) S_ISLNK(m) (as macros or helpers)
    R_OK W_OK X_OK F_OK              (access mode flags)
    O_APPEND                         (missing today)

**Stat struct design for cx:**

    struct cx_stat {
        int st_mode;    // file type + permissions
        int st_size;    // file size in bytes
        int st_mtime;   // modification time (unix epoch)
        int st_nlink;   // number of hard links
        int st_uid;     // owner uid
    };

The intrinsic maps native struct stat fields into this
simplified cx struct. cx code never sees the native layout.

**Deliverables:**
- cx.c with batch 1 intrinsics
- tests/toys_intrinsics.c — exercises every new intrinsic
- tests/toys_tests.c — updated with Stage 3 entry

**Acceptance:**

    ./build/cx tests/toys_tests.c   # all tests pass
    ./build/cx tests/toys_intrinsics.c  # intrinsic tests

**Estimated cx.c changes:** ~80 lines (intrinsics) +
~30 lines (symbols) + ~20 lines (stat struct helper).
Split into two PRs: batch 1 first, batch 2 after.

---

### Stage 4: Filesystem commands

**Goal:** Commands that create, delete, copy, move files
and directories. Requires Stage 3 intrinsics.

**Commands (13):**

    ls      — list directory (-l -a -1)
    touch   — create file / update mtime
    mkdir   — create directory (-p parents)
    rmdir   — remove empty directory
    rm      — remove files/dirs (-r -f)
    cp      — copy files (-r recursive)
    mv      — move/rename files
    ln      — create links (-s symbolic)
    chmod   — change permissions
    pwd     — print working directory
    cd      — change directory (shell builtin only)
    env     — print or run with modified environment
    install — generate wrapper scripts in ./bin

**Install command:**

    ./build/cx toy.c install ./bin

Creates the target directory and writes one wrapper
script per registered command:

    #!/bin/sh
    exec cx toy.c cat "$@"

Uses absolute paths resolved from getcwd at install
time. The shell (toy.c sh) prepends the install dir
to PATH so installed commands are found automatically.

**Deliverables:**
- All 13 commands in toy.c
- tests/toys_filesystem.c — tests for every command
- tests/toys_tests.c — updated with Stage 4 entry

**Reference code:**
- toybox: ls.c (644), cp.c (544), rm.c (117), mkdir.c (44)
- cash: cp.js (170), rm.js (130), mv.js (40), ls.js (250),
  mkdir.js (30), touch.js (150)
- shelljs: cp.js, rm.js (201), mv.js (119), ls.js (155)

**Design notes:**
- ls: minimal version first — just names from readdir.
  -l adds stat calls for mode/size/mtime formatting.
  -a includes dotfiles. Sort alphabetically.
- cp: for files, open/read/write loop. For -r, recursive
  readdir + mkdir + copy. Cash's copyFileSync is the model.
- mv: try rename() first (same filesystem). If EXDEV,
  fall back to cp + rm.
- rm -r: recursive readdir, unlink files, rmdir dirs.
  Cash's rmdirSyncRecursive is the model.

**Acceptance:**

    ./build/cx tests/toys_tests.c   # all tests pass
    ./build/cx toy.c touch build/foo
    ./build/cx toy.c ls build/
    ./build/cx toy.c cp build/foo build/bar
    ./build/cx toy.c mv build/bar build/baz
    ./build/cx toy.c rm build/baz
    ./build/cx toy.c mkdir -p build/a/b/c
    ./build/cx toy.c rm -r build/a
    ./build/cx toy.c install ./bin && ./bin/echo hi

**Estimated size:** +500 lines toy.c + ~120 lines tests

---

### Stage 5: Shell (sh)

**Goal:** Minimal interactive shell. In-process dispatch
for builtins. rc/Plan9-inspired, not POSIX sh.

**Features (priority order):**

    P0 — Line reading, tokenizing, command dispatch
    P0 — In-process builtin calls (no recompile)
    P0 — Exit status ($?) and && || chaining
    P0 — Variable assignment and expansion (VAR=val, $VAR)
    P0 — Quoting: "double" and 'single'
    P1 — Pipes via temp files: cmd1 | cmd2
    P1 — Redirects: > >> < (using dup2 intrinsic)
    P1 — cd, export, set, source builtins
    P1 — Semicolon chaining: cmd1 ; cmd2
    P2 — Glob expansion: *.c (using readdir + pattern match)
    P2 — if/else/while/for control flow
    P2 — Background: cmd & (using system() in fire-and-forget)
    P3 — Here-documents: cmd <<EOF ... EOF
    P3 — Command substitution: $(cmd) or `cmd`
    P3 — Functions

**Deliverables:**
- cmd_sh in toy.c
- tests/toys_shell.c — tests for P0 and P1 features
- tests/toys_selfhost.c — self-hosting recursion test
- tests/toys_tests.c — updated with Stage 5 entries

**Shell architecture:**

    1. Read line (cx_getline from stdin or source file)
    2. Tokenize: split on whitespace, respect quotes,
       expand $VAR references, handle escapes
    3. Parse: identify pipes, redirects, && || ; &
    4. Execute segments left to right:
       - Builtin? Call cmd_xxx() directly
       - External? system("cx toy.c cmd args")
       - Pipe? Write left output to tmpfile,
         open as stdin for right side

**Token structure:**

    struct token {
        char *text;     // token string
        int type;       // WORD PIPE REDIR_OUT REDIR_IN
                        // REDIR_APPEND AND OR SEMI BG
    };

**Variable table:** Simple array of name=value strings,
searched linearly. 256 entries is plenty. getenv for
inherited variables, local table for shell-set ones.

**Pipe implementation (temp files):**

Without fork/exec, pipes materialize intermediate
results. For `A | B`:

    1. Execute A with stdout redirected to tmpfile
    2. Execute B with stdin redirected from tmpfile
    3. Delete tmpfile

With dup2 intrinsic this is clean. Without it,
A writes to a file explicitly and B reads from it.
This means piped builtins need to accept an "output fd"
parameter, or we temporarily swap a global stdout_fd.

Future optimization: replace temp files with mmap'd
shared memory buffers. The pipe producer writes into
an anonymous mmap region; the consumer reads from it.
No filesystem I/O, no cleanup. Defer until temp file
pipes prove to be a bottleneck.

**Source builtin:** Read file line by line, execute each
line through the same parse/dispatch loop. Recursive.

**Self-hosting test:** The full chain must work:
cx compiles toy.c → sh reads script → script spawns
cx toy.c commands. Each spawn recompiles toy.c.
This is slow but correct. Speed optimization (bytecode
caching, warm VM reuse) is a Stage 6+ concern.

**Acceptance:**

    ./build/cx tests/toys_tests.c   # all tests pass
    echo "echo hello" | ./build/cx toy.c sh
    echo "ls | grep .c | sort" | ./build/cx toy.c sh
    echo 'X=world; echo hello $X' | ./build/cx toy.c sh
    echo 'cat toy.c | wc' | ./build/cx toy.c sh

**Estimated size:** +600 lines toy.c + ~100 lines tests

---

### Stage 6: Advanced (stretch goals)

**Commands and features for later:**

    find    — recursive file search with predicates
    xargs   — build commands from stdin
    test    — file/string test expressions ([ ])
    date    — needs time()/localtime() intrinsics
    sleep   — needs sleep() or nanosleep() intrinsic
    which   — search PATH for executable
    kill    — needs kill() intrinsic
    ps      — needs /proc or sysctl, platform-specific

**Bytecode executables (-o flag):**

    cx -o hello hello.c

Compile a source file once and write a self-executing
bytecode file. Output format: a shebang line followed by
the bytecode encoded as hex text (64 bytes per line) so
the file remains a plain text artifact that any editor
can display.

    #!/usr/bin/env cx --exec
    # cx bytecode v1
    7f4358010000000000000000000000000000000000000000...
    ...

The first line is a shebang invoking cx in --exec mode,
which decodes the hex back into the in-memory opcode
stream and runs it directly without re-parsing the
source. The hex envelope means:

- Files are diffable, grep-able, copy-pasteable
- chmod +x makes them runnable from any shell
- No platform-specific binary format needed
- Minimal cx code change (read+decode is straightforward)

This is the chosen approach for distributing cx programs.
Per-shell-spawn bytecode caching is explicitly NOT
pursued - the executable file IS the cached bytecode.

**Warm VM:** keep the VM alive across shell command
dispatches instead of respawning cx per command (still
worth doing for the shell's external command path).

**Regex engine:**

A ~200-line NFA regex engine in cx would upgrade grep
and sed from fixed-string to real pattern matching.
Thompson NFA construction. Support: . * + ? [] ^ $ |
and capture groups \( \). Not a priority until basic
grep -F and sed s/// are battle-tested.

Reference implementation: `local/tiny-regex-c/` (kokke,
public domain, ~500 SLOC). Zero malloc, all static arrays,
iterative matching (no recursion). Supports . ^ $ * + ?
[a-z] [^abc] \s\S \w\W \d\D. Known gaps: | (alternation)
broken, no capture groups. The struct uses a union (ch vs
ccl pointer) which cx handles. Good starting point — adapt
and trim to ~200-300 lines for cx, fix inverted classes,
possibly add alternation.

**Full sed:**

POSIX sed beyond s///: address ranges (line numbers,
/pattern/), multiple commands ({...}), hold/pattern
space (h H g G x), labels and branches (: b t),
delete/print/append (d p a i c), read/write (r w).
This is essentially a small language interpreter.
~500-800 lines. Defer until there's a real need.

**Pipe optimization:**

Replace temp-file pipes with mmap'd anonymous shared
memory. Producer writes into MAP_ANON region, consumer
reads directly. Zero filesystem overhead. Requires
careful size management (grow via mremap or pre-allocate
a generous buffer).

---

## cx.c change policy

Keep cx minimalistic. Each new intrinsic must:

1. Be a direct 1:1 mapping to a POSIX syscall
2. Require at most 3 lines in the VM switch
3. Unlock at least 2 toy.c commands
4. Not duplicate something achievable with existing
   intrinsics (no convenience wrappers)

Exception: the stat struct helper (~20 lines) which
translates native stat to cx's simplified layout.
This is justified because struct stat varies wildly
between platforms and cx code should not care.

---

## Decisions

1. **Install target.** `toy.c install ./bin` writes
   wrapper scripts into ./bin (local, does not touch
   system directories). The shell prepends ./bin to
   PATH automatically.

2. **Pipe buffering.** Start with temp files in build/.
   Future: mmap'd shared memory buffers (noted in
   Stage 5 and Stage 6 stretch goals).

3. **Error output.** rt_err (write to fd 2) is a Stage 0
   deliverable. Every command uses it from day one.
   Never printf for diagnostics.

4. **Self-hosting test.** Explicit test file:
   tests/toys_selfhost.c. Exercises the full recursive
   chain: cx → toy.c sh → system → cx → toy.c cmd.
   Speed is not a concern now. Performance optimization
   (bytecode caching, warm VM) deferred to Stage 6.

---

## Milestone checklist

    [x] Stage 0 — foundation + tests pass
    [x] Stage 1 — 11 stream commands + tests pass
    [x] Stage 2 — 7 text processors + tests pass
    [x] Stage 3 — cx.c intrinsics batch 1 + tests pass
    [x] Stage 4 — 13 filesystem commands + tests pass
    [x] Stage 3b — cx.c intrinsics batch 2 + tests pass
    [x] Stage 5 — shell P0+P1 + tests pass
    [x] Stage 5 — self-hosting test passes
    [x] Stage 5b — shell P2 features + tests pass
    [x] Stage 6 — stretch goals: realloc, find, xargs, test, which, date, sleep, kill
    [x] Stage 6 — PIC and PID implementation (relative jumps and offsets)
    [x] cx self-hosts cleanly — tests/all.c runs every test twice (native
        and cx-on-cx), 45/45 passing on both paths. See MEMORY.md for the
        five bugs that had to be fixed to get here.
    [x] Stage 6 — regex engine (~180 lines, adapted from tiny-regex-c):
        . ^ $ * + ? [abc] [^abc] [a-z] \s\S \w\W \d\D — wired into
        grep and sed (both now do real pattern matching, not just strstr)
    [x] Stage 6 — ps (/proc on Linux, system ps fallback on macOS)
    [x] Stage 6 — full sed: addresses (line, $, /regex/), ranges,
        commands (s///[g], d, p, q), -n suppress, -e multi-expression
    [x] Stage 7 — interactive shell: readline with history/arrows,
        isatty/termraw/winsize intrinsics, help/exit builtins,
        toys invoked bare enters sh mode (warm VM)
    [x] Stage 7 — vi: minimal screen editor (insert/normal/ex modes,
        arrows, dd, x, A, I, o/O, /search, :w :wq :q!)
    [x] Stage 8 — cx as embedded interpreter:
        (a) cx builtin finds cx binary (adjacent to argv[0], build/,
            or PATH) and runs any file.c with main()
        (b) cx.c skips #! shebang lines — chmod +x file.c works
            (shebang: #!/usr/bin/env cx)
        (c) shell prompt shows basename of cwd (e.g. "c4.devs$ "),
            now driven by PS1 variable (see Stage 10)
    [x] Stage 9 — LLM agent discoverability:
        ls -lah with permissions + human sizes, --help for all commands
        shows available flags so agents can introspect the tool suite
    [x] Stage 10 — PS1 customizable prompt:
        sh_expand_prompt() expands \w (basename cwd), \W (full cwd),
        \u ($USER), \h ($HOSTNAME short), \$ (literal $), \n (newline),
        plus $VAR expansion. Default PS1='\w$ ' (matches prior behavior).
    [x] Stage 10 — shell UX polish:
        (a) ~ expansion: ~ and ~/path expand to $HOME
        (b) type builtin: classifies builtins vs PATH executables
        (c) tab completion for registered command names (single/multi match)
    [x] cx.c: assert() with stack trace, __FILE__, __LINE__:
        assert(cond) is an intrinsic (I_ASRT). On failure prints
        file:line, function name, full call chain (up to 16 frames),
        then exit(-1). fn_name_at() maps code addresses to function
        names via sym_pool. __FILE__ and __LINE__ are lexer builtins.
        tests/assertions.c covers pass-through, expressions, __LINE__
        advancement, __FILE__ string, and subprocess assert(0) failure.
    [~] Stage 11 — cx.c: float, double, unsigned, long long
        `long long` parser sugar done (two-token specifier → INT64).
        All bare `int` in cx.c and toys.c converted to int32_t/int64_t
        (except `int main` and keyword registration string).
    [ ] Stage 12 — sh: $((expr)) arithmetic expansion (needs Stage 11)

---

## Progress log

See `MEMORY.md` — timestamped entries for each checkpoint landed against
this plan. New work goes on top; each entry cites the commit(s) that
implemented it.
