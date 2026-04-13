#ifndef __cx__
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include <termios.h>
#include <sys/ioctl.h>
static struct termios _tr_orig;
static int _tr_saved;
static void _sigwinch(int sig) { (void)sig; }
static int winsize(int fd, int *buf) {
    struct winsize ws;
    int r = ioctl(fd, TIOCGWINSZ, &ws);
    if (r == 0) { buf[0] = ws.ws_row; buf[1] = ws.ws_col; }
    return r;
}
static int termraw(int fd, int enable) {
    if (enable) {
        if (!_tr_saved) {
            tcgetattr(fd, &_tr_orig);
            _tr_saved = 1;
        }
        struct termios raw = _tr_orig;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = _sigwinch;
        sigaction(SIGWINCH, &sa, 0);
        return tcsetattr(fd, TCSAFLUSH, &raw);
    }
    if (_tr_saved) {
        signal(SIGWINCH, SIG_DFL);
        return tcsetattr(fd, TCSAFLUSH, &_tr_orig);
    }
    return 0;
}
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

static void cx_write(int fd, const void * buf, int n) {
    int r = (int)write(fd, buf, n);
    (void)r;
}

static void cx_err(char * s) {
    cx_write(2, s, (int)strlen(s));
}

static void cx_out(char * s, int n) {
    cx_write(1, s, n);
}

static void cx_puts(char * s) {
    cx_write(1, s, (int)strlen(s));
}

struct sb {
    int count;
    int capacity;
    char * data;
};

static void * sb_oom(void * p) {
    if (!p) { cx_write(2, "Out of Memory\n", 14); exit(1); }
    return p;
}

static void sb_grow(struct sb * b, int extra) {
    int needed = b->count + extra + 1;
    if (needed > b->capacity) {
        b->capacity = needed * 2;
        if (b->data) {
            b->data = (char *)sb_oom(realloc(b->data, b->capacity));
        } else {
            b->data = (char *)sb_oom(malloc(b->capacity));
            b->data[0] = 0;
        }
    }
}

static void sb_put(struct sb * b, const char * d, int bytes) {
    sb_grow(b, bytes);
    memcpy(b->data + b->count, d, bytes);
    b->count = b->count + bytes;
    b->data[b->count] = '\0';
}

static void sb_puts(struct sb * b, const char * s) {
    if (s) { sb_put(b, s, (int)strlen(s)); }
}

static void sb_putc(struct sb * b, char c) { sb_put(b, &c, 1); }

static void sb_free(struct sb * b) {
    if (b->data) { free(b->data); }
    b->data = 0;
    b->count = 0;
    b->capacity = 0;
}

static int cx_itoa(int n, char * buf) {
    char tmp[24];
    int len = 0;
    int neg = 0;
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    if (n == 0) {
        tmp[len] = '0';
        len++;
    }
    while (n > 0) {
        tmp[len] = '0' + n % 10;
        len++;
        n = n / 10;
    }
    int out = 0;
    if (neg) {
        buf[out] = '-';
        out++;
    }
    int i = len - 1;
    while (i >= 0) {
        buf[out] = tmp[i];
        out++;
        i--;
    }
    buf[out] = 0;
    return out;
}

static int cx_itopad(int n, char * buf, int w) {
    char tmp[24];
    int len = cx_itoa(n, tmp);
    int pad = w - len;
    int i = 0;
    if (pad < 0) {
        pad = 0;
    }
    while (i < pad) {
        buf[i] = ' ';
        i++;
    }
    memcpy(buf + pad, tmp, len + 1);
    return pad + len;
}

static void cx_putint(int fd, int n) {
    char buf[24];
    int len = cx_itoa(n, buf);
    cx_write(fd, buf, len);
}

static int cx_getline(int fd, char * buf, int cap) {
    int n = 0;
    int done = 0;
    char c = 0;
    int r = 0;
    while (!done && n < cap - 1) {
        r = read(fd, &c, 1);
        if (r <= 0) {
            done = 1;
        }
        if (!done) {
            buf[n] = c;
            n++;
            if (c == '\n') {
                done = 1;
            }
        }
    }
    buf[n] = 0;
    return n;
}

static int cx_openrd(char * path) {
    int fd = open(path, 0);
    if (fd < 0) {
        cx_err("cannot open: ");
        cx_err(path);
        cx_err("\n");
    }
    return fd;
}

struct lines {
    char ** data;
    int * lens;
    int count;
    int cap;
};

static void lines_init(struct lines * l) {
    l->cap = 64;
    l->count = 0;
    l->data = (char**)malloc(l->cap * 8);
    l->lens = (int*)malloc(l->cap * 8);
}

static void lines_grow(struct lines * l) {
    int nc = l->cap * 2;
    l->data = (char**)realloc(l->data, nc * 8);
    l->lens = (int*)realloc(l->lens, nc * 8);
    l->cap = nc;
}

static void lines_add(struct lines * l, char * s, int len) {
    if (l->count >= l->cap) {
        lines_grow(l);
    }
    char * copy = (char*)malloc(len + 1);
    memcpy(copy, s, len);
    copy[len] = 0;
    l->data[l->count] = copy;
    l->lens[l->count] = len;
    l->count++;
}

static void lines_free(struct lines * l) {
    int i = 0;
    while (i < l->count) {
        free(l->data[i]);
        i++;
    }
    free(l->data);
    free(l->lens);
}

static int lines_read(struct lines * l, int fd) {
    char buf[4096];
    int n = cx_getline(fd, buf, 4096);
    while (n > 0) {
        lines_add(l, buf, n);
        n = cx_getline(fd, buf, 4096);
    }
    return l->count;
}

typedef int (*cmd_fn)(int, char **);

struct cmd {
    char * name;
    cmd_fn fn;
    char * help;
};

struct cmd cmds[64];
int ncmds = 0;
char cx_path[4096];

static int cmd_true(int argc, char ** argv) {
    return 0;
}

static int cmd_false(int argc, char ** argv) {
    return 1;
}

static int cmd_echo(int argc, char ** argv) {
    int rc = 0;
    int nl = 1;
    int i = 1;
    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        nl = 0;
        i = 2;
    }
    int first = i;
    while (i < argc) {
        if (i > first) {
            cx_out(" ", 1);
        }
        cx_puts(argv[i]);
        i++;
    }
    if (nl) {
        cx_out("\n", 1);
    }
    return rc;
}

static int cmd_yes(int argc, char ** argv) {
    int rc = 0;
    char * msg = "y";
    if (argc >= 2) {
        msg = argv[1];
    }
    while (1) {
        cx_puts(msg);
        cx_out("\n", 1);
    }
    return rc;
}

static int cmd_basename(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cx_err("basename: missing operand\n");
        rc = 1;
    }
    if (!rc) {
        char * path = argv[1];
        char * suf = argc >= 3 ? argv[2] : 0;
        int len = strlen(path);
        while (len > 1 && path[len - 1] == '/') {
            len--;
        }
        char * base = path;
        int i = 0;
        while (i < len) {
            if (path[i] == '/') {
                base = path + i + 1;
            }
            i++;
        }
        int blen = len - (base - path);
        if (blen <= 0) {
            base = "/";
            blen = 1;
        }
        if (suf) {
            int slen = strlen(suf);
            if (blen > slen &&
                memcmp(base + blen - slen, suf, slen) == 0) {
                blen -= slen;
            }
        }
        cx_write(1, base, blen);
        cx_out("\n", 1);
    }
    return rc;
}

static int cmd_dirname(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cx_err("dirname: missing operand\n");
        rc = 1;
    }
    if (!rc) {
        char * path = argv[1];
        int len = strlen(path);
        while (len > 1 && path[len - 1] == '/') {
            len--;
        }
        int last = -1;
        int i = 0;
        while (i < len) {
            if (path[i] == '/') {
                last = i;
            }
            i++;
        }
        if (last < 0) {
            cx_out(".", 1);
        } else if (last == 0) {
            cx_out("/", 1);
        } else {
            cx_write(1, path, last);
        }
        cx_out("\n", 1);
    }
    return rc;
}

static int cmd_seq(int argc, char ** argv) {
    int rc = 0;
    int start = 1;
    int step = 1;
    int end = 0;
    if (argc < 2) {
        cx_err("seq: missing argument\n");
        rc = 1;
    }
    if (!rc && argc == 2) {
        end = atoi(argv[1]);
    }
    if (!rc && argc == 3) {
        start = atoi(argv[1]);
        end = atoi(argv[2]);
    }
    if (!rc && argc >= 4) {
        start = atoi(argv[1]);
        step = atoi(argv[2]);
        end = atoi(argv[3]);
    }
    if (!rc && step == 0) {
        cx_err("seq: step cannot be 0\n");
        rc = 1;
    }
    if (!rc) {
        int i = start;
        int going = step > 0 ? i <= end : i >= end;
        while (going) {
            cx_putint(1, i);
            cx_out("\n", 1);
            i += step;
            going = step > 0 ? i <= end : i >= end;
        }
    }
    return rc;
}

static void cat_fd(int fd) {
    char buf[4096];
    int n = read(fd, buf, 4096);
    while (n > 0) {
        cx_write(1, buf, n);
        n = read(fd, buf, 4096);
    }
}

static int cmd_cat(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cat_fd(0);
    } else {
        int i = 1;
        while (i < argc && !rc) {
            int fd = cx_openrd(argv[i]);
            if (fd < 0) {
                rc = 1;
            } else {
                cat_fd(fd);
                close(fd);
            }
            i++;
        }
    }
    return rc;
}

static void head_fd(int fd, int n) {
    char buf[4096];
    int line = 0;
    int done = 0;
    while (!done && line < n) {
        int len = cx_getline(fd, buf, 4096);
        if (len <= 0) {
            done = 1;
        }
        if (!done) {
            cx_write(1, buf, len);
            line++;
        }
    }
}

static int cmd_head(int argc, char ** argv) {
    int rc = 0;
    int n = 10;
    int i = 1;
    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        n = atoi(argv[2]);
        i = 3;
    }
    if (i >= argc) {
        head_fd(0, n);
    } else {
        while (i < argc && !rc) {
            int fd = cx_openrd(argv[i]);
            if (fd < 0) {
                rc = 1;
            } else {
                head_fd(fd, n);
                close(fd);
            }
            i++;
        }
    }
    return rc;
}

static int cmd_tail(int argc, char ** argv) {
    int rc = 0;
    int n = 10;
    int i = 1;
    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        n = atoi(argv[2]);
        i = 3;
    }
    struct lines l;
    lines_init(&l);
    if (i >= argc) {
        lines_read(&l, 0);
    } else {
        while (i < argc && !rc) {
            int fd = cx_openrd(argv[i]);
            if (fd < 0) {
                rc = 1;
            } else {
                lines_read(&l, fd);
                close(fd);
            }
            i++;
        }
    }
    if (!rc) {
        int start = l.count - n;
        if (start < 0) {
            start = 0;
        }
        int j = start;
        while (j < l.count) {
            cx_write(1, l.data[j], l.lens[j]);
            j++;
        }
    }
    lines_free(&l);
    return rc;
}

static void wc_fd(int fd, int * wl, int * ww, int * wb) {
    char buf[4096];
    int in_word = 0;
    int nl = 0;
    int nw = 0;
    int nb = 0;
    int n = read(fd, buf, 4096);
    while (n > 0) {
        nb = nb + n;
        int j = 0;
        while (j < n) {
            if (buf[j] == '\n') {
                nl++;
            }
            if (isspace(buf[j])) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                nw++;
            }
            j++;
        }
        n = read(fd, buf, 4096);
    }
    *wl = *wl + nl;
    *ww = *ww + nw;
    *wb = *wb + nb;
}

static int cmd_wc(int argc, char ** argv) {
    int rc = 0;
    int i = 1;
    int wl = 0;
    int ww = 0;
    int wb = 0;
    if (i >= argc) {
        wc_fd(0, &wl, &ww, &wb);
        cx_putint(1, wl);
        cx_out(" ", 1);
        cx_putint(1, ww);
        cx_out(" ", 1);
        cx_putint(1, wb);
        cx_out("\n", 1);
    } else {
        while (i < argc && !rc) {
            wl = 0;
            ww = 0;
            wb = 0;
            int fd = cx_openrd(argv[i]);
            if (fd < 0) {
                rc = 1;
            } else {
                wc_fd(fd, &wl, &ww, &wb);
                close(fd);
                cx_putint(1, wl);
                cx_out(" ", 1);
                cx_putint(1, ww);
                cx_out(" ", 1);
                cx_putint(1, wb);
                cx_out(" ", 1);
                cx_puts(argv[i]);
                cx_out("\n", 1);
            }
            i++;
        }
    }
    return rc;
}

static int cmd_tee(int argc, char ** argv) {
    int rc = 0;
    int fds[16];
    int nfds = 0;
    int i = 1;
    while (i < argc && nfds < 16 && !rc) {
        fds[nfds] = open(argv[i], 1 | O_CREAT | O_TRUNC, 420);
        if (fds[nfds] < 0) {
            cx_err("tee: cannot open: ");
            cx_err(argv[i]);
            cx_err("\n");
            rc = 1;
        } else {
            nfds++;
        }
        i++;
    }
    if (!rc) {
        char buf[4096];
        int n = read(0, buf, 4096);
        while (n > 0) {
            cx_write(1, buf, n);
            int j = 0;
            while (j < nfds) {
                cx_write(fds[j], buf, n);
                j++;
            }
            n = read(0, buf, 4096);
        }
    }
    int j = 0;
    while (j < nfds) {
        close(fds[j]);
        j++;
    }
    return rc;
}

static int cmd_rev(int argc, char ** argv) {
    int rc = 0;
    char buf[4096];
    int n = cx_getline(0, buf, 4096);
    while (n > 0) {
        int lo = 0;
        int hi = n - 1;
        if (hi >= 0 && buf[hi] == '\n') {
            hi--;
        }
        while (lo < hi) {
            char t = buf[lo];
            buf[lo] = buf[hi];
            buf[hi] = t;
            lo++;
            hi--;
        }
        cx_write(1, buf, n);
        n = cx_getline(0, buf, 4096);
    }
    return rc;
}

static int cmd_tac(int argc, char ** argv) {
    int rc = 0;
    struct lines l;
    lines_init(&l);
    if (argc < 2) {
        lines_read(&l, 0);
    } else {
        int i = 1;
        while (i < argc && !rc) {
            int fd = cx_openrd(argv[i]);
            if (fd < 0) {
                rc = 1;
            } else {
                lines_read(&l, fd);
                close(fd);
            }
            i++;
        }
    }
    if (!rc) {
        int i = l.count - 1;
        while (i >= 0) {
            cx_write(1, l.data[i], l.lens[i]);
            i--;
        }
    }
    lines_free(&l);
    return rc;
}

static int cmd_nl(int argc, char ** argv) {
    int rc = 0;
    int fd = 0;
    if (argc >= 2) {
        fd = cx_openrd(argv[1]);
        if (fd < 0) {
            rc = 1;
        }
    }
    if (!rc) {
        char buf[4096];
        char num[32];
        int line = 1;
        int n = cx_getline(fd, buf, 4096);
        while (n > 0) {
            int nlen = cx_itopad(line, num, 6);
            cx_write(1, num, nlen);
            cx_out("\t", 1);
            cx_write(1, buf, n);
            line++;
            n = cx_getline(fd, buf, 4096);
        }
    }
    if (fd > 0) {
        close(fd);
    }
    return rc;
}

static int cmd_fold(int argc, char ** argv) {
    int rc = 0;
    int w = 80;
    if (argc > 2 && strcmp(argv[1], "-w") == 0) {
        w = atoi(argv[2]);
    }
    char buf[4096];
    int col = 0;
    int n = read(0, buf, 4096);
    while (n > 0) {
        int i = 0;
        while (i < n) {
            if (buf[i] == '\n') {
                cx_out("\n", 1);
                col = 0;
            } else {
                if (col >= w) {
                    cx_out("\n", 1);
                    col = 0;
                }
                cx_write(1, buf + i, 1);
                col++;
            }
            i++;
        }
        n = read(0, buf, 4096);
    }
    return rc;
}

static int cmd_expand(int argc, char ** argv) {
    int rc = 0;
    int tab = 8;
    if (argc > 2 && strcmp(argv[1], "-t") == 0) {
        tab = atoi(argv[2]);
    }
    char buf[4096];
    int col = 0;
    int n = read(0, buf, 4096);
    while (n > 0) {
        int i = 0;
        while (i < n) {
            if (buf[i] == '\t') {
                int spaces = tab - col % tab;
                int j = 0;
                while (j < spaces) {
                    cx_out(" ", 1);
                    j++;
                }
                col += spaces;
            } else if (buf[i] == '\n') {
                cx_out("\n", 1);
                col = 0;
            } else {
                cx_write(1, buf + i, 1);
                col++;
            }
            i++;
        }
        n = read(0, buf, 4096);
    }
    return rc;
}

static int cmd_paste(int argc, char ** argv) {
    int rc = 0;
    char delim = '\t';
    int i = 1;
    if (argc > 2 && strcmp(argv[1], "-d") == 0) {
        delim = argv[2][0];
        i = 3;
    }
    int nfds = 0;
    int fds[16];
    while (i < argc && nfds < 16 && !rc) {
        if (strcmp(argv[i], "-") == 0) {
            fds[nfds] = 0;
        } else {
            fds[nfds] = cx_openrd(argv[i]);
            if (fds[nfds] < 0) {
                rc = 1;
            }
        }
        nfds++;
        i++;
    }
    if (!rc && nfds > 0) {
        int any = 1;
        while (any) {
            any = 0;
            int j = 0;
            while (j < nfds) {
                char buf[4096];
                int n = cx_getline(fds[j], buf, 4096);
                if (j > 0) {
                    cx_write(1, &delim, 1);
                }
                if (n > 0) {
                    any = 1;
                    if (buf[n - 1] == '\n') {
                        n--;
                    }
                    cx_write(1, buf, n);
                }
                j++;
            }
            if (any) {
                cx_out("\n", 1);
            }
        }
    }
    int j = 0;
    while (j < nfds) {
        if (fds[j] > 0) {
            close(fds[j]);
        }
        j++;
    }
    return rc;
}

static int cmd_tr(int argc, char ** argv) {
    int rc = 0;
    if (argc < 3) {
        cx_err("tr: usage: tr SET1 SET2\n");
        rc = 1;
    }
    if (!rc) {
        char tab[256];
        int i = 0;
        while (i < 256) {
            tab[i] = i;
            i++;
        }
        char * s1 = argv[1];
        char * s2 = argv[2];
        int s2len = strlen(s2);
        int j = 0;
        while (s1[j]) {
            int from = s1[j] & 0xFF;
            int k = j < s2len ? j : s2len - 1;
            tab[from] = s2[k];
            j++;
        }
        char buf[4096];
        int n = read(0, buf, 4096);
        while (n > 0) {
            int p = 0;
            while (p < n) {
                buf[p] = tab[buf[p] & 0xFF];
                p++;
            }
            cx_write(1, buf, n);
            n = read(0, buf, 4096);
        }
    }
    return rc;
}

static int cmd_cut(int argc, char ** argv) {
    int rc = 0;
    char delim = '\t';
    int field = 1;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (argv[i][1] == 'd') {
            if (argv[i][2]) {
                delim = argv[i][2];
                i++;
            } else if (i + 1 < argc) {
                delim = argv[i + 1][0];
                i += 2;
            } else {
                i++;
            }
        } else if (argv[i][1] == 'f') {
            if (argv[i][2]) {
                field = atoi(argv[i] + 2);
                i++;
            } else if (i + 1 < argc) {
                field = atoi(argv[i + 1]);
                i += 2;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
    if (field < 1) {
        cx_err("cut: bad field\n");
        rc = 1;
    }
    if (!rc) {
        char buf[4096];
        int n = cx_getline(0, buf, 4096);
        while (n > 0) {
            int len = n;
            if (len > 0 && buf[len - 1] == '\n') {
                len--;
            }
            int f = 1;
            int start = 0;
            int end = len;
            int p = 0;
            int done = 0;
            while (p < len && !done) {
                if (buf[p] == delim) {
                    if (f == field) {
                        end = p;
                        done = 1;
                    } else {
                        f++;
                        start = p + 1;
                    }
                }
                if (!done) {
                    p++;
                }
            }
            if (!done && f != field) {
                start = 0;
                end = 0;
            }
            cx_write(1, buf + start, end - start);
            cx_out("\n", 1);
            n = cx_getline(0, buf, 4096);
        }
    }
    return rc;
}

// ====================== regex engine ======================
// Adapted from tiny-regex-c (kokke, public domain).
// Supports: . ^ $ * + ? [abc] [^abc] [a-z] \s \S \w \W \d \D

#define RE_MAX 30
#define RE_CCL 40

enum {
    RE_UNUSED, RE_DOT, RE_BEGIN, RE_END, RE_QUEST, RE_STAR,
    RE_PLUS, RE_CHAR, RE_CLASS, RE_NCLASS, RE_DIGIT, RE_NDIGIT,
    RE_ALPHA, RE_NALPHA, RE_SPACE, RE_NSPACE
};

// compiled regex: parallel arrays instead of struct-with-union
int re_type[RE_MAX];
int re_ch[RE_MAX];     // for RE_CHAR: the character
char re_ccl_buf[RE_CCL];  // buffer for character class strings

static int re_matchpat(int pi, char * text, int * mlen);

static int re_compile(char * pat) {
    int ci = 1; // ccl_buf index (0 reserved)
    int j = 0;  // compiled pattern index
    int i = 0;  // source pattern index
    while (pat[i] && j + 1 < RE_MAX) {
        int c = pat[i];
        if (c == '^') { re_type[j] = RE_BEGIN; }
        else if (c == '$') { re_type[j] = RE_END; }
        else if (c == '.') { re_type[j] = RE_DOT; }
        else if (c == '*') { re_type[j] = RE_STAR; }
        else if (c == '+') { re_type[j] = RE_PLUS; }
        else if (c == '?') { re_type[j] = RE_QUEST; }
        else if (c == '\\' && pat[i + 1]) {
            i++;
            if (pat[i] == 'd') { re_type[j] = RE_DIGIT; }
            else if (pat[i] == 'D') { re_type[j] = RE_NDIGIT; }
            else if (pat[i] == 'w') { re_type[j] = RE_ALPHA; }
            else if (pat[i] == 'W') { re_type[j] = RE_NALPHA; }
            else if (pat[i] == 's') { re_type[j] = RE_SPACE; }
            else if (pat[i] == 'S') { re_type[j] = RE_NSPACE; }
            else { re_type[j] = RE_CHAR; re_ch[j] = pat[i]; }
        }
        else if (c == '[') {
            int buf_begin = ci;
            if (pat[i + 1] == '^') {
                re_type[j] = RE_NCLASS;
                i++;
                if (!pat[i + 1]) { return 0; }
            } else {
                re_type[j] = RE_CLASS;
            }
            while (pat[++i] && pat[i] != ']') {
                if (pat[i] == '\\' && pat[i + 1]) {
                    if (ci >= RE_CCL - 1) { return 0; }
                    re_ccl_buf[ci] = pat[i]; ci++;
                    i++;
                } else if (ci >= RE_CCL) {
                    return 0;
                }
                re_ccl_buf[ci] = pat[i]; ci++;
            }
            if (ci >= RE_CCL) { return 0; }
            re_ccl_buf[ci] = 0; ci++;
            re_ch[j] = buf_begin; // index into re_ccl_buf
        }
        else { re_type[j] = RE_CHAR; re_ch[j] = c; }
        if (!pat[i]) { return 0; } // unterminated pattern
        i++;
        j++;
    }
    re_type[j] = RE_UNUSED;
    return 1;
}

static int re_matchone(int pi, int c) {
    int t = re_type[pi];
    if (t == RE_DOT) { return 1; }
    if (t == RE_DIGIT) { return isdigit(c); }
    if (t == RE_NDIGIT) { return !isdigit(c); }
    if (t == RE_ALPHA) { return c == '_' || isalpha(c) || isdigit(c); }
    if (t == RE_NALPHA) { return !(c == '_' || isalpha(c) || isdigit(c)); }
    if (t == RE_SPACE) { return isspace(c); }
    if (t == RE_NSPACE) { return !isspace(c); }
    if (t == RE_CHAR) { return re_ch[pi] == c; }
    if (t == RE_CLASS || t == RE_NCLASS) {
        char * s = re_ccl_buf + re_ch[pi];
        int found = 0;
        while (*s) {
            // range: a-z
            if (s[0] != '-' && s[1] == '-' && s[2]) {
                if (c >= s[0] && c <= s[2]) { found = 1; }
                s = s + 3;
            } else if (s[0] == '\\' && s[1]) {
                int mc = s[1];
                if (mc == 'd' && isdigit(c)) { found = 1; }
                if (mc == 'D' && !isdigit(c)) { found = 1; }
                if (mc == 'w' && (c == '_' || isalpha(c) || isdigit(c))) { found = 1; }
                if (mc == 'W' && !(c == '_' || isalpha(c) || isdigit(c))) { found = 1; }
                if (mc == 's' && isspace(c)) { found = 1; }
                if (mc == 'S' && !isspace(c)) { found = 1; }
                if (mc != 'd' && mc != 'D' && mc != 'w' && mc != 'W' &&
                    mc != 's' && mc != 'S' && mc == c) { found = 1; }
                s = s + 2;
            } else {
                if (*s == c) { found = 1; }
                s++;
            }
        }
        return t == RE_CLASS ? found : !found;
    }
    return 0;
}

static int re_matchstar(int pi, int ni, char * text, int * mlen) {
    int prelen = *mlen;
    char * pre = text;
    while (*text && re_matchone(pi, *text)) { text++; (*mlen)++; }
    while (text >= pre) {
        if (re_matchpat(ni, text, mlen)) { return 1; }
        text--;
        (*mlen)--;
    }
    *mlen = prelen;
    return 0;
}

static int re_matchplus(int pi, int ni, char * text, int * mlen) {
    char * pre = text;
    while (*text && re_matchone(pi, *text)) { text++; (*mlen)++; }
    while (text > pre) {
        if (re_matchpat(ni, text, mlen)) { return 1; }
        text--;
        (*mlen)--;
    }
    return 0;
}

static int re_matchquest(int pi, int ni, char * text, int * mlen) {
    if (re_type[pi] == RE_UNUSED) { return 1; }
    if (re_matchpat(ni, text, mlen)) { return 1; }
    if (*text && re_matchone(pi, *text)) {
        (*mlen)++;
        if (re_matchpat(ni, text + 1, mlen)) { return 1; }
        (*mlen)--;
    }
    return 0;
}

static int re_matchpat(int pi, char * text, int * mlen) {
    int pre = *mlen;
    while (1) {
        if (re_type[pi] == RE_UNUSED || re_type[pi + 1] == RE_QUEST) {
            return re_matchquest(pi, pi + 2, text, mlen);
        }
        if (re_type[pi + 1] == RE_STAR) {
            return re_matchstar(pi, pi + 2, text, mlen);
        }
        if (re_type[pi + 1] == RE_PLUS) {
            return re_matchplus(pi, pi + 2, text, mlen);
        }
        if (re_type[pi] == RE_END && re_type[pi + 1] == RE_UNUSED) {
            return *text == 0;
        }
        if (*text && re_matchone(pi, *text)) {
            pi++;
            text++;
            (*mlen)++;
        } else {
            *mlen = pre;
            return 0;
        }
    }
}

// Returns index of match in text, or -1. Sets *matchlen.
static int re_match(char * text, int * matchlen) {
    *matchlen = 0;
    if (re_type[0] == RE_BEGIN) {
        return re_matchpat(1, text, matchlen) ? 0 : -1;
    }
    int idx = 0;
    while (1) {
        *matchlen = 0;
        if (re_matchpat(0, text, matchlen)) {
            if (*text == 0) { return -1; }
            return idx;
        }
        if (*text == 0) { break; }
        text++;
        idx++;
    }
    return -1;
}

enum { G_INVERT = 1, G_COUNT = 2, G_NUMBER = 4, G_ICASE = 8 };

static int grep_parse_flags(char * arg) {
    int flags = 0;
    char * f = arg + 1;
    while (*f) {
        if (*f == 'v') {
            flags |= G_INVERT;
        }
        if (*f == 'c') {
            flags |= G_COUNT;
        }
        if (*f == 'n') {
            flags |= G_NUMBER;
        }
        if (*f == 'i') {
            flags |= G_ICASE;
        }
        f++;
    }
    return flags;
}

static int cmd_grep(int argc, char ** argv) {
    int rc = 0;
    int flags = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        flags = flags | grep_parse_flags(argv[i]);
        i++;
    }
    if (i >= argc) {
        cx_err("grep: missing pattern\n");
        rc = 1;
    }
    if (!rc) {
        char * pat = argv[i];
        char lpat[256];
        int j = 0;
        while (pat[j] && j < 255) {
            lpat[j] = (flags & G_ICASE) ? tolower(pat[j]) : pat[j];
            j++;
        }
        lpat[j] = 0;
        if (!re_compile(lpat)) {
            cx_err("grep: bad pattern\n");
            return 1;
        }
        char buf[4096];
        char lbuf[4096];
        int line_no = 0;
        int matches = 0;
        int mlen = 0;
        int n = cx_getline(0, buf, 4096);
        while (n > 0) {
            line_no++;
            // strip trailing newline for regex matching
            int slen = n;
            if (slen > 0 && buf[slen - 1] == '\n') { slen--; }
            char saved = buf[slen];
            buf[slen] = 0;
            char * search = buf;
            if (flags & G_ICASE) {
                int k = 0;
                while (k < slen) {
                    lbuf[k] = tolower(buf[k]);
                    k++;
                }
                lbuf[slen] = 0;
                search = lbuf;
            }
            int found = re_match(search, &mlen) >= 0;
            buf[slen] = saved;
            int show = (flags & G_INVERT) ? !found : found;
            if (show) {
                matches++;
                if (!(flags & G_COUNT)) {
                    if (flags & G_NUMBER) {
                        cx_putint(1, line_no);
                        cx_out(":", 1);
                    }
                    cx_write(1, buf, n);
                }
            }
            n = cx_getline(0, buf, 4096);
        }
        if (flags & G_COUNT) {
            cx_putint(1, matches);
            cx_out("\n", 1);
        }
        if (matches == 0) {
            rc = 1;
        }
    }
    return rc;
}

// sed command storage (parallel arrays, up to 16 commands)
#define SED_MAX 16
enum { SA_NONE, SA_LINE, SA_LAST, SA_RE };
int sed_a1t[SED_MAX]; // addr1 type
int sed_a1n[SED_MAX]; // addr1 line number
int sed_a2t[SED_MAX]; // addr2 type
int sed_a2n[SED_MAX]; // addr2 line number
int sed_cmd[SED_MAX]; // command char: 's' 'd' 'p' 'q'
int sed_sg[SED_MAX];  // s global flag
int sed_in_range[SED_MAX]; // range tracking
char sed_spat[4096];  // s patterns (16 x 256)
char sed_srep[4096];  // s replacements (16 x 256)
char sed_apat[8192];  // address regex patterns (16 x 256 x 2: a1 + a2)
int sed_nc;           // command count

static int sed_parse_addr(char * expr, int * pos, int ci, int which) {
    int * at = (which == 1) ? sed_a1t : sed_a2t;
    int * an = (which == 1) ? sed_a1n : sed_a2n;
    int p = *pos;
    if (expr[p] >= '1' && expr[p] <= '9') {
        int n = 0;
        while (expr[p] >= '0' && expr[p] <= '9') {
            n = n * 10 + expr[p] - '0';
            p++;
        }
        at[ci] = SA_LINE;
        an[ci] = n;
    } else if (expr[p] == '$') {
        at[ci] = SA_LAST;
        p++;
    } else if (expr[p] == '/') {
        p++;
        int off = ci * 256 + (which - 1) * 256 * SED_MAX;
        int qi = 0;
        while (expr[p] && expr[p] != '/' && qi < 255) {
            sed_apat[off + qi] = expr[p];
            qi++;
            p++;
        }
        sed_apat[off + qi] = 0;
        if (expr[p] == '/') { p++; }
        at[ci] = SA_RE;
    } else {
        at[ci] = SA_NONE;
        *pos = p;
        return 0;
    }
    *pos = p;
    return 1;
}

static int sed_parse_one(char * expr) {
    if (sed_nc >= SED_MAX) { return -1; }
    int ci = sed_nc;
    sed_a1t[ci] = SA_NONE;
    sed_a2t[ci] = SA_NONE;
    sed_in_range[ci] = 0;
    int p = 0;
    // parse optional address(es)
    if (sed_parse_addr(expr, &p, ci, 1)) {
        if (expr[p] == ',') {
            p++;
            sed_parse_addr(expr, &p, ci, 2);
        }
    }
    // parse command
    if (expr[p] == 's' && expr[p + 1] == '/') {
        sed_cmd[ci] = 's';
        p += 2;
        int qi = 0;
        char * sp = sed_spat + ci * 256;
        while (expr[p] && expr[p] != '/' && qi < 255) {
            sp[qi] = expr[p]; qi++; p++;
        }
        sp[qi] = 0;
        if (expr[p] == '/') { p++; }
        qi = 0;
        char * rp = sed_srep + ci * 256;
        while (expr[p] && expr[p] != '/' && qi < 255) {
            rp[qi] = expr[p]; qi++; p++;
        }
        rp[qi] = 0;
        sed_sg[ci] = 0;
        if (expr[p] == '/') {
            p++;
            if (expr[p] == 'g') { sed_sg[ci] = 1; }
        }
    } else if (expr[p] == 'd') {
        sed_cmd[ci] = 'd';
    } else if (expr[p] == 'p') {
        sed_cmd[ci] = 'p';
    } else if (expr[p] == 'q') {
        sed_cmd[ci] = 'q';
    } else {
        return -1;
    }
    sed_nc++;
    return 0;
}

static int sed_addr_match(int ci, int which, char * line, int lno,
                          int is_last) {
    int t = (which == 1) ? sed_a1t[ci] : sed_a2t[ci];
    int n = (which == 1) ? sed_a1n[ci] : sed_a2n[ci];
    if (t == SA_NONE) { return 1; }
    if (t == SA_LINE) { return lno == n; }
    if (t == SA_LAST) { return is_last; }
    if (t == SA_RE) {
        int off = ci * 256 + (which - 1) * 256 * SED_MAX;
        re_compile(sed_apat + off);
        int ml = 0;
        return re_match(line, &ml) >= 0;
    }
    return 0;
}

static int sed_in_addr(int ci, char * line, int lno, int is_last) {
    if (sed_a1t[ci] == SA_NONE && sed_a2t[ci] == SA_NONE) {
        return 1; // no address = all lines
    }
    if (sed_a2t[ci] == SA_NONE) {
        return sed_addr_match(ci, 1, line, lno, is_last);
    }
    // range: addr1,addr2
    if (!sed_in_range[ci]) {
        if (sed_addr_match(ci, 1, line, lno, is_last)) {
            sed_in_range[ci] = 1;
            return 1;
        }
        return 0;
    } else {
        if (sed_addr_match(ci, 2, line, lno, is_last)) {
            sed_in_range[ci] = 0;
        }
        return 1;
    }
}

static void sed_do_sub(int ci, char * line, int len, char * out,
                       int * olen) {
    char * spat = sed_spat + ci * 256;
    char * srep = sed_srep + ci * 256;
    int rlen = strlen(srep);
    re_compile(spat);
    int oi = 0;
    int p = 0;
    int done_one = 0;
    while (p < len) {
        int can = sed_sg[ci] || !done_one;
        if (can) {
            int ml = 0;
            int mi = re_match(line + p, &ml);
            if (mi >= 0) {
                memcpy(out + oi, line + p, mi);
                oi = oi + mi;
                memcpy(out + oi, srep, rlen);
                oi = oi + rlen;
                p = p + mi + (ml > 0 ? ml : 1);
                done_one = 1;
            } else {
                out[oi] = line[p]; oi++; p++;
            }
        } else {
            out[oi] = line[p]; oi++; p++;
        }
    }
    *olen = oi;
}

static int cmd_sed(int argc, char ** argv) {
    int suppress = 0;
    sed_nc = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        if (strcmp(argv[i], "-n") == 0) { suppress = 1; }
        if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            i++;
            if (sed_parse_one(argv[i]) < 0) {
                cx_err("sed: bad expression\n");
                return 1;
            }
        }
        i++;
    }
    // non-flag arg is an expression if no -e was given
    if (sed_nc == 0 && i < argc) {
        if (sed_parse_one(argv[i]) < 0) {
            cx_err("sed: bad expression\n");
            return 1;
        }
        i++;
    }
    if (sed_nc == 0) {
        cx_err("sed: missing expression\n");
        return 1;
    }
    // read all lines to detect last line for '$' address
    int sed_cap = 4096;
    int sed_lw = 4096; // line width
    char * lines = (char*)malloc(sed_cap * sed_lw);
    int * lens = (int*)malloc(sed_cap * sizeof(int));
    int total = 0;
    char buf[4096];
    int n = cx_getline(0, buf, 4096);
    while (n > 0 && total < sed_cap) {
        int blen = n;
        if (blen > 0 && buf[blen - 1] == '\n') { blen--; }
        if (blen >= sed_lw) { blen = sed_lw - 1; }
        memcpy(lines + total * sed_lw, buf, blen);
        lens[total] = blen;
        total++;
        n = cx_getline(0, buf, 4096);
    }
    // process each line
    int li = 0;
    while (li < total) {
        char * line = lines + li * sed_lw;
        int llen = lens[li];
        char saved = line[llen];
        line[llen] = 0; // null-terminate for regex
        int lno = li + 1;
        int is_last = (li == total - 1);
        int deleted = 0;
        int quit = 0;
        char out[8192];
        int oi = llen;
        memcpy(out, line, llen);
        int ci = 0;
        while (ci < sed_nc && !deleted && !quit) {
            if (sed_in_addr(ci, line, lno, is_last)) {
                if (sed_cmd[ci] == 'd') {
                    deleted = 1;
                } else if (sed_cmd[ci] == 'p') {
                    cx_write(1, out, oi);
                    cx_out("\n", 1);
                } else if (sed_cmd[ci] == 'q') {
                    if (!suppress) {
                        cx_write(1, out, oi);
                        cx_out("\n", 1);
                    }
                    quit = 1;
                } else if (sed_cmd[ci] == 's') {
                    int new_oi = 0;
                    char tmp[8192];
                    out[oi] = 0;
                    sed_do_sub(ci, out, oi, tmp, &new_oi);
                    memcpy(out, tmp, new_oi);
                    oi = new_oi;
                }
            }
            ci++;
        }
        line[llen] = saved;
        if (!deleted && !quit && !suppress) {
            cx_write(1, out, oi);
            cx_out("\n", 1);
        }
        if (quit) { break; }
        li++;
    }
    free(lines);
    free(lens);
    return 0;
}

static void uniq_emit(char * line, int len, int count, int sc) {
    if (count > 0) {
        if (sc) {
            char num[16];
            int nlen = cx_itopad(count, num, 4);
            cx_write(1, num, nlen);
            cx_out(" ", 1);
        }
        cx_write(1, line, len);
        cx_out("\n", 1);
    }
}

static int cmd_uniq(int argc, char ** argv) {
    int rc = 0;
    int show_count = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (argv[i][1] == 'c') {
            show_count = 1;
        }
        i++;
    }
    char prev[4096];
    char curr[4096];
    int prev_len = 0;
    int count = 0;
    int n = cx_getline(0, curr, 4096);
    while (n > 0) {
        int curr_len = n;
        if (curr_len > 0 && curr[curr_len - 1] == '\n') {
            curr_len--;
        }
        int same = prev_len == curr_len &&
                   memcmp(prev, curr, curr_len) == 0;
        if (same) {
            count++;
        } else {
            uniq_emit(prev, prev_len, count, show_count);
            memcpy(prev, curr, curr_len);
            prev_len = curr_len;
            count = 1;
        }
        n = cx_getline(0, curr, 4096);
    }
    uniq_emit(prev, prev_len, count, show_count);
    return rc;
}

static int cmd_sort(int argc, char ** argv) {
    int rc = 0;
    int reverse = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (argv[i][1] == 'r') {
            reverse = 1;
        }
        i++;
    }
    struct lines l;
    lines_init(&l);
    if (i >= argc) {
        lines_read(&l, 0);
    } else {
        while (i < argc && !rc) {
            int fd = cx_openrd(argv[i]);
            if (fd < 0) {
                rc = 1;
            } else {
                lines_read(&l, fd);
                close(fd);
            }
            i++;
        }
    }
    if (!rc) {
        int x = 1;
        while (x < l.count) {
            char * key = l.data[x];
            int klen = l.lens[x];
            int y = x - 1;
            int done = 0;
            while (y >= 0 && !done) {
                int cmp = strcmp(l.data[y], key);
                if (reverse) {
                    cmp = -cmp;
                }
                if (cmp > 0) {
                    l.data[y + 1] = l.data[y];
                    l.lens[y + 1] = l.lens[y];
                    y--;
                } else {
                    done = 1;
                }
            }
            l.data[y + 1] = key;
            l.lens[y + 1] = klen;
            x++;
        }
        int z = 0;
        while (z < l.count) {
            cx_write(1, l.data[z], l.lens[z]);
            z++;
        }
    }
    lines_free(&l);
    return rc;
}

static int cmd_printf(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cx_err("printf: missing format\n");
        rc = 1;
    }
    if (!rc) {
        char * fmt = argv[1];
        int ai = 2;
        int p = 0;
        while (fmt[p]) {
            if (fmt[p] == '\\') {
                p++;
                if (fmt[p] == 'n') {
                    cx_out("\n", 1);
                } else if (fmt[p] == 't') {
                    cx_out("\t", 1);
                } else if (fmt[p] == '\\') {
                    cx_out("\\", 1);
                } else {
                    cx_write(1, fmt + p, 1);
                }
                p++;
            } else if (fmt[p] == '%') {
                p++;
                if (fmt[p] == 's') {
                    if (ai < argc) {
                        cx_puts(argv[ai]);
                        ai++;
                    }
                    p++;
                } else if (fmt[p] == 'd') {
                    if (ai < argc) {
                        cx_putint(1, atoi(argv[ai]));
                        ai++;
                    }
                    p++;
                } else if (fmt[p] == 'c') {
                    if (ai < argc) {
                        cx_write(1, argv[ai], 1);
                        ai++;
                    }
                    p++;
                } else if (fmt[p] == '%') {
                    cx_out("%", 1);
                    p++;
                }
            } else {
                cx_write(1, fmt + p, 1);
                p++;
            }
        }
    }
    return rc;
}

struct cx_stat {
    int mode;
    int size;
    int mtime;
    int nlink;
    int uid;
};

static void cx_writes(int fd, char * s) {
    cx_write(fd, s, strlen(s));
}

static int dir_exists(char * path) {
    int yes = 0;
    struct cx_stat st;
    if (stat(path, (void*)&st) == 0) {
        if (S_ISDIR(st.mode)) {
            yes = 1;
        }
    }
    return yes;
}

static int mkdir_p(char * path) {
    int rc = 0;
    char buf[4096];
    int len = strlen(path);
    if (len >= 4096) {
        rc = -1;
    }
    if (!rc) {
        memcpy(buf, path, len + 1);
        int i = 1;
        while (i <= len && !rc) {
            if (buf[i] == '/' || buf[i] == 0) {
                char save = buf[i];
                buf[i] = 0;
                if (!dir_exists(buf)) {
                    if (mkdir(buf, 493) != 0) {
                        rc = -1;
                    }
                }
                buf[i] = save;
            }
            i++;
        }
    }
    return rc;
}

static int copy_file(char * src, char * dst) {
    int rc = 0;
    int sfd = open(src, 0);
    if (sfd < 0) {
        rc = -1;
    }
    if (!rc) {
        int dfd = open(dst, 1 | O_CREAT | O_TRUNC, 420);
        if (dfd < 0) {
            close(sfd);
            rc = -1;
        }
        if (!rc) {
            char buf[4096];
            int n = read(sfd, buf, 4096);
            while (n > 0) {
                cx_write(dfd, buf, n);
                n = read(sfd, buf, 4096);
            }
            close(dfd);
            close(sfd);
        }
    }
    return rc;
}

static int rm_recursive(char * path) {
    int rc = 0;
    struct cx_stat st;
    if (stat(path, (void*)&st) != 0) {
        rc = -1;
    }
    if (!rc && S_ISDIR(st.mode)) {
        char * names = (char*)malloc(64 * 256);
        int ncount = 0;
        void * dp = opendir(path);
        if (dp == 0) {
            rc = -1;
        }
        if (!rc) {
            char * name = (char*)readdir(dp);
            while (name && ncount < 64) {
                if (strcmp(name, ".") != 0 &&
                    strcmp(name, "..") != 0) {
                    int nl = strlen(name);
                    if (nl < 256) {
                        memcpy(names + ncount * 256, name, nl + 1);
                        ncount++;
                    }
                }
                name = (char*)readdir(dp);
            }
            closedir(dp);
            int i = 0;
            while (i < ncount && !rc) {
                char child[4096];
                int plen = strlen(path);
                int nlen = strlen(names + i * 256);
                if (plen + 1 + nlen < 4096) {
                    memcpy(child, path, plen);
                    child[plen] = '/';
                    memcpy(child + plen + 1,
                           names + i * 256, nlen + 1);
                    if (rm_recursive(child) != 0) {
                        rc = -1;
                    }
                }
                i++;
            }
        }
        free(names);
        if (!rc) {
            if (rmdir(path) != 0) {
                rc = -1;
            }
        }
    }
    if (!rc && (st.mode & S_IFMT) != S_IFDIR) {
        if (unlink(path) != 0) {
            rc = -1;
        }
    }
    return rc;
}

static int cmd_pwd(int argc, char ** argv) {
    int rc = 0;
    char buf[4096];
    char * p = getcwd(buf, 4096);
    if (p) {
        cx_puts(buf);
        cx_out("\n", 1);
    } else {
        cx_err("pwd: cannot get cwd\n");
        rc = 1;
    }
    return rc;
}

static int cmd_touch(int argc, char ** argv) {
    int rc = 0;
    int i = 1;
    while (i < argc && !rc) {
        int fd = open(argv[i], 1 | O_CREAT, 420);
        if (fd < 0) {
            cx_err("touch: cannot create: ");
            cx_err(argv[i]);
            cx_err("\n");
            rc = 1;
        } else {
            close(fd);
        }
        i++;
    }
    return rc;
}

static int cmd_mkdir(int argc, char ** argv) {
    int rc = 0;
    int parents = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (argv[i][1] == 'p') {
            parents = 1;
        }
        i++;
    }
    while (i < argc && !rc) {
        int r;
        if (parents) {
            r = mkdir_p(argv[i]);
        } else {
            r = mkdir(argv[i], 493);
        }
        if (r != 0) {
            cx_err("mkdir: cannot create: ");
            cx_err(argv[i]);
            cx_err("\n");
            rc = 1;
        }
        i++;
    }
    return rc;
}

static int cmd_rmdir(int argc, char ** argv) {
    int rc = 0;
    int i = 1;
    while (i < argc && !rc) {
        if (rmdir(argv[i]) != 0) {
            cx_err("rmdir: cannot remove: ");
            cx_err(argv[i]);
            cx_err("\n");
            rc = 1;
        }
        i++;
    }
    return rc;
}

static int cmd_rm(int argc, char ** argv) {
    int rc = 0;
    int recursive = 0;
    int force = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        char * f = argv[i] + 1;
        while (*f) {
            if (*f == 'r' || *f == 'R') {
                recursive = 1;
            }
            if (*f == 'f') {
                force = 1;
            }
            f++;
        }
        i++;
    }
    while (i < argc && !rc) {
        int r;
        if (recursive) {
            r = rm_recursive(argv[i]);
        } else {
            r = unlink(argv[i]);
        }
        if (r != 0 && !force) {
            cx_err("rm: cannot remove: ");
            cx_err(argv[i]);
            cx_err("\n");
            rc = 1;
        }
        i++;
    }
    return rc;
}

static int cmd_cp(int argc, char ** argv) {
    int rc = 0;
    if (argc < 3) {
        cx_err("cp: usage: cp SRC DST\n");
        rc = 1;
    }
    if (!rc) {
        if (copy_file(argv[1], argv[2]) != 0) {
            cx_err("cp: failed: ");
            cx_err(argv[1]);
            cx_err(" -> ");
            cx_err(argv[2]);
            cx_err("\n");
            rc = 1;
        }
    }
    return rc;
}

static int cmd_mv(int argc, char ** argv) {
    int rc = 0;
    if (argc < 3) {
        cx_err("mv: usage: mv SRC DST\n");
        rc = 1;
    }
    if (!rc) {
        if (rename(argv[1], argv[2]) != 0) {
            cx_err("mv: failed: ");
            cx_err(argv[1]);
            cx_err(" -> ");
            cx_err(argv[2]);
            cx_err("\n");
            rc = 1;
        }
    }
    return rc;
}

static int cmd_ln(int argc, char ** argv) {
    int rc = 0;
    int symbolic = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (argv[i][1] == 's') {
            symbolic = 1;
        }
        i++;
    }
    if (argc - i < 2) {
        cx_err("ln: need target and link name\n");
        rc = 1;
    }
    if (!rc) {
        char * target = argv[i];
        char * linkname = argv[i + 1];
        int r;
        if (symbolic) {
            r = symlink(target, linkname);
        } else {
            r = link(target, linkname);
        }
        if (r != 0) {
            cx_err("ln: failed\n");
            rc = 1;
        }
    }
    return rc;
}

static int cmd_chmod(int argc, char ** argv) {
    int rc = 0;
    if (argc < 3) {
        cx_err("chmod: usage: chmod MODE FILE\n");
        rc = 1;
    }
    if (!rc) {
        char * ms = argv[1];
        int symbolic = 0;
        int mode = 0;
        // detect symbolic mode: [ugoa][+-=][rwx...]
        if ((*ms >= 'a' && *ms <= 'z') || *ms == '+' || *ms == '-') {
            symbolic = 1;
        }
        if (!symbolic) {
            char * s = ms;
            while (*s >= '0' && *s <= '7') {
                mode = mode * 8 + (*s - '0');
                s++;
            }
        }
        int j = 2;
        while (j < argc && !rc) {
            if (symbolic) {
                struct cx_stat st;
                if (stat(argv[j], (void*)&st) != 0) {
                    cx_err("chmod: cannot stat: ");
                    cx_err(argv[j]);
                    cx_err("\n");
                    rc = 1;
                } else {
                    mode = st.mode & 4095; // keep low 12 bits
                    char * p = ms;
                    while (*p) {
                        // parse who: u g o a (default = a)
                        int umask = 0;
                        int gmask = 0;
                        int omask = 0;
                        int who = 0;
                        while (*p == 'u' || *p == 'g' || *p == 'o' ||
                               *p == 'a') {
                            if (*p == 'u') { umask = 1; who = 1; }
                            if (*p == 'g') { gmask = 1; who = 1; }
                            if (*p == 'o') { omask = 1; who = 1; }
                            if (*p == 'a') {
                                umask = 1; gmask = 1; omask = 1; who = 1;
                            }
                            p++;
                        }
                        if (!who) { umask = 1; gmask = 1; omask = 1; }
                        // parse op: + - =
                        int op = 0;
                        if (*p == '+' || *p == '-' || *p == '=') {
                            op = *p; p++;
                        }
                        // parse perms: r w x
                        int bits = 0;
                        while (*p == 'r' || *p == 'w' || *p == 'x') {
                            if (*p == 'r') { bits = bits | 4; }
                            if (*p == 'w') { bits = bits | 2; }
                            if (*p == 'x') { bits = bits | 1; }
                            p++;
                        }
                        // apply
                        int mask = 0;
                        if (umask) { mask = mask | (bits << 6); }
                        if (gmask) { mask = mask | (bits << 3); }
                        if (omask) { mask = mask | bits; }
                        if (op == '+') { mode = mode | mask; }
                        if (op == '-') { mode = mode & ~mask; }
                        if (op == '=') {
                            int clear = 0;
                            if (umask) { clear = clear | (7 << 6); }
                            if (gmask) { clear = clear | (7 << 3); }
                            if (omask) { clear = clear | 7; }
                            mode = (mode & ~clear) | mask;
                        }
                        if (*p == ',') { p++; }
                    }
                    if (chmod(argv[j], mode) != 0) {
                        cx_err("chmod: failed: ");
                        cx_err(argv[j]);
                        cx_err("\n");
                        rc = 1;
                    }
                }
            } else {
                if (chmod(argv[j], mode) != 0) {
                    cx_err("chmod: failed: ");
                    cx_err(argv[j]);
                    cx_err("\n");
                    rc = 1;
                }
            }
            j++;
        }
    }
    return rc;
}

static int cmd_cd(int argc, char ** argv) {
    int rc = 0;
    char * target = "/";
    if (argc >= 2) {
        target = argv[1];
    }
    if (chdir(target) != 0) {
        cx_err("cd: cannot chdir: ");
        cx_err(target);
        cx_err("\n");
        rc = 1;
    }
    return rc;
}

static int cmd_env(int argc, char ** argv) {
    int rc = 0;
    char * known[8];
    known[0] = "PATH";
    known[1] = "HOME";
    known[2] = "USER";
    known[3] = "SHELL";
    known[4] = "TERM";
    known[5] = "PWD";
    known[6] = "LANG";
    known[7] = 0;
    int i = 0;
    while (known[i]) {
        char * v = getenv(known[i]);
        if (v) {
            cx_puts(known[i]);
            cx_out("=", 1);
            cx_puts(v);
            cx_out("\n", 1);
        }
        i++;
    }
    return rc;
}

static int cmd_install(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cx_err("install: usage: install DIR\n");
        rc = 1;
    }
    char cwd[4096];
    if (!rc) {
        if (mkdir_p(argv[1]) != 0) {
            cx_err("install: cannot create target\n");
            rc = 1;
        }
    }
    if (!rc) {
        if (getcwd(cwd, 4096) == 0) {
            cx_err("install: cannot get cwd\n");
            rc = 1;
        }
    }
    if (!rc) {
        int i = 0;
        while (i < ncmds && !rc) {
            char path[4096];
            int tl = strlen(argv[1]);
            int nl = strlen(cmds[i].name);
            memcpy(path, argv[1], tl);
            if (tl > 0 && path[tl - 1] != '/') {
                path[tl] = '/';
                tl++;
            }
            memcpy(path + tl, cmds[i].name, nl + 1);
            int fd = open(path, 1 | O_CREAT | O_TRUNC, 493);
            if (fd < 0) {
                cx_err("install: cannot write: ");
                cx_err(path);
                cx_err("\n");
                rc = 1;
            } else {
                cx_writes(fd, "#!/bin/sh\nexec ");
                cx_writes(fd, cwd);
                cx_writes(fd, "/build/cx ");
                cx_writes(fd, cwd);
                cx_writes(fd, "/toys.c ");
                cx_writes(fd, cmds[i].name);
                cx_writes(fd, " \"$@\"\n");
                close(fd);
            }
            i++;
        }
    }
    return rc;
}

static void ls_perms(int mode, char * out) {
    out[0] = S_ISDIR(mode) ? 'd' : '-';
    out[1] = (mode & 256) ? 'r' : '-';
    out[2] = (mode & 128) ? 'w' : '-';
    out[3] = (mode & 64) ? 'x' : '-';
    out[4] = (mode & 32) ? 'r' : '-';
    out[5] = (mode & 16) ? 'w' : '-';
    out[6] = (mode & 8) ? 'x' : '-';
    out[7] = (mode & 4) ? 'r' : '-';
    out[8] = (mode & 2) ? 'w' : '-';
    out[9] = (mode & 1) ? 'x' : '-';
    out[10] = 0;
}

static void ls_human_size(int size, char * out) {
    if (size < 1024) {
        cx_itoa(size, out);
    } else if (size < 1024 * 1024) {
        int k = size / 1024;
        int frac = (size % 1024) * 10 / 1024;
        if (k < 10 && frac > 0) {
            int len = cx_itoa(k, out);
            out[len] = '.';
            out[len + 1] = '0' + frac;
            out[len + 2] = 'K';
            out[len + 3] = 0;
        } else {
            int len = cx_itoa(k, out);
            out[len] = 'K';
            out[len + 1] = 0;
        }
    } else {
        int m = size / (1024 * 1024);
        int frac = (size % (1024 * 1024)) * 10 / (1024 * 1024);
        if (m < 10 && frac > 0) {
            int len = cx_itoa(m, out);
            out[len] = '.';
            out[len + 1] = '0' + frac;
            out[len + 2] = 'M';
            out[len + 3] = 0;
        } else {
            int len = cx_itoa(m, out);
            out[len] = 'M';
            out[len + 1] = 0;
        }
    }
}

static int cmd_ls(int argc, char ** argv) {
    int rc = 0;
    int show_all = 0;
    int long_fmt = 0;
    int human = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        char * f = argv[i] + 1;
        while (*f) {
            if (*f == 'a') { show_all = 1; }
            if (*f == 'l') { long_fmt = 1; }
            if (*f == 'h') { human = 1; }
            f++;
        }
        i++;
    }
    // collect dir/file arguments
    int first_arg = i;
    if (i >= argc) { first_arg = 0; } // will use "." default
    // handle each argument — could be a file or directory
    int ai = first_arg;
    int arg_count = (first_arg == 0) ? 1 : argc - first_arg;
    int argi = 0;
    while (argi < arg_count && !rc) {
    char * dir;
    if (first_arg == 0) {
        dir = ".";
    } else {
        dir = argv[ai + argi];
    }
    // check if it's a file (not a directory)
    struct cx_stat fst;
    int is_file = 0;
    if (stat(dir, (void*)&fst) == 0) {
        if ((fst.mode & 0040000) == 0) { is_file = 1; }
    }
    if (is_file) {
        if (long_fmt) {
            char perms[12];
            ls_perms(fst.mode, perms);
            cx_puts(perms);
            cx_out("  ", 2);
            char nb[16];
            cx_itopad(fst.nlink, nb, 2);
            cx_puts(nb);
            cx_out(" ", 1);
            if (human) {
                char hb[16];
                ls_human_size(fst.size, hb);
                int hl = strlen(hb);
                int pad = 5 - hl;
                while (pad > 0) { cx_out(" ", 1); pad--; }
                cx_puts(hb);
            } else {
                cx_itopad(fst.size, nb, 8);
                cx_puts(nb);
            }
            cx_out(" ", 1);
        }
        cx_puts(dir);
        cx_out("\n", 1);
        argi++;
        continue;
    }
    if (arg_count > 1) {
        cx_puts(dir);
        cx_out(":\n", 2);
    }
    void * dp = opendir(dir);
    if (dp == 0) {
        cx_err("ls: cannot open: ");
        cx_err(dir);
        cx_err("\n");
        rc = 1;
    }
    if (!rc) {
        char * names = (char*)malloc(256 * 256);
        int ncount = 0;
        char * name = (char*)readdir(dp);
        while (name && ncount < 256) {
            if (show_all || name[0] != '.') {
                int nl = strlen(name);
                if (nl < 256) {
                    memcpy(names + ncount * 256, name, nl + 1);
                    ncount++;
                }
            }
            name = (char*)readdir(dp);
        }
        closedir(dp);
        int x = 1;
        while (x < ncount) {
            char key[256];
            memcpy(key, names + x * 256, 256);
            int y = x - 1;
            int done = 0;
            while (y >= 0 && !done) {
                if (strcmp(names + y * 256, key) > 0) {
                    memcpy(names + (y + 1) * 256,
                           names + y * 256, 256);
                    y--;
                } else {
                    done = 1;
                }
            }
            memcpy(names + (y + 1) * 256, key, 256);
            x++;
        }
        int j = 0;
        while (j < ncount) {
            char * n = names + j * 256;
            if (long_fmt) {
                char path[4096];
                int dl = strlen(dir);
                memcpy(path, dir, dl);
                if (dl > 0 && path[dl - 1] != '/') {
                    path[dl] = '/';
                    dl++;
                }
                int nl2 = strlen(n);
                memcpy(path + dl, n, nl2 + 1);
                struct cx_stat st;
                if (stat(path, (void*)&st) == 0) {
                    char perms[12];
                    ls_perms(st.mode, perms);
                    cx_puts(perms);
                    cx_out("  ", 2);
                    char nb[16];
                    cx_itopad(st.nlink, nb, 2);
                    cx_puts(nb);
                    cx_out(" ", 1);
                    if (human) {
                        char hb[16];
                        ls_human_size(st.size, hb);
                        int hl = strlen(hb);
                        // right-align to 5 chars
                        int pad = 5 - hl;
                        while (pad > 0) { cx_out(" ", 1); pad--; }
                        cx_puts(hb);
                    } else {
                        cx_itopad(st.size, nb, 8);
                        cx_puts(nb);
                    }
                    cx_out(" ", 1);
                }
            }
            cx_puts(n);
            cx_out("\n", 1);
            j++;
        }
        free(names);
    }
    argi++;
    } // end while argi
    return rc;
}

static int find_walk(char * path, char * name_pat, char tf) {
    int rc = 0;
    struct cx_stat st;
    if (stat(path, (void*)&st) != 0) {
        rc = -1;
    }
    if (!rc) {
        int show = 1;
        if (name_pat) {
            char * base = path;
            char * p = path;
            while (*p) {
                if (*p == '/') {
                    base = p + 1;
                }
                p++;
            }
            if (strcmp(base, name_pat) != 0) {
                show = 0;
            }
        }
        if (show && tf) {
            int is_dir = S_ISDIR(st.mode);
            int is_reg = S_ISREG(st.mode);
            if (tf == 'f' && !is_reg) {
                show = 0;
            }
            if (tf == 'd' && !is_dir) {
                show = 0;
            }
        }
        if (show) {
            cx_puts(path);
            cx_out("\n", 1);
        }
        if ((st.mode & S_IFMT) == S_IFDIR) {
            void * dp = opendir(path);
            if (dp != 0) {
                char * names = (char*)malloc(64 * 256);
                int n = 0;
                char * name = (char*)readdir(dp);
                while (name && n < 64) {
                    if (strcmp(name, ".") != 0 &&
                        strcmp(name, "..") != 0) {
                        int nl = strlen(name);
                        if (nl < 256) {
                            memcpy(names + n * 256, name, nl + 1);
                            n++;
                        }
                    }
                    name = (char*)readdir(dp);
                }
                closedir(dp);
                int i = 0;
                while (i < n) {
                    char child[4096];
                    int plen = strlen(path);
                    int nlen = strlen(names + i * 256);
                    if (plen + 1 + nlen < 4096) {
                        memcpy(child, path, plen);
                        child[plen] = '/';
                        memcpy(child + plen + 1,
                               names + i * 256, nlen + 1);
                        find_walk(child, name_pat, tf);
                    }
                    i++;
                }
                free(names);
            }
        }
    }
    return rc;
}

static int cmd_find(int argc, char ** argv) {
    int rc = 0;
    char * start = ".";
    char * name_pat = 0;
    char tf = 0;
    int i = 1;
    if (i < argc && argv[i][0] != '-') {
        start = argv[i];
        i++;
    }
    while (i < argc) {
        if (strcmp(argv[i], "-name") == 0 && i + 1 < argc) {
            name_pat = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-type") == 0 &&
                   i + 1 < argc) {
            tf = argv[i + 1][0];
            i += 2;
        } else {
            i++;
        }
    }
    find_walk(start, name_pat, tf);
    return rc;
}

static int cmd_xargs(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cx_err("xargs: usage: xargs CMD [ARGS...]\n");
        return 1;
    }
    struct sb sb_input;
    memset(&sb_input, 0, sizeof(struct sb));
    char buf[4096];
    int n;
    while ((n = read(0, buf, 4096)) > 0) {
        sb_put(&sb_input, buf, n);
    }
    struct sb sb_cmd;
    memset(&sb_cmd, 0, sizeof(struct sb));
    int j = 1;
    while (j < argc) {
        if (j > 1) { sb_putc(&sb_cmd, ' '); }
        sb_puts(&sb_cmd, argv[j]);
        j++;
    }
    if (sb_input.data) {
        char * p = sb_input.data;
        while (*p) {
            while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) { p++; }
            if (!*p) { break; }
            sb_putc(&sb_cmd, ' ');
            while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
                sb_putc(&sb_cmd, *p);
                p++;
            }
        }
    }
    int sysrc = system(sb_cmd.data ? sb_cmd.data : "");
    rc = sysrc / 256;
    sb_free(&sb_input);
    sb_free(&sb_cmd);
    return rc;
}

static int test_eval2(char * arg, char op) {
    int result = 1;
    if (op == 'e') {
        if (access(arg, F_OK) == 0) {
            result = 0;
        }
    } else if (op == 'f') {
        struct cx_stat st;
        if (stat(arg, (void*)&st) == 0 && S_ISREG(st.mode)) {
            result = 0;
        }
    } else if (op == 'd') {
        struct cx_stat st;
        if (stat(arg, (void*)&st) == 0 && S_ISDIR(st.mode)) {
            result = 0;
        }
    } else if (op == 'r') {
        if (access(arg, R_OK) == 0) {
            result = 0;
        }
    } else if (op == 'w') {
        if (access(arg, W_OK) == 0) {
            result = 0;
        }
    } else if (op == 'x') {
        if (access(arg, X_OK) == 0) {
            result = 0;
        }
    } else if (op == 'z') {
        if (strlen(arg) == 0) {
            result = 0;
        }
    } else if (op == 'n') {
        if (strlen(arg) > 0) {
            result = 0;
        }
    }
    return result;
}

static int test_eval3(char * a, char * op, char * b) {
    int result = 1;
    if (strcmp(op, "=") == 0) {
        if (strcmp(a, b) == 0) {
            result = 0;
        }
    } else if (strcmp(op, "!=") == 0) {
        if (strcmp(a, b) != 0) {
            result = 0;
        }
    } else if (strcmp(op, "-eq") == 0) {
        if (atoi(a) == atoi(b)) {
            result = 0;
        }
    } else if (strcmp(op, "-ne") == 0) {
        if (atoi(a) != atoi(b)) {
            result = 0;
        }
    } else if (strcmp(op, "-lt") == 0) {
        if (atoi(a) < atoi(b)) {
            result = 0;
        }
    } else if (strcmp(op, "-le") == 0) {
        if (atoi(a) <= atoi(b)) {
            result = 0;
        }
    } else if (strcmp(op, "-gt") == 0) {
        if (atoi(a) > atoi(b)) {
            result = 0;
        }
    } else if (strcmp(op, "-ge") == 0) {
        if (atoi(a) >= atoi(b)) {
            result = 0;
        }
    }
    return result;
}

static int cmd_test(int argc, char ** argv) {
    int rc = 1;
    int end = argc;
    int ok = 1;
    if (strcmp(argv[0], "[") == 0) {
        if (argc < 2 || strcmp(argv[argc - 1], "]") != 0) {
            cx_err("[: missing ]\n");
            ok = 0;
        } else {
            end = argc - 1;
        }
    }
    if (ok) {
        int n = end - 1;
        if (n == 1) {
            if (strlen(argv[1]) > 0) {
                rc = 0;
            }
        } else if (n == 2 && argv[1][0] == '-' &&
                   argv[1][2] == 0) {
            rc = test_eval2(argv[2], argv[1][1]);
        } else if (n == 3) {
            rc = test_eval3(argv[1], argv[2], argv[3]);
        } else if (n == 0) {
            rc = 1;
        }
    }
    return rc;
}

static int cmd_which(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cx_err("which: usage: which NAME\n");
        rc = 1;
    }
    if (!rc) {
        char * path = getenv("PATH");
        if (!path) {
            rc = 1;
        }
        if (!rc) {
            char * name = argv[1];
            int found = 0;
            int p = 0;
            int plen = strlen(path);
            while (p < plen && !found) {
                int e = p;
                while (e < plen && path[e] != ':') {
                    e++;
                }
                int dlen = e - p;
                if (dlen > 0) {
                    char full[4096];
                    int nlen = strlen(name);
                    if (dlen + 1 + nlen < 4096) {
                        memcpy(full, path + p, dlen);
                        full[dlen] = '/';
                        memcpy(full + dlen + 1,
                               name, nlen + 1);
                        if (access(full, X_OK) == 0) {
                            cx_puts(full);
                            cx_out("\n", 1);
                            found = 1;
                        }
                    }
                }
                p = e + 1;
            }
            if (!found) {
                rc = 1;
            }
        }
    }
    return rc;
}

static int cmd_type(int argc, char ** argv) {
    if (argc < 2) {
        cx_err("type: usage: type NAME...\n");
        return 1;
    }
    int rc = 0;
    int ai = 1;
    while (ai < argc) {
        char * name = argv[ai];
        // check shell-only builtins first
        if (strcmp(name, "set") == 0 || strcmp(name, "export") == 0 ||
            strcmp(name, "source") == 0 || strcmp(name, "type") == 0) {
            cx_puts(name); cx_out(" is a shell builtin\n", 20);
        } else {
            // check registered commands
            int found = 0;
            int i = 0;
            while (i < ncmds && !found) {
                if (strcmp(name, cmds[i].name) == 0) {
                    cx_puts(name); cx_out(" is a shell builtin\n", 20);
                    found = 1;
                }
                i++;
            }
            if (!found) {
                // check PATH
                char * path = getenv("PATH");
                if (path) {
                    int p = 0;
                    int plen = strlen(path);
                    while (p < plen && !found) {
                        int e = p;
                        while (e < plen && path[e] != ':') { e++; }
                        int dlen = e - p;
                        if (dlen > 0) {
                            char full[4096];
                            int nlen = strlen(name);
                            if (dlen + 1 + nlen < 4096) {
                                memcpy(full, path + p, dlen);
                                full[dlen] = '/';
                                memcpy(full + dlen + 1, name, nlen + 1);
                                if (access(full, X_OK) == 0) {
                                    cx_puts(name); cx_out(" is ", 4);
                                    cx_puts(full); cx_out("\n", 1);
                                    found = 1;
                                }
                            }
                        }
                        p = e + 1;
                    }
                }
                if (!found) {
                    cx_err("type: ");
                    cx_err(name);
                    cx_err(": not found\n");
                    rc = 1;
                }
            }
        }
        ai++;
    }
    return rc;
}

static int dispatch(char * name, int argc, char ** argv);

enum {
    SH_WORD = 1, SH_PIPE, SH_REDOUT, SH_REDAPP, SH_REDIN,
    SH_AND, SH_OR, SH_SEMI, SH_BG
};

char sh_tok_buf[16384];
int64_t sh_tok_types[64];
int sh_tok_count = 0;
char sh_var_names[4096];
char sh_var_vals[16384];
char sh_var_exported[64];
int sh_var_count = 0;
int sh_last_rc = 0;
char sh_expanded[16384];
int sh_tmp_counter = 0;

static int sh_is_var_char(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

static char * sh_get_var(char * name) {
    char * result = 0;
    int i = 0;
    while (i < sh_var_count && !result) {
        if (strcmp(sh_var_names + i * 64, name) == 0) {
            result = sh_var_vals + i * 256;
        }
        i++;
    }
    if (!result) {
        result = getenv(name);
    }
    return result;
}

static void sh_set_var(char * name, char * value) {
    int i = 0;
    int found = 0;
    while (i < sh_var_count && !found) {
        if (strcmp(sh_var_names + i * 64, name) == 0) {
            strcpy(sh_var_vals + i * 256, value);
            found = 1;
        }
        i++;
    }
    if (!found && sh_var_count < 64) {
        strcpy(sh_var_names + sh_var_count * 64, name);
        strcpy(sh_var_vals + sh_var_count * 256, value);
        sh_var_exported[sh_var_count] = 0;
        sh_var_count++;
    }
}

static void sh_export_var(char * name) {
    int i = 0;
    int found = 0;
    while (i < sh_var_count && !found) {
        if (strcmp(sh_var_names + i * 64, name) == 0) {
            sh_var_exported[i] = 1;
            found = 1;
        }
        i++;
    }
}

static void sh_emit_op(int type) {
    sh_tok_buf[sh_tok_count * 256] = 0;
    sh_tok_types[sh_tok_count] = type;
    sh_tok_count++;
}

static void sh_tokenize(char * line) {
    sh_tok_count = 0;
    int i = 0;
    int done = 0;
    while (line[i] && !done && sh_tok_count < 64) {
        while (line[i] == ' ' || line[i] == '\t' ||
               line[i] == '\n') {
            i++;
        }
        if (!line[i]) {
            done = 1;
        }
        if (!done) {
            char c = line[i];
            if (c == '&' && line[i + 1] == '&') {
                sh_emit_op(SH_AND);
                i += 2;
            } else if (c == '|' && line[i + 1] == '|') {
                sh_emit_op(SH_OR);
                i += 2;
            } else if (c == '|') {
                sh_emit_op(SH_PIPE);
                i++;
            } else if (c == ';') {
                sh_emit_op(SH_SEMI);
                i++;
            } else if (c == '>' && line[i + 1] == '>') {
                sh_emit_op(SH_REDAPP);
                i += 2;
            } else if (c == '>') {
                sh_emit_op(SH_REDOUT);
                i++;
            } else if (c == '<') {
                sh_emit_op(SH_REDIN);
                i++;
            } else if (c == '&') {
                sh_emit_op(SH_BG);
                i++;
            } else {
                int o = 0;
                int wdone = 0;
                while (line[i] && !wdone && o < 255) {
                    char wc = line[i];
                    if (wc == ' ' || wc == '\t' || wc == '\n' ||
                        wc == '|' || wc == '&' || wc == ';' ||
                        wc == '<' || wc == '>') {
                        wdone = 1;
                    } else if (wc == '"' || wc == '\'') {
                        char quote = wc;
                        i++;
                        while (line[i] && line[i] != quote && o < 255) {
                            sh_tok_buf[sh_tok_count * 256 + o] = line[i];
                            o++;
                            i++;
                        }
                        if (line[i] == quote) {
                            i++;
                        }
                    } else {
                        sh_tok_buf[sh_tok_count * 256 + o] = wc;
                        o++;
                        i++;
                    }
                }
                sh_tok_buf[sh_tok_count * 256 + o] = 0;
                sh_tok_types[sh_tok_count] = SH_WORD;
                sh_tok_count++;
            }
        }
    }
}

static void sh_expand_word(char * src, char * dst, int max) {
    int i = 0;
    int o = 0;
    // ~ expansion at start of word: ~ or ~/...
    if (src[0] == '~' && (src[1] == 0 || src[1] == '/')) {
        char * home = getenv("HOME");
        if (home) {
            while (home[0] && o < max - 1) {
                dst[o] = home[0]; o++; home++;
            }
        }
        i = 1; // skip the ~, keep the /... part
    }
    while (src[i] && o < max - 1) {
        if (src[i] == '$') {
            i++;
            char vn[64];
            int vl = 0;
            if (src[i] == '?') {
                vn[vl] = '?';
                vl++;
                i++;
            } else {
                while (sh_is_var_char(src[i]) && vl < 63) {
                    vn[vl] = src[i];
                    vl++;
                    i++;
                }
            }
            vn[vl] = 0;
            if (vl > 0) {
                if (strcmp(vn, "?") == 0) {
                    char buf[16];
                    int blen = cx_itoa(sh_last_rc, buf);
                    int k = 0;
                    while (k < blen && o < max - 1) {
                        dst[o] = buf[k];
                        o++;
                        k++;
                    }
                } else {
                    char * val = sh_get_var(vn);
                    if (val) {
                        int k = 0;
                        while (val[k] && o < max - 1) {
                            dst[o] = val[k];
                            o++;
                            k++;
                        }
                    }
                }
            } else {
                dst[o] = '$';
                o++;
            }
        } else {
            dst[o] = src[i];
            o++;
            i++;
        }
    }
    dst[o] = 0;
}

static void sh_expand_prompt(char * dst, int max) {
    char * ps1 = sh_get_var("PS1");
    if (!ps1) { ps1 = "\\w$ "; }
    char cwd[4096];
    if (!getcwd(cwd, 4096)) { cwd[0] = 0; }
    char * base = strrchr(cwd, '/');
    base = base ? base + 1 : cwd;
    // first pass: expand \w \W \u \h \$ backslash escapes
    char tmp[512];
    int i = 0;
    int o = 0;
    while (ps1[i] && o < 510) {
        if (ps1[i] == '\\' && ps1[i + 1]) {
            char c = ps1[i + 1];
            i = i + 2;
            if (c == 'w') {
                int k = 0;
                while (base[k] && o < 510) { tmp[o] = base[k]; o++; k++; }
            } else if (c == 'W') {
                int k = 0;
                while (cwd[k] && o < 510) { tmp[o] = cwd[k]; o++; k++; }
            } else if (c == 'u') {
                char * u = getenv("USER");
                if (!u) { u = "?"; }
                int k = 0;
                while (u[k] && o < 510) { tmp[o] = u[k]; o++; k++; }
            } else if (c == 'h') {
                char * hn = getenv("HOSTNAME");
                if (!hn) { hn = getenv("HOST"); }
                if (!hn) { hn = getenv("NAME"); }
                if (!hn) { hn = "?"; }
                int k = 0;
                while (hn[k] && hn[k] != '.' && o < 510) {
                    tmp[o] = hn[k]; o++; k++;
                }
            } else if (c == '$') {
                tmp[o] = '$'; o++;
            } else if (c == 'n') {
                tmp[o] = 10; o++;
            } else {
                tmp[o] = '\\'; o++;
                if (o < 510) { tmp[o] = c; o++; }
            }
        } else {
            tmp[o] = ps1[i]; o++; i++;
        }
    }
    tmp[o] = 0;
    // second pass: expand $VAR references
    sh_expand_word(tmp, dst, max);
}

static void sh_run_tokens(int start, int end);

static int cmd_sh_set(int argc, char ** argv) {
    int i = 0;
    while (i < sh_var_count) {
        cx_puts(sh_var_names + i * 64);
        cx_out("=", 1);
        cx_puts(sh_var_vals + i * 256);
        cx_out("\n", 1);
        i++;
    }
    return 0;
}

static int cmd_sh_export(int argc, char ** argv) {
    if (argc < 2) {
        int i = 0;
        while (i < sh_var_count) {
            if (sh_var_exported[i]) {
                cx_err("export ");
                cx_err(sh_var_names + i * 64);
                cx_err("=");
                cx_err(sh_var_vals + i * 256);
                cx_err("\n");
            }
            i++;
        }
        return 0;
    }
    int j = 1;
    while (j < argc) {
        sh_export_var(argv[j]);
        j++;
    }
    return 0;
}

static cmd_fn sh_find_cmd(char * name) {
    if (strcmp(name, "set") == 0) return cmd_sh_set;
    if (strcmp(name, "export") == 0) return cmd_sh_export;
    cmd_fn result = 0;
    int i = 0;
    while (i < ncmds && !result) {
        if (strcmp(name, cmds[i].name) == 0) {
            result = cmds[i].fn;
        }
        i++;
    }
    return result;
}

static void sh_sync_env(struct sb * b) {
    int i = 0;
    while (i < sh_var_count) {
        if (sh_var_exported[i]) {
            sb_puts(b, sh_var_names + i * 64);
            sb_putc(b, '=');
            sb_puts(b, sh_var_vals + i * 256);
            sb_putc(b, ' ');
        }
        i++;
    }
}

static int sh_run_external(int argc, char ** argv) {
    struct sb sb_cmd;
    memset(&sb_cmd, 0, sizeof(struct sb));
    sh_sync_env(&sb_cmd);
    int j = 0;
    while (j < argc) {
        if (j > 0 || sb_cmd.count > 0) { sb_putc(&sb_cmd, ' '); }
        sb_puts(&sb_cmd, argv[j]);
        j++;
    }
    int wstatus = system(sb_cmd.data ? sb_cmd.data : "");
    sb_free(&sb_cmd);
    return wstatus / 256;
}

static void sh_make_tmp_name(char * out, int idx) {
    strcpy(out, "/tmp/cx_pipe_");
    char nbuf[16];
    cx_itoa(idx, nbuf);
    strcat(out, nbuf);
}

static int sh_exec_segment(int start, int end) {
    char * argv[64];
    int argc = 0;
    char stdin_path[256];
    char stdout_path[256];
    int has_stdin = 0;
    int has_stdout = 0;
    int append_stdout = 0;
    int i = start;
    while (i < end) {
        int t = sh_tok_types[i];
        if (t == SH_WORD) {
            char * src = sh_tok_buf + i * 256;
            char * dst = sh_expanded + argc * 256;
            sh_expand_word(src, dst, 256);
            argv[argc] = dst;
            argc++;
            i++;
        } else if (t == SH_REDIN && i + 1 < end) {
            sh_expand_word(sh_tok_buf + (i + 1) * 256,
                           stdin_path, 256);
            has_stdin = 1;
            i += 2;
        } else if (t == SH_REDOUT && i + 1 < end) {
            sh_expand_word(sh_tok_buf + (i + 1) * 256,
                           stdout_path, 256);
            has_stdout = 1;
            append_stdout = 0;
            i += 2;
        } else if (t == SH_REDAPP && i + 1 < end) {
            sh_expand_word(sh_tok_buf + (i + 1) * 256,
                           stdout_path, 256);
            has_stdout = 1;
            append_stdout = 1;
            i += 2;
        } else {
            i++;
        }
    }
    int rc = 0;
    int saved_in = -1;
    int saved_out = -1;
    if (has_stdin) {
        int fd = open(stdin_path, 0);
        if (fd < 0) {
            cx_err("sh: cannot open: ");
            cx_err(stdin_path);
            cx_err("\n");
            rc = 1;
        } else {
            saved_in = dup2(0, 100);
            dup2(fd, 0);
            close(fd);
        }
    }
    if (!rc && has_stdout) {
        int flags = 1 | O_CREAT;
        if (append_stdout) {
            flags = flags | O_APPEND;
        } else {
            flags = flags | O_TRUNC;
        }
        int fd = open(stdout_path, flags, 420);
        if (fd < 0) {
            cx_err("sh: cannot open: ");
            cx_err(stdout_path);
            cx_err("\n");
            rc = 1;
        } else {
            saved_out = dup2(1, 101);
            dup2(fd, 1);
            close(fd);
        }
    }
    if (!rc) {
        int handled = 0;
        if (argc == 1 && sh_tok_types[start] == SH_WORD) {
            char * raw = sh_tok_buf + start * 256;
            char * eq = strchr(raw, '=');
            if (eq && eq != raw) {
                int nlen = eq - raw;
                char vn[64];
                memcpy(vn, raw, nlen);
                vn[nlen] = 0;
                char vv[256];
                sh_expand_word(eq + 1, vv, 256);
                sh_set_var(vn, vv);
                rc = 0;
                handled = 1;
            }
        }
        if (!handled && argc >= 2 &&
            (strcmp(argv[0], "source") == 0 ||
             strcmp(argv[0], ".") == 0)) {
            int fd = open(argv[1], 0);
            if (fd < 0) {
                cx_err("source: cannot open: ");
                cx_err(argv[1]);
                cx_err("\n");
                rc = 1;
            } else {
                char * sb_back = (char*)malloc(16384);
                int64_t * st = (int64_t*)malloc(512);
                memcpy(sb_back, sh_tok_buf, 16384);
                memcpy(st, sh_tok_types, 512);
                int sc = sh_tok_count;
                char line[4096];
                int n = cx_getline(fd, line, 4096);
                while (n > 0) {
                    sh_tokenize(line);
                    sh_run_tokens(0, sh_tok_count);
                    n = cx_getline(fd, line, 4096);
                }
                close(fd);
                memcpy(sh_tok_buf, sb_back, 16384);
                memcpy(sh_tok_types, st, 512);
                sh_tok_count = sc;
                free(sb_back);
                free(st);
                rc = sh_last_rc;
            }
            handled = 1;
        }
        if (!handled && argc > 0) {
            char ** actual_argv = argv;
            int actual_argc = argc;
            if (argc > 1 && strcmp(argv[0], "--") == 0) {
                actual_argv = argv + 1;
                actual_argc = argc - 1;
            }
            cmd_fn fn = sh_find_cmd(actual_argv[0]);
            if (fn) {
                rc = fn(actual_argc, actual_argv);
            } else {
                // heuristic: if command ends in .c, run through cx
                char * cmd0 = actual_argv[0];
                int clen = strlen(cmd0);
                if (clen > 2 && cmd0[clen - 2] == '.' &&
                    cmd0[clen - 1] == 'c' &&
                    access(cmd0, 4) == 0 && cx_path[0]) {
                    // build: cx_path file.c args...
                    char * cx_argv[64];
                    cx_argv[0] = cx_path;
                    int ca = 0;
                    while (ca < actual_argc && ca < 62) {
                        cx_argv[ca + 1] = actual_argv[ca];
                        ca++;
                    }
                    cx_argv[ca + 1] = 0;
                    rc = sh_run_external(ca + 1, cx_argv);
                } else {
                    rc = sh_run_external(actual_argc, actual_argv);
                }
            }
        }
    }
    if (saved_in >= 0) {
        dup2(saved_in, 0);
        close(saved_in);
    }
    if (saved_out >= 0) {
        dup2(saved_out, 1);
        close(saved_out);
    }
    return rc;
}

static void sh_run_pipeline(int start, int end) {
    int seg_count = 1;
    int i = start;
    while (i < end) {
        if (sh_tok_types[i] == SH_PIPE) {
            seg_count++;
        }
        i++;
    }
    if (seg_count == 1) {
        sh_last_rc = sh_exec_segment(start, end);
    } else {
        char tmps[512];
        int n_tmps = seg_count - 1;
        if (n_tmps > 8) {
            n_tmps = 8;
        }
        int t = 0;
        while (t < n_tmps) {
            sh_make_tmp_name(tmps + t * 64, sh_tmp_counter);
            sh_tmp_counter++;
            t++;
        }
        int seg_starts[9];
        int seg_ends[9];
        int sc = 0;
        seg_starts[0] = start;
        i = start;
        while (i < end && sc < 8) {
            if (sh_tok_types[i] == SH_PIPE) {
                seg_ends[sc] = i;
                sc++;
                seg_starts[sc] = i + 1;
            }
            i++;
        }
        seg_ends[sc] = end;
        sc++;
        int s = 0;
        int rc = 0;
        while (s < sc) {
            int saved_in = -1;
            int saved_out = -1;
            if (s > 0) {
                int fd = open(tmps + (s - 1) * 64, 0);
                if (fd >= 0) {
                    saved_in = dup2(0, 100);
                    dup2(fd, 0);
                    close(fd);
                }
            }
            if (s < sc - 1) {
                int fd = open(tmps + s * 64,
                    1 | O_CREAT | O_TRUNC, 420);
                if (fd >= 0) {
                    saved_out = dup2(1, 101);
                    dup2(fd, 1);
                    close(fd);
                }
            }
            rc = sh_exec_segment(seg_starts[s], seg_ends[s]);
            if (saved_in >= 0) {
                dup2(saved_in, 0);
                close(saved_in);
            }
            if (saved_out >= 0) {
                dup2(saved_out, 1);
                close(saved_out);
            }
            s++;
        }
        t = 0;
        while (t < n_tmps) {
            unlink(tmps + t * 64);
            t++;
        }
        sh_last_rc = rc;
    }
}

static void sh_run_tokens(int start, int end) {
    int i = start;
    int prev_op = SH_SEMI;
    while (i < end) {
        int p_end = i;
        while (p_end < end &&
               sh_tok_types[p_end] != SH_SEMI &&
               sh_tok_types[p_end] != SH_AND &&
               sh_tok_types[p_end] != SH_OR) {
            p_end++;
        }
        int run = 0;
        if (prev_op == SH_SEMI) {
            run = 1;
        } else if (prev_op == SH_AND && sh_last_rc == 0) {
            run = 1;
        } else if (prev_op == SH_OR && sh_last_rc != 0) {
            run = 1;
        }
        if (run && p_end > i) {
            sh_run_pipeline(i, p_end);
        }
        if (p_end < end) {
            prev_op = sh_tok_types[p_end];
            i = p_end + 1;
        } else {
            i = p_end;
        }
    }
}

static int cmd_date(int argc, char ** argv) {
    int t = time(0);
    int res[6];
#ifdef __cx__
    if (localtime(t, res) == 0) {
#else
    time_t tt = t;
    struct tm *lt = localtime(&tt);
    if (lt) {
        res[0] = lt->tm_year + 1900;
        res[1] = lt->tm_mon + 1;
        res[2] = lt->tm_mday;
        res[3] = lt->tm_hour;
        res[4] = lt->tm_min;
        res[5] = lt->tm_sec;
#endif
        cx_putint(1, res[0]);
        cx_out("-", 1);
        if (res[1] < 10) { cx_out("0", 1); }
        cx_putint(1, res[1]);
        cx_out("-", 1);
        if (res[2] < 10) { cx_out("0", 1); }
        cx_putint(1, res[2]);
        cx_out(" ", 1);
        if (res[3] < 10) { cx_out("0", 1); }
        cx_putint(1, res[3]);
        cx_out(":", 1);
        if (res[4] < 10) { cx_out("0", 1); }
        cx_putint(1, res[4]);
        cx_out(":", 1);
        if (res[5] < 10) { cx_out("0", 1); }
        cx_putint(1, res[5]);
        cx_out("\n", 1);
        return 0;
    }
    return 1;
}

static int cmd_sleep(int argc, char ** argv) {
    if (argc < 2) {
        cx_err("sleep: missing operand\n");
        return 1;
    }
    sleep(atoi(argv[1]));
    return 0;
}

static int cmd_kill(int argc, char ** argv) {
    if (argc < 2) {
        cx_err("kill: missing operand\n");
        return 1;
    }
    int sig = 15;
    int pid = 0;
    if (argc >= 3 && argv[1][0] == '-') {
        sig = atoi(argv[1] + 1);
        pid = atoi(argv[2]);
    } else {
        pid = atoi(argv[1]);
    }
    if (kill(pid, sig) != 0) {
        cx_err("kill: failed\n");
        return 1;
    }
    return 0;
}

static int cmd_ps(int argc, char ** argv) {
    // try /proc on Linux, fall back to popen("ps") on macOS
    struct cx_stat st;
    int have_proc = stat("/proc/1", (void*)&st) == 0;
    if (have_proc) {
        cx_puts("  PID STATE COMMAND\n");
        void * dp = opendir("/proc");
        if (!dp) { cx_err("ps: cannot open /proc\n"); return 1; }
        char * name = (char*)readdir(dp);
        while (name) {
            // only numeric directories are PIDs
            int is_pid = name[0] >= '1' && name[0] <= '9';
            int k = 0;
            while (is_pid && name[k]) {
                if (name[k] < '0' || name[k] > '9') { is_pid = 0; }
                k++;
            }
            if (is_pid) {
                char path[128];
                strcpy(path, "/proc/");
                strcat(path, name);
                strcat(path, "/comm");
                int fd = open(path, 0);
                char comm[256];
                comm[0] = 0;
                if (fd >= 0) {
                    int n = read(fd, comm, 255);
                    if (n < 0) { n = 0; }
                    comm[n] = 0;
                    // strip trailing newline
                    if (n > 0 && comm[n - 1] == 10) { comm[n - 1] = 0; }
                    close(fd);
                }
                strcpy(path, "/proc/");
                strcat(path, name);
                strcat(path, "/stat");
                fd = open(path, 0);
                char state = '?';
                if (fd >= 0) {
                    char sbuf[512];
                    int n = read(fd, sbuf, 511);
                    if (n < 0) { n = 0; }
                    sbuf[n] = 0;
                    // stat format: PID (comm) STATE ...
                    // find closing paren, state is next non-space
                    char * cp = sbuf;
                    while (*cp && *cp != ')') { cp++; }
                    if (*cp == ')') { cp++; }
                    while (*cp == ' ') { cp++; }
                    if (*cp) { state = *cp; }
                    close(fd);
                }
                char nb[16];
                cx_itopad(atoi(name), nb, 5);
                cx_puts(nb);
                cx_out(" ", 1);
                char sb[4];
                sb[0] = ' '; sb[1] = ' '; sb[2] = state; sb[3] = ' ';
                cx_write(1, sb, 4);
                cx_out(" ", 1);
                cx_puts(comm);
                cx_out("\n", 1);
            }
            name = (char*)readdir(dp);
        }
        closedir(dp);
    } else {
        // macOS fallback: use system ps and pipe to stdout
        int r = system("ps -eo pid,stat,comm");
        (void)r;
    }
    return 0;
}

// --- Shell history and line editing ---

#define RL_HIST_MAX 64
#define RL_HIST_LEN 1024

char rl_hist[65536]; // RL_HIST_MAX * RL_HIST_LEN
int rl_hist_count;

static void rl_hist_add(char * line) {
    if (line[0] == 0) return;
    if (rl_hist_count > 0) {
        char * last = rl_hist + (rl_hist_count - 1) * RL_HIST_LEN;
        if (strcmp(last, line) == 0) return;
    }
    if (rl_hist_count >= RL_HIST_MAX) {
        memmove(rl_hist, rl_hist + RL_HIST_LEN,
                (RL_HIST_MAX - 1) * RL_HIST_LEN);
        rl_hist_count = RL_HIST_MAX - 1;
    }
    strcpy(rl_hist + rl_hist_count * RL_HIST_LEN, line);
    rl_hist_count++;
}

static void rl_esc(char c) {
    char buf[3];
    buf[0] = 27;
    buf[1] = '[';
    buf[2] = c;
    cx_write(2, buf, 3);
}

static void rl_esc_n(int n, char c) {
    char buf[16];
    char nb[8];
    buf[0] = 27;
    buf[1] = '[';
    int len = cx_itoa(n, nb);
    memcpy(buf + 2, nb, len);
    buf[2 + len] = c;
    cx_write(2, buf, 3 + len);
}

static void rl_redraw(char * prompt, int plen, char * buf,
                       int len, int pos) {
    cx_write(2, "\r", 1);
    cx_write(2, prompt, plen);
    if (len > 0) { cx_write(2, buf, len); }
    rl_esc('K');
    cx_write(2, "\r", 1);
    if (plen + pos > 0) { rl_esc_n(plen + pos, 'C'); }
}

static int sh_readline(char * buf, int size, char * prompt) {
    int plen = strlen(prompt);
    int len = 0;
    int pos = 0;
    int hi = rl_hist_count;
    char saved[1024];
    saved[0] = 0;
    buf[0] = 0;
    cx_write(2, prompt, plen);
    termraw(0, 1);
    char ch[2];
    int done = 0;
    int result = 0;
    while (!done) {
        ch[0] = 0;
        ch[1] = 0;
        int n = read(0, ch, 1);
        if (n <= 0) {
            result = -1;
            done = 1;
        } else if (ch[0] == 13 || ch[0] == 10) {
            cx_write(2, "\r\n", 2);
            buf[len] = 0;
            if (len > 0) { rl_hist_add(buf); }
            result = len;
            done = 1;
        } else if (ch[0] == 4 && len == 0) {
            cx_write(2, "\r\n", 2);
            result = -1;
            done = 1;
        } else if (ch[0] == 4 && pos < len) {
            memmove(buf + pos, buf + pos + 1, len - pos - 1);
            len--;
            buf[len] = 0;
            rl_redraw(prompt, plen, buf, len, pos);
        } else if (ch[0] == 3) {
            len = 0;
            pos = 0;
            buf[0] = 0;
            termraw(0, 0);
            cx_write(2, "^C\r\n", 4);
            cx_write(2, prompt, plen);
            termraw(0, 1);
        } else if (ch[0] == 1) {
            pos = 0;
            rl_redraw(prompt, plen, buf, len, pos);
        } else if (ch[0] == 5) {
            pos = len;
            rl_redraw(prompt, plen, buf, len, pos);
        } else if (ch[0] == 21) {
            memmove(buf, buf + pos, len - pos);
            len = len - pos;
            pos = 0;
            buf[len] = 0;
            rl_redraw(prompt, plen, buf, len, pos);
        } else if (ch[0] == 11) {
            len = pos;
            buf[len] = 0;
            rl_redraw(prompt, plen, buf, len, pos);
        } else if (ch[0] == 12) {
            char cls[7];
            cls[0] = 27; cls[1] = '['; cls[2] = '2'; cls[3] = 'J';
            cls[4] = 27; cls[5] = '['; cls[6] = 'H';
            cx_write(2, cls, 7);
            rl_redraw(prompt, plen, buf, len, pos);
        } else if (ch[0] == 127 || ch[0] == 8) {
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos);
                pos--;
                len--;
                buf[len] = 0;
                rl_redraw(prompt, plen, buf, len, pos);
            }
        } else if (ch[0] == 27) {
            ch[0] = 0;
            n = read(0, ch, 1);
            if (n > 0 && ch[0] == '[') {
                ch[0] = 0;
                n = read(0, ch, 1);
                if (n > 0) {
                    int c = ch[0];
                    if (c == 'A' && hi > 0) {
                        if (hi == rl_hist_count) {
                            memcpy(saved, buf, len + 1);
                        }
                        hi--;
                        strcpy(buf, rl_hist + hi * RL_HIST_LEN);
                        len = strlen(buf);
                        pos = len;
                        rl_redraw(prompt, plen, buf, len, pos);
                    } else if (c == 'B' && hi < rl_hist_count) {
                        hi++;
                        if (hi == rl_hist_count) {
                            memcpy(buf, saved, strlen(saved) + 1);
                        } else {
                            strcpy(buf, rl_hist + hi * RL_HIST_LEN);
                        }
                        len = strlen(buf);
                        pos = len;
                        rl_redraw(prompt, plen, buf, len, pos);
                    } else if (c == 'C' && pos < len) {
                        pos++;
                        rl_esc('C');
                    } else if (c == 'D' && pos > 0) {
                        pos--;
                        rl_esc('D');
                    } else if (c == 'H') {
                        pos = 0;
                        rl_redraw(prompt, plen, buf, len, pos);
                    } else if (c == 'F') {
                        pos = len;
                        rl_redraw(prompt, plen, buf, len, pos);
                    } else if (c == '3') {
                        ch[0] = 0;
                        n = read(0, ch, 1);
                        if (n > 0 && ch[0] == '~' && pos < len) {
                            memmove(buf + pos, buf + pos + 1,
                                    len - pos - 1);
                            len--;
                            buf[len] = 0;
                            rl_redraw(prompt, plen, buf, len, pos);
                        }
                    }
                }
            }
        } else if (ch[0] == 9) {
            // Tab completion: find word under cursor
            int ws = pos;
            while (ws > 0 && buf[ws - 1] != ' ') { ws--; }
            int wlen = pos - ws;
            // is this the first word? (no non-space before ws)
            int is_first = 1;
            { int k = 0; while (k < ws) {
                if (buf[k] != ' ') { is_first = 0; } k++;
            } }
            char * matches = (char*)malloc(8192);
            int moff = 0;
            int mcount = 0;
            char * last_match = 0;
            char prefix[256];
            if (wlen > 255) { wlen = 255; }
            memcpy(prefix, buf + ws, wlen);
            prefix[wlen] = 0;
            if (is_first) {
                // command name completion
                int ci = 0;
                while (ci < ncmds) {
                    if (strncmp(cmds[ci].name, prefix, wlen) == 0) {
                        int nlen = strlen(cmds[ci].name);
                        if (moff + nlen + 1 < 8192) {
                            memcpy(matches + moff, cmds[ci].name,
                                   nlen + 1);
                            last_match = matches + moff;
                            moff = moff + nlen + 1;
                            mcount++;
                        }
                    }
                    ci++;
                }
            } else {
                // filename completion — split prefix into dir + base
                char dir[256];
                char base[256];
                int last_slash = -1;
                { int k = 0; while (k < wlen) {
                    if (prefix[k] == '/') { last_slash = k; } k++;
                } }
                if (last_slash >= 0) {
                    memcpy(dir, prefix, last_slash + 1);
                    dir[last_slash + 1] = 0;
                    strcpy(base, prefix + last_slash + 1);
                } else {
                    strcpy(dir, ".");
                    strcpy(base, prefix);
                }
                int blen = strlen(base);
                void * dp = opendir(dir);
                if (dp) {
                    char * ent = (char*)readdir(dp);
                    while (ent) {
                        if (ent[0] != '.' || (blen > 0 && base[0] == '.')) {
                            if (strncmp(ent, base, blen) == 0) {
                                // build full match: dir/name or just name
                                char full[512];
                                int fl = 0;
                                if (last_slash >= 0) {
                                    memcpy(full, dir, strlen(dir));
                                    fl = strlen(dir);
                                }
                                int el = strlen(ent);
                                memcpy(full + fl, ent, el + 1);
                                fl = fl + el;
                                // append / if directory
                                char spath[512];
                                if (last_slash >= 0) {
                                    strcpy(spath, full);
                                } else {
                                    strcpy(spath, dir);
                                    strcat(spath, "/");
                                    strcat(spath, ent);
                                }
                                struct cx_stat fst;
                                if (stat(spath, (void*)&fst) == 0 &&
                                    (fst.mode & 0040000)) {
                                    full[fl] = '/'; fl++;
                                    full[fl] = 0;
                                }
                                if (moff + fl + 1 < 8192) {
                                    memcpy(matches + moff, full, fl + 1);
                                    last_match = matches + moff;
                                    moff = moff + fl + 1;
                                    mcount++;
                                }
                            }
                        }
                        ent = (char*)readdir(dp);
                    }
                    closedir(dp);
                }
            }
            if (mcount == 1) {
                // single match — complete it
                int nlen = strlen(last_match);
                int add = nlen - wlen;
                // add trailing space unless it ends with /
                int trail = (last_match[nlen - 1] == '/') ? 0 : 1;
                if (len + add + trail < size) {
                    memmove(buf + pos + add + trail,
                            buf + pos, len - pos);
                    memcpy(buf + pos, last_match + wlen, add);
                    if (trail) { buf[pos + add] = ' '; }
                    len = len + add + trail;
                    pos = pos + add + trail;
                    buf[len] = 0;
                    rl_redraw(prompt, plen, buf, len, pos);
                }
            } else if (mcount > 1) {
                // multiple matches — show them
                cx_write(2, "\r\n", 2);
                int mi = 0;
                char * mp = matches;
                while (mi < mcount) {
                    int ml = strlen(mp);
                    cx_write(2, mp, ml);
                    cx_write(2, "  ", 2);
                    mp = mp + ml + 1;
                    mi++;
                }
                cx_write(2, "\r\n", 2);
                rl_redraw(prompt, plen, buf, len, pos);
            }
            free(matches);
        } else if (ch[0] >= 32 && len < size - 1) {
            if (pos < len) {
                memmove(buf + pos + 1, buf + pos, len - pos);
            }
            buf[pos] = ch[0];
            pos++;
            len++;
            buf[len] = 0;
            rl_redraw(prompt, plen, buf, len, pos);
        }
    }
    termraw(0, 0);
    return result;
}

// ====================== vi editor ======================

#define VI_MAXLN 4096
#define VI_NORMAL 0
#define VI_INSERT 1
#define VI_EX 2
#define VK_UP    256
#define VK_DOWN  257
#define VK_RIGHT 258
#define VK_LEFT  259
#define VK_HOME  260
#define VK_END   261
#define VK_DEL   262
#define VK_PGUP  263
#define VK_PGDN  264

char * vi_ld[4096];
int vi_ll[4096];
int vi_lc[4096];
int vi_nl;
int vi_cx;
int vi_cy;
int vi_top;
int vi_left;
int vi_rows;
int vi_cols;
int vi_mode;
int vi_dirty;
int vi_run;
int vi_pend;
int vi_pb;
char vi_file[256];
char vi_msg[256];
char vi_ex_buf[256];
int vi_ex_len;
int vi_ex_pr;
char vi_srch[256];
char vi_scr[32768];
int vi_sn;

static void vi_sput(char * s, int n) {
    if (vi_sn + n < 32768) {
        memcpy(vi_scr + vi_sn, s, n);
        vi_sn = vi_sn + n;
    }
}

static void vi_sputs(char * s) { vi_sput(s, strlen(s)); }

static void vi_sflush(void) {
    if (vi_sn > 0) { cx_write(1, vi_scr, vi_sn); vi_sn = 0; }
}

static void vi_sputc(int c) {
    if (vi_sn < 32767) { vi_scr[vi_sn] = c; vi_sn++; }
}

static void vi_sesc(char * s) {
    vi_sputc(27);
    vi_sputs(s);
}

static void vi_sgoto(int row, int col) {
    char b[24];
    char n1[8];
    char n2[8];
    b[0] = 27; b[1] = '[';
    int l1 = cx_itoa(row + 1, n1);
    memcpy(b + 2, n1, l1);
    b[2 + l1] = ';';
    int l2 = cx_itoa(col + 1, n2);
    memcpy(b + 3 + l1, n2, l2);
    b[3 + l1 + l2] = 'H';
    vi_sput(b, 4 + l1 + l2);
}

// --- vi line management ---

static void vi_lensure(int i, int need) {
    if (need + 1 > vi_lc[i]) {
        int cap = vi_lc[i];
        if (cap < 32) { cap = 32; }
        while (cap < need + 1) { cap = cap * 2; }
        vi_ld[i] = (char *)realloc(vi_ld[i], cap);
        vi_lc[i] = cap;
    }
}

static void vi_ladd(int at, char * text, int len) {
    if (vi_nl >= VI_MAXLN) return;
    int i = vi_nl;
    while (i > at) {
        vi_ld[i] = vi_ld[i - 1];
        vi_ll[i] = vi_ll[i - 1];
        vi_lc[i] = vi_lc[i - 1];
        i--;
    }
    int cap = 32;
    while (cap < len + 1) { cap = cap * 2; }
    char * p = (char *)malloc(cap);
    if (len > 0) { memcpy(p, text, len); }
    p[len] = 0;
    vi_ld[at] = p;
    vi_lc[at] = cap;
    vi_ll[at] = len;
    vi_nl++;
}

static void vi_ldel(int at) {
    if (vi_nl <= 1) {
        vi_ld[0][0] = 0;
        vi_ll[0] = 0;
        return;
    }
    free(vi_ld[at]);
    int i = at;
    while (i < vi_nl - 1) {
        vi_ld[i] = vi_ld[i + 1];
        vi_ll[i] = vi_ll[i + 1];
        vi_lc[i] = vi_lc[i + 1];
        i++;
    }
    vi_nl--;
}

static void vi_lfree(void) {
    int i = 0;
    while (i < vi_nl) { free(vi_ld[i]); i++; }
    vi_nl = 0;
}

// --- vi file I/O ---

static int vi_load(char * filename) {
    strcpy(vi_file, filename);
    int fd = open(filename, 0);
    if (fd < 0) {
        vi_ladd(0, "", 0);
        strcpy(vi_msg, "[New file]");
        return 0;
    }
    int sz = (int)lseek(fd, 0, 2);
    if (sz < 0) { sz = 0; }
    lseek(fd, 0, 0);
    char * buf = (char *)malloc(sz + 1);
    int nr = (int)read(fd, buf, sz);
    if (nr < 0) { nr = 0; }
    buf[nr] = 0;
    sz = nr;
    close(fd);
    int start = 0;
    int i = 0;
    while (i <= sz) {
        if (i == sz || buf[i] == 10) {
            vi_ladd(vi_nl, buf + start, i - start);
            start = i + 1;
        }
        i++;
    }
    free(buf);
    if (vi_nl == 0) { vi_ladd(0, "", 0); }
    if (vi_nl > 1 && vi_ll[vi_nl - 1] == 0) {
        free(vi_ld[vi_nl - 1]);
        vi_nl--;
    }
    return 0;
}

static int vi_save(void) {
    if (vi_file[0] == 0) {
        strcpy(vi_msg, "No filename");
        return -1;
    }
    int fd = open(vi_file, 1 | O_CREAT | O_TRUNC, 420);
    if (fd < 0) {
        strcpy(vi_msg, "Cannot write");
        return -1;
    }
    int i = 0;
    int bytes = 0;
    while (i < vi_nl) {
        cx_write(fd, vi_ld[i], vi_ll[i]);
        cx_write(fd, "\n", 1);
        bytes = bytes + vi_ll[i] + 1;
        i++;
    }
    close(fd);
    vi_dirty = 0;
    char nb[16];
    strcpy(vi_msg, "\"");
    strcat(vi_msg, vi_file);
    strcat(vi_msg, "\" ");
    cx_itoa(vi_nl, nb);
    strcat(vi_msg, nb);
    strcat(vi_msg, "L ");
    cx_itoa(bytes, nb);
    strcat(vi_msg, nb);
    strcat(vi_msg, "B written");
    return 0;
}

// --- vi terminal and cursor ---

static void vi_getsize(void) {
    int buf[2];
    buf[0] = 24; buf[1] = 80;
    winsize(1, buf);
    vi_rows = buf[0];
    vi_cols = buf[1];
}

static int vi_maxcx(void) {
    int len = vi_ll[vi_cy];
    if (vi_mode == VI_INSERT) return len;
    return len > 0 ? len - 1 : 0;
}

static void vi_clamp(void) {
    if (vi_cy < 0) { vi_cy = 0; }
    if (vi_cy >= vi_nl) { vi_cy = vi_nl - 1; }
    int mx = vi_maxcx();
    if (vi_cx > mx) { vi_cx = mx; }
    if (vi_cx < 0) { vi_cx = 0; }
}

static void vi_scroll(void) {
    if (vi_cy < vi_top) { vi_top = vi_cy; }
    if (vi_cy >= vi_top + vi_rows - 1) {
        vi_top = vi_cy - vi_rows + 2;
    }
    if (vi_cx < vi_left) { vi_left = vi_cx; }
    if (vi_cx >= vi_left + vi_cols) {
        vi_left = vi_cx - vi_cols + 1;
    }
}

// --- vi rendering ---

static void vi_render(void) {
    vi_getsize();
    vi_scroll();
    vi_sn = 0;
    vi_sesc("[?25l");
    vi_sgoto(0, 0);
    int r = 0;
    while (r < vi_rows - 1) {
        int ln = vi_top + r;
        if (ln < vi_nl) {
            char * line = vi_ld[ln];
            int len = vi_ll[ln];
            int start = vi_left;
            if (start > len) { start = len; }
            int show = len - start;
            if (show > vi_cols) { show = vi_cols; }
            if (show > 0) { vi_sput(line + start, show); }
        } else {
            vi_sputc('~');
        }
        vi_sesc("[K");
        if (r < vi_rows - 2) { vi_sputc(13); vi_sputc(10); }
        r++;
    }
    vi_sgoto(vi_rows - 1, 0);
    vi_sesc("[7m");
    if (vi_mode == VI_EX) {
        vi_sputc(vi_ex_pr);
        if (vi_ex_len > 0) { vi_sput(vi_ex_buf, vi_ex_len); }
    } else if (vi_msg[0]) {
        vi_sputs(vi_msg);
    } else {
        if (vi_file[0]) { vi_sputs(vi_file); }
        else { vi_sputs("[No Name]"); }
        if (vi_dirty) { vi_sputs(" [+]"); }
        vi_sputs("  ");
        char nb[16];
        cx_itoa(vi_cy + 1, nb); vi_sputs(nb);
        vi_sputc('/');
        cx_itoa(vi_nl, nb); vi_sputs(nb);
        vi_sputs("  Col ");
        cx_itoa(vi_cx + 1, nb); vi_sputs(nb);
        if (vi_mode == VI_INSERT) { vi_sputs("  -- INSERT --"); }
    }
    vi_sesc("[K");
    vi_sesc("[0m");
    if (vi_mode == VI_EX) {
        vi_sgoto(vi_rows - 1, vi_ex_len + 1);
    } else {
        vi_sgoto(vi_cy - vi_top, vi_cx - vi_left);
    }
    vi_sesc("[?25h");
    vi_sflush();
    vi_msg[0] = 0;
}

// --- vi key reading ---

static int vi_readkey(void) {
    if (vi_pb >= 0) {
        int c = vi_pb;
        vi_pb = -1;
        return c;
    }
    char ch[2];
    ch[0] = 0; ch[1] = 0;
    int n = read(0, ch, 1);
    if (n <= 0) return -1;
    if (ch[0] != 27) return ch[0];
    ch[0] = 0;
    n = read(0, ch, 1);
    if (n <= 0) return 27;
    if (ch[0] != '[') { vi_pb = ch[0]; return 27; }
    ch[0] = 0;
    n = read(0, ch, 1);
    if (n <= 0) return 27;
    if (ch[0] == 'A') return VK_UP;
    if (ch[0] == 'B') return VK_DOWN;
    if (ch[0] == 'C') return VK_RIGHT;
    if (ch[0] == 'D') return VK_LEFT;
    if (ch[0] == 'H') return VK_HOME;
    if (ch[0] == 'F') return VK_END;
    if (ch[0] == '3') { int r = read(0, ch, 1); (void)r; return VK_DEL; }
    if (ch[0] == '5') { int r = read(0, ch, 1); (void)r; return VK_PGUP; }
    if (ch[0] == '6') { int r = read(0, ch, 1); (void)r; return VK_PGDN; }
    return -1;
}

// --- vi search ---

static char * vi_strfind(char * s, char * pat) {
    int pl = strlen(pat);
    int sl = strlen(s);
    int i = 0;
    while (i <= sl - pl) {
        if (memcmp(s + i, pat, pl) == 0) return s + i;
        i++;
    }
    return 0;
}

static void vi_search(void) {
    if (vi_srch[0] == 0) return;
    int y = vi_cy;
    int x0 = vi_cx + 1;
    int wrap = 0;
    while (1) {
        char * line = vi_ld[y];
        int off = (y == vi_cy && !wrap) ? x0 : 0;
        if (off < vi_ll[y]) {
            char * f = vi_strfind(line + off, vi_srch);
            if (f) {
                vi_cy = y;
                vi_cx = (int)(f - line);
                return;
            }
        }
        y++;
        if (y >= vi_nl) { y = 0; wrap = 1; }
        if (y == vi_cy && wrap) {
            strcpy(vi_msg, "Pattern not found");
            return;
        }
    }
}

// --- vi normal mode ---

static void vi_normal(int k) {
    if (vi_pend) {
        int p = vi_pend;
        vi_pend = 0;
        if (p == 'd' && k == 'd') {
            vi_ldel(vi_cy);
            vi_clamp();
            vi_dirty = 1;
        } else if (p == 'g' && k == 'g') {
            vi_cy = 0;
            vi_cx = 0;
        } else if (p == 'Z' && k == 'Z') {
            if (vi_dirty) { vi_save(); }
            vi_run = 0;
        }
        return;
    }
    if (k == 'h' || k == VK_LEFT) {
        if (vi_cx > 0) vi_cx--;
    } else if (k == 'l' || k == VK_RIGHT) {
        if (vi_cx < vi_maxcx()) vi_cx++;
    } else if (k == 'j' || k == VK_DOWN) {
        if (vi_cy < vi_nl - 1) vi_cy++;
        vi_clamp();
    } else if (k == 'k' || k == VK_UP) {
        if (vi_cy > 0) vi_cy--;
        vi_clamp();
    } else if (k == '0' || k == VK_HOME) {
        vi_cx = 0;
    } else if (k == '$' || k == VK_END) {
        vi_cx = vi_maxcx();
    } else if (k == '^') {
        char * line = vi_ld[vi_cy];
        int i = 0;
        while (i < vi_ll[vi_cy] && (line[i] == ' ' || line[i] == 9)) i++;
        vi_cx = i;
        vi_clamp();
    } else if (k == 'G') {
        vi_cy = vi_nl - 1;
        vi_clamp();
    } else if (k == 'g') {
        vi_pend = 'g';
    } else if (k == 'i') {
        vi_mode = VI_INSERT;
    } else if (k == 'I') {
        char * line = vi_ld[vi_cy];
        int i = 0;
        while (i < vi_ll[vi_cy] && (line[i] == ' ' || line[i] == 9)) i++;
        vi_cx = i;
        vi_mode = VI_INSERT;
    } else if (k == 'a') {
        if (vi_ll[vi_cy] > 0) vi_cx++;
        if (vi_cx > vi_ll[vi_cy]) vi_cx = vi_ll[vi_cy];
        vi_mode = VI_INSERT;
    } else if (k == 'A') {
        vi_cx = vi_ll[vi_cy];
        vi_mode = VI_INSERT;
    } else if (k == 'o') {
        vi_ladd(vi_cy + 1, "", 0);
        vi_cy++;
        vi_cx = 0;
        vi_mode = VI_INSERT;
        vi_dirty = 1;
    } else if (k == 'O') {
        vi_ladd(vi_cy, "", 0);
        vi_cx = 0;
        vi_mode = VI_INSERT;
        vi_dirty = 1;
    } else if (k == 'x' || k == VK_DEL) {
        if (vi_ll[vi_cy] > 0 && vi_cx < vi_ll[vi_cy]) {
            char * line = vi_ld[vi_cy];
            memmove(line + vi_cx, line + vi_cx + 1,
                    vi_ll[vi_cy] - vi_cx);
            vi_ll[vi_cy]--;
            vi_dirty = 1;
            vi_clamp();
        }
    } else if (k == 'X') {
        if (vi_cx > 0) {
            char * line = vi_ld[vi_cy];
            vi_cx--;
            memmove(line + vi_cx, line + vi_cx + 1,
                    vi_ll[vi_cy] - vi_cx);
            vi_ll[vi_cy]--;
            vi_dirty = 1;
        }
    } else if (k == 'd') {
        vi_pend = 'd';
    } else if (k == 'r') {
        int nk = vi_readkey();
        if (nk >= 32 && nk < 127 && vi_cx < vi_ll[vi_cy]) {
            vi_ld[vi_cy][vi_cx] = nk;
            vi_dirty = 1;
        }
    } else if (k == 'J') {
        if (vi_cy < vi_nl - 1) {
            int cl = vi_ll[vi_cy];
            int nl2 = vi_ll[vi_cy + 1];
            vi_lensure(vi_cy, cl + 1 + nl2);
            char * cur = vi_ld[vi_cy];
            char * nxt = vi_ld[vi_cy + 1];
            cur[cl] = ' ';
            memcpy(cur + cl + 1, nxt, nl2);
            cur[cl + 1 + nl2] = 0;
            vi_ll[vi_cy] = cl + 1 + nl2;
            vi_ldel(vi_cy + 1);
            vi_dirty = 1;
        }
    } else if (k == 'Z') {
        vi_pend = 'Z';
    } else if (k == ':') {
        vi_mode = VI_EX;
        vi_ex_pr = ':';
        vi_ex_len = 0;
        vi_ex_buf[0] = 0;
    } else if (k == '/') {
        vi_mode = VI_EX;
        vi_ex_pr = '/';
        vi_ex_len = 0;
        vi_ex_buf[0] = 0;
    } else if (k == 'n') {
        vi_search();
    } else if (k == VK_PGUP) {
        vi_cy = vi_cy - (vi_rows - 1) / 2;
        vi_clamp();
    } else if (k == VK_PGDN) {
        vi_cy = vi_cy + (vi_rows - 1) / 2;
        vi_clamp();
    } else if (k == 3) {
        vi_pend = 0;
    }
}

// --- vi insert mode ---

static void vi_insert(int k) {
    if (k == 27 || k == 3) {
        vi_mode = VI_NORMAL;
        if (vi_cx > 0) vi_cx--;
        vi_clamp();
        return;
    }
    if (k == VK_LEFT) {
        if (vi_cx > 0) vi_cx--;
    } else if (k == VK_RIGHT) {
        if (vi_cx < vi_ll[vi_cy]) vi_cx++;
    } else if (k == VK_UP) {
        if (vi_cy > 0) vi_cy--;
        if (vi_cx > vi_ll[vi_cy]) vi_cx = vi_ll[vi_cy];
    } else if (k == VK_DOWN) {
        if (vi_cy < vi_nl - 1) vi_cy++;
        if (vi_cx > vi_ll[vi_cy]) vi_cx = vi_ll[vi_cy];
    } else if (k == VK_HOME) {
        vi_cx = 0;
    } else if (k == VK_END) {
        vi_cx = vi_ll[vi_cy];
    } else if (k == 127 || k == 8) {
        if (vi_cx > 0) {
            char * line = vi_ld[vi_cy];
            memmove(line + vi_cx - 1, line + vi_cx,
                    vi_ll[vi_cy] - vi_cx + 1);
            vi_cx--;
            vi_ll[vi_cy]--;
            vi_dirty = 1;
        } else if (vi_cy > 0) {
            int pl = vi_ll[vi_cy - 1];
            int cl = vi_ll[vi_cy];
            vi_lensure(vi_cy - 1, pl + cl);
            char * prev = vi_ld[vi_cy - 1];
            char * cur = vi_ld[vi_cy];
            memcpy(prev + pl, cur, cl);
            prev[pl + cl] = 0;
            vi_ll[vi_cy - 1] = pl + cl;
            vi_ldel(vi_cy);
            vi_cy--;
            vi_cx = pl;
            vi_dirty = 1;
        }
    } else if (k == VK_DEL) {
        if (vi_cx < vi_ll[vi_cy]) {
            char * line = vi_ld[vi_cy];
            memmove(line + vi_cx, line + vi_cx + 1,
                    vi_ll[vi_cy] - vi_cx);
            vi_ll[vi_cy]--;
            vi_dirty = 1;
        }
    } else if (k == 10 || k == 13) {
        char * line = vi_ld[vi_cy];
        int rest = vi_ll[vi_cy] - vi_cx;
        vi_ladd(vi_cy + 1, line + vi_cx, rest);
        vi_ld[vi_cy][vi_cx] = 0;
        vi_ll[vi_cy] = vi_cx;
        vi_cy++;
        vi_cx = 0;
        vi_dirty = 1;
    } else if (k == 9 || (k >= 32 && k < 127)) {
        vi_lensure(vi_cy, vi_ll[vi_cy] + 1);
        char * line = vi_ld[vi_cy];
        memmove(line + vi_cx + 1, line + vi_cx,
                vi_ll[vi_cy] - vi_cx + 1);
        line[vi_cx] = k;
        vi_cx++;
        vi_ll[vi_cy]++;
        vi_dirty = 1;
    }
}

// --- vi ex mode ---

static void vi_ex_sub(char * cmd) {
    // parse :s/pat/rep/[gi]  :%s/pat/rep/[gi]  :N,Ms/pat/rep/[gi]
    int from = vi_cy;
    int to = vi_cy;
    char * p = cmd;
    // parse optional range
    if (*p == '%') {
        from = 0;
        to = vi_nl - 1;
        p++;
    } else if (*p >= '0' && *p <= '9') {
        from = 0;
        while (*p >= '0' && *p <= '9') {
            from = from * 10 + (*p - '0'); p++;
        }
        from--; // 1-based to 0-based
        to = from;
        if (*p == ',') {
            p++;
            if (*p == '$') {
                to = vi_nl - 1; p++;
            } else {
                to = 0;
                while (*p >= '0' && *p <= '9') {
                    to = to * 10 + (*p - '0'); p++;
                }
                to--; // 1-based to 0-based
            }
        }
    }
    if (*p != 's') {
        strcpy(vi_msg, "Unknown command");
        return;
    }
    p++; // skip 's'
    if (*p == 0) { strcpy(vi_msg, "No pattern"); return; }
    char delim = *p; p++; // usually '/'
    // extract pattern
    char pat[256];
    int pi = 0;
    while (*p && *p != delim && pi < 255) {
        pat[pi] = *p; pi++; p++;
    }
    pat[pi] = 0;
    if (*p == delim) { p++; }
    // extract replacement
    char rep[256];
    int ri = 0;
    while (*p && *p != delim && ri < 255) {
        rep[ri] = *p; ri++; p++;
    }
    rep[ri] = 0;
    if (*p == delim) { p++; }
    // parse flags
    int global = 0;
    int icase = 0;
    while (*p) {
        if (*p == 'g') { global = 1; }
        if (*p == 'i') { icase = 1; }
        p++;
    }
    // clamp range
    if (from < 0) { from = 0; }
    if (to >= vi_nl) { to = vi_nl - 1; }
    if (from > to) { strcpy(vi_msg, "Invalid range"); return; }
    // compile pattern (with case folding if needed)
    char cpat[256];
    if (icase) {
        int k = 0;
        while (pat[k]) { cpat[k] = tolower(pat[k]); k++; }
        cpat[k] = 0;
    } else {
        strcpy(cpat, pat);
    }
    if (!re_compile(cpat)) {
        strcpy(vi_msg, "Bad pattern");
        return;
    }
    int total = 0;
    int rlen = strlen(rep);
    int li = from;
    while (li <= to) {
        char * line = vi_ld[li];
        int llen = vi_ll[li];
        char out[8192];
        int oi = 0;
        int pos = 0;
        int did_one = 0;
        while (pos < llen) {
            int can = global || !did_one;
            if (can) {
                // for icase, build a lowered copy of remaining text
                char lbuf[8192];
                char * match_text = line + pos;
                if (icase) {
                    int k = 0;
                    while (k < llen - pos) {
                        lbuf[k] = tolower(line[pos + k]); k++;
                    }
                    lbuf[k] = 0;
                    match_text = lbuf;
                }
                int ml = 0;
                int mi = re_match(match_text, &ml);
                if (mi >= 0) {
                    // copy chars before match
                    memcpy(out + oi, line + pos, mi);
                    oi = oi + mi;
                    // copy replacement
                    memcpy(out + oi, rep, rlen);
                    oi = oi + rlen;
                    pos = pos + mi + (ml > 0 ? ml : 1);
                    did_one = 1;
                    total++;
                } else {
                    out[oi] = line[pos]; oi++; pos++;
                }
            } else {
                out[oi] = line[pos]; oi++; pos++;
            }
        }
        if (did_one) {
            vi_lensure(li, oi);
            memcpy(vi_ld[li], out, oi);
            vi_ld[li][oi] = 0;
            vi_ll[li] = oi;
            vi_dirty = 1;
        }
        li++;
    }
    // build status message
    char nb[16];
    cx_itoa(total, nb);
    strcpy(vi_msg, nb);
    strcat(vi_msg, " substitution");
    if (total != 1) { strcat(vi_msg, "s"); }
}

static void vi_ex_exec(void) {
    vi_ex_buf[vi_ex_len] = 0;
    if (vi_ex_pr == '/') {
        if (vi_ex_len > 0) { strcpy(vi_srch, vi_ex_buf); }
        vi_mode = VI_NORMAL;
        vi_search();
        return;
    }
    char * cmd = vi_ex_buf;
    while (*cmd == ' ') cmd++;
    if (strcmp(cmd, "q") == 0) {
        if (vi_dirty) {
            strcpy(vi_msg, "Unsaved changes (use :q!)");
        } else {
            vi_run = 0;
        }
    } else if (strcmp(cmd, "q!") == 0) {
        vi_run = 0;
    } else if (strcmp(cmd, "w") == 0) {
        vi_save();
    } else if (strcmp(cmd, "wq") == 0 || strcmp(cmd, "x") == 0) {
        if (vi_save() == 0) vi_run = 0;
    } else if (cmd[0] == 'w' && cmd[1] == ' ') {
        char * fn = cmd + 2;
        while (*fn == ' ') fn++;
        if (*fn) {
            strcpy(vi_file, fn);
            vi_save();
        }
    } else if (cmd[0] >= '0' && cmd[0] <= '9') {
        // check if pure number (line jump) or range/substitute
        char * p = cmd;
        while (*p >= '0' && *p <= '9') { p++; }
        if (*p == 0) {
            // :<number> — jump to line
            int ln = 0;
            p = cmd;
            while (*p >= '0' && *p <= '9') {
                ln = ln * 10 + (*p - '0'); p++;
            }
            ln--; // 1-based to 0-based
            if (ln < 0) { ln = 0; }
            if (ln >= vi_nl) { ln = vi_nl - 1; }
            vi_cy = ln;
            vi_cx = 0;
            if (vi_cy < vi_top) { vi_top = vi_cy; }
            if (vi_cy >= vi_top + vi_rows - 1) {
                vi_top = vi_cy - vi_rows / 2;
                if (vi_top < 0) { vi_top = 0; }
            }
        } else {
            // :N,Ms/...  or :Ns/...
            vi_ex_sub(cmd);
        }
    } else if (cmd[0] == '$') {
        // :$ — jump to last line
        vi_cy = vi_nl - 1;
        vi_cx = 0;
        vi_top = vi_cy - vi_rows / 2;
        if (vi_top < 0) { vi_top = 0; }
    } else if (cmd[0] == 's' || cmd[0] == '%') {
        vi_ex_sub(cmd);
    } else {
        strcpy(vi_msg, "Unknown command");
    }
    vi_mode = VI_NORMAL;
}

static void vi_ex_key(int k) {
    if (k == 27 || k == 3) {
        vi_mode = VI_NORMAL;
    } else if (k == 10 || k == 13) {
        vi_ex_exec();
    } else if (k == 127 || k == 8) {
        if (vi_ex_len > 0) {
            vi_ex_len--;
            vi_ex_buf[vi_ex_len] = 0;
        } else {
            vi_mode = VI_NORMAL;
        }
    } else if (k >= 32 && k < 127 && vi_ex_len < 254) {
        vi_ex_buf[vi_ex_len] = k;
        vi_ex_len++;
        vi_ex_buf[vi_ex_len] = 0;
    }
}

// --- vi entry point ---

static int cmd_vi(int argc, char ** argv) {
    vi_nl = 0;
    vi_cx = 0; vi_cy = 0; vi_top = 0; vi_left = 0;
    vi_mode = VI_NORMAL; vi_dirty = 0; vi_run = 1;
    vi_pend = 0; vi_pb = -1;
    vi_file[0] = 0; vi_msg[0] = 0; vi_srch[0] = 0;
    vi_sn = 0;
    if (argc > 1) { vi_load(argv[1]); }
    else { vi_ladd(0, "", 0); }
    vi_getsize();
    termraw(0, 1);
    vi_sn = 0;
    vi_sesc("[?1049h");
    vi_sflush();
    while (vi_run) {
        vi_render();
        int k = vi_readkey();
        if (k == -1) { vi_getsize(); continue; }
        if (vi_mode == VI_NORMAL) { vi_normal(k); }
        else if (vi_mode == VI_INSERT) { vi_insert(k); }
        else if (vi_mode == VI_EX) { vi_ex_key(k); }
    }
    vi_sn = 0;
    vi_sesc("[?1049l");
    vi_sflush();
    termraw(0, 0);
    vi_lfree();
    return 0;
}

static void cx_find(char * argv0) {
    char buf[4096];
    cx_path[0] = 0;
    // try directory of argv[0] (works for native builds: build/toys -> build/cx)
    char * sl = strrchr(argv0, '/');
    if (sl) {
        int dlen = sl - argv0;
        memcpy(buf, argv0, dlen);
        strcpy(buf + dlen, "/cx");
        if (access(buf, 1) == 0) { strcpy(cx_path, buf); return; }
    }
    // try build/cx relative to cwd
    if (access("build/cx", 1) == 0) {
        char cwd[4096];
        if (getcwd(cwd, 4096)) {
            strcpy(cx_path, cwd);
            strcat(cx_path, "/build/cx");
            return;
        }
    }
    // try cx in PATH via which-like search
    char * path = getenv("PATH");
    if (path) {
        int pi = 0;
        while (path[pi]) {
            int start = pi;
            while (path[pi] && path[pi] != ':') { pi++; }
            int plen = pi - start;
            if (plen > 0 && plen < 4080) {
                memcpy(buf, path + start, plen);
                strcpy(buf + plen, "/cx");
                if (access(buf, 1) == 0) { strcpy(cx_path, buf); return; }
            }
            if (path[pi] == ':') { pi++; }
        }
    }
}

static int cmd_cx(int argc, char ** argv) {
    if (argc < 2) {
        cx_err("usage: cx FILE.c [ARGS...]\n");
        return 1;
    }
    if (cx_path[0] == 0) {
        cx_err("cx: cannot find cx binary\n");
        return 1;
    }
    struct sb cmd;
    memset(&cmd, 0, sizeof(struct sb));
    sb_puts(&cmd, cx_path);
    int i = 1;
    while (i < argc) {
        sb_putc(&cmd, ' ');
        sb_puts(&cmd, argv[i]);
        i++;
    }
    int rc = system(cmd.data ? cmd.data : "");
    sb_free(&cmd);
    return rc / 256;
}

static int cmd_help(int argc, char ** argv) {
    int i = 0;
    while (i < ncmds) {
        cx_err(cmds[i].help);
        cx_err("\n");
        i++;
    }
    return 0;
}

static int cmd_exit(int argc, char ** argv) {
    int code = 0;
    if (argc > 1) { code = atoi(argv[1]); }
    exit(code);
    return 0;
}

static int cmd_sh(int argc, char ** argv) {
    int fd = 0;
    if (argc > 1) {
        if (strcmp(argv[1], "-c") == 0) {
            if (argc > 2) {
                sh_tokenize(argv[2]);
                sh_run_tokens(0, sh_tok_count);
                return sh_last_rc;
            }
            return 0;
        }
        fd = open(argv[1], 0);
        if (fd < 0) {
            cx_err("sh: cannot open ");
            cx_err(argv[1]);
            cx_err("\n");
            return 1;
        }
    }
    int interactive = (fd == 0 && isatty(0));
    char line[4096];
    if (interactive) {
        char prompt[512];
        int n;
        while (1) {
            sh_expand_prompt(prompt, 512);
            n = sh_readline(line, 4096, prompt);
            if (n < 0) { break; }
            if (n > 0) {
                sh_tokenize(line);
                sh_run_tokens(0, sh_tok_count);
            }
        }
    } else {
        int n = cx_getline(fd, line, 4096);
        while (n > 0) {
            sh_tokenize(line);
            sh_run_tokens(0, sh_tok_count);
            n = cx_getline(fd, line, 4096);
        }
        if (fd > 0) {
            close(fd);
        }
    }
    return sh_last_rc;
}

static void cmd_reg(char * name, cmd_fn fn, char * help) {
    cmds[ncmds].name = name;
    cmds[ncmds].fn = fn;
    cmds[ncmds].help = help;
    ncmds++;
}

static void setup(void) {
    cmd_reg("true", cmd_true, "true");
    cmd_reg("false", cmd_false, "false");
    cmd_reg("echo", cmd_echo, "echo [-n] [ARGS...]");
    cmd_reg("yes", cmd_yes, "yes [STRING]");
    cmd_reg("basename", cmd_basename, "basename PATH [SUFFIX]");
    cmd_reg("dirname", cmd_dirname, "dirname PATH");
    cmd_reg("seq", cmd_seq, "seq [FIRST] [INCR] LAST");
    cmd_reg("cat", cmd_cat, "cat [FILE...]");
    cmd_reg("head", cmd_head, "head [-n N] [FILE...]");
    cmd_reg("tail", cmd_tail, "tail [-n N] [FILE...]");
    cmd_reg("wc", cmd_wc, "wc [-lwm] [FILE...]");
    cmd_reg("tee", cmd_tee, "tee [-a] [FILE...]");
    cmd_reg("rev", cmd_rev, "rev [FILE...]");
    cmd_reg("tac", cmd_tac, "tac [FILE...]");
    cmd_reg("nl", cmd_nl, "nl [FILE...]");
    cmd_reg("fold", cmd_fold, "fold [-w WIDTH] [FILE...]");
    cmd_reg("expand", cmd_expand, "expand [-t TAB] [FILE...]");
    cmd_reg("paste", cmd_paste, "paste [-d DELIM] [FILE...]");
    cmd_reg("tr", cmd_tr, "tr SET1 SET2");
    cmd_reg("cut", cmd_cut, "cut -d DELIM -f FIELD [FILE...]");
    cmd_reg("grep", cmd_grep, "grep [-ivcn] REGEX [FILE...] (.^$*+?[a-z]\\d\\w\\s)");
    cmd_reg("sed", cmd_sed, "sed [-n] [-e CMD] [ADDR[,ADDR]]s/RE/REPL/[g]|d|p|q");
    cmd_reg("uniq", cmd_uniq, "uniq [-c] [FILE...]");
    cmd_reg("sort", cmd_sort, "sort [-r] [FILE...]");
    cmd_reg("printf", cmd_printf, "printf FORMAT [ARGS...]");
    cmd_reg("pwd", cmd_pwd, "pwd");
    cmd_reg("touch", cmd_touch, "touch FILE...");
    cmd_reg("mkdir", cmd_mkdir, "mkdir [-p] DIR...");
    cmd_reg("rmdir", cmd_rmdir, "rmdir DIR...");
    cmd_reg("rm", cmd_rm, "rm [-rf] FILE...");
    cmd_reg("cp", cmd_cp, "cp [-r] SRC DST");
    cmd_reg("mv", cmd_mv, "mv SRC DST");
    cmd_reg("ln", cmd_ln, "ln [-s] TARGET LINK");
    cmd_reg("chmod", cmd_chmod, "chmod [ugoa][+-=][rwx] | OCTAL FILE...");
    cmd_reg("cd", cmd_cd, "cd [DIR]");
    cmd_reg("env", cmd_env, "env [NAME=VALUE...]");
    cmd_reg("install", cmd_install, "install DIR");
    cmd_reg("ls", cmd_ls, "ls [-lah] [DIR...]");
    cmd_reg("find", cmd_find, "find [DIR...] [-name PAT] [-type f|d]");
    cmd_reg("xargs", cmd_xargs, "xargs [CMD [ARGS...]]");
    cmd_reg("test", cmd_test, "test EXPRESSION");
    cmd_reg("[", cmd_test, "[ EXPRESSION ]");
    cmd_reg("which", cmd_which, "which NAME");
    cmd_reg("type", cmd_type, "type NAME...");
    cmd_reg("date", cmd_date, "date");
    cmd_reg("sleep", cmd_sleep, "sleep SECONDS");
    cmd_reg("kill", cmd_kill, "kill [-SIG] PID");
    cmd_reg("ps", cmd_ps, "ps");
    cmd_reg("sh", cmd_sh, "sh [-c CMD] [SCRIPT] PS1='\\w\\$ ' \\w \\W \\u \\h");
    cmd_reg("vi", cmd_vi, "vi [FILE] :N :%s/re/rep/[gi] :N,Ms///[gi]");
    cmd_reg("cx", cmd_cx, "cx FILE.c [ARGS...]");
    cmd_reg("help", cmd_help, "help");
    cmd_reg("exit", cmd_exit, "exit [CODE]");
}

static int dispatch(char * name, int argc, char ** argv) {
    cmd_fn fn = 0;
    char * help = 0;
    int i = 0;
    while (i < ncmds && !fn) {
        if (strcmp(name, cmds[i].name) == 0) {
            fn = cmds[i].fn;
            help = cmds[i].help;
        } else {
            i++;
        }
    }
    if (!fn) {
        cx_err("toys: unknown command: ");
        cx_err(name);
        cx_err("\n");
        return 1;
    }
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        cx_err("usage: ");
        cx_err(help);
        cx_err("\n");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--") == 0) {
        return fn(argc - 1, argv + 1);
    }
    return fn(argc, argv);
}

int main(int argc, char ** argv) {
    setup();
    cx_find(argv[0]);
    if (argc < 2) {
        return cmd_sh(1, argv);
    }
    return dispatch(argv[1], argc - 1, argv + 1);
}
