#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#if os(linux)
#define CX_O_CREAT 64
#define CX_O_TRUNC 512
#endif
#if os(apple)
#define CX_O_CREAT 512
#define CX_O_TRUNC 1024
#endif
#ifndef CX_O_CREAT
#define CX_O_CREAT O_CREAT
#define CX_O_TRUNC O_TRUNC
#endif

static void cx_err(char * s) {
    write(2, s, strlen(s));
}

static void cx_out(char * s, int n) {
    write(1, s, n);
}

static void cx_puts(char * s) {
    write(1, s, strlen(s));
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
    write(fd, buf, len);
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

static char * readall(char * path, int * lenp) {
    int fd = open(path, 0);
    char * buf = 0;
    if (fd < 0) {
        cx_err("readall: cannot open ");
        cx_err(path);
        cx_err("\n");
    }
    if (fd >= 0) {
        int sz = lseek(fd, 0, 2);
        lseek(fd, 0, 0);
        buf = (char*)malloc(sz + 1);
        int n = read(fd, buf, sz);
        buf[n] = 0;
        if (lenp) {
            *lenp = n;
        }
        close(fd);
    }
    return buf;
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
    l->data = (char**)malloc(l->cap * sizeof(int));
    l->lens = (int*)malloc(l->cap * sizeof(int));
}

static void lines_grow(struct lines * l) {
    int nc = l->cap * 2;
    char ** nd = (char**)malloc(nc * sizeof(int));
    int * nl = (int*)malloc(nc * sizeof(int));
    memcpy(nd, l->data, l->count * sizeof(int));
    memcpy(nl, l->lens, l->count * sizeof(int));
    free(l->data);
    free(l->lens);
    l->data = nd;
    l->lens = nl;
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
};

struct cmd cmds[64];
int ncmds = 0;

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
        write(1, base, blen);
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
            write(1, path, last);
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
        write(1, buf, n);
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
            write(1, buf, len);
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
            write(1, l.data[j], l.lens[j]);
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
        fds[nfds] = open(argv[i], 1 | CX_O_CREAT | CX_O_TRUNC, 420);
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
            write(1, buf, n);
            int j = 0;
            while (j < nfds) {
                write(fds[j], buf, n);
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
        write(1, buf, n);
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
            write(1, l.data[i], l.lens[i]);
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
            write(1, num, nlen);
            cx_out("\t", 1);
            write(1, buf, n);
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
                write(1, buf + i, 1);
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
                write(1, buf + i, 1);
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
                    write(1, &delim, 1);
                }
                if (n > 0) {
                    any = 1;
                    if (buf[n - 1] == '\n') {
                        n--;
                    }
                    write(1, buf, n);
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
            write(1, buf, n);
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
            write(1, buf + start, end - start);
            cx_out("\n", 1);
            n = cx_getline(0, buf, 4096);
        }
    }
    return rc;
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
        flags |= grep_parse_flags(argv[i]);
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
        char buf[4096];
        char lbuf[4096];
        int line_no = 0;
        int matches = 0;
        int n = cx_getline(0, buf, 4096);
        while (n > 0) {
            line_no++;
            char * search = buf;
            if (flags & G_ICASE) {
                int k = 0;
                while (k < n) {
                    lbuf[k] = tolower(buf[k]);
                    k++;
                }
                lbuf[n] = 0;
                search = lbuf;
            }
            int found = strstr(search, lpat) != 0;
            int show = (flags & G_INVERT) ? !found : found;
            if (show) {
                matches++;
                if (!(flags & G_COUNT)) {
                    if (flags & G_NUMBER) {
                        cx_putint(1, line_no);
                        cx_out(":", 1);
                    }
                    write(1, buf, n);
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

static int cmd_sed(int argc, char ** argv) {
    int rc = 0;
    if (argc < 2) {
        cx_err("sed: missing expression\n");
        rc = 1;
    }
    char old_pat[256];
    char new_pat[256];
    int global = 0;
    if (!rc) {
        char * expr = argv[1];
        if (expr[0] != 's' || expr[1] != '/') {
            cx_err("sed: only s/old/new/[g] supported\n");
            rc = 1;
        }
        if (!rc) {
            int p = 2;
            int o = 0;
            while (expr[p] && expr[p] != '/' && o < 255) {
                old_pat[o] = expr[p];
                o++;
                p++;
            }
            old_pat[o] = 0;
            if (expr[p] != '/') {
                cx_err("sed: bad expression\n");
                rc = 1;
            }
            if (!rc) {
                p++;
                int q = 0;
                while (expr[p] && expr[p] != '/' && q < 255) {
                    new_pat[q] = expr[p];
                    q++;
                    p++;
                }
                new_pat[q] = 0;
                if (expr[p] == '/') {
                    p++;
                    if (expr[p] == 'g') {
                        global = 1;
                    }
                }
            }
        }
    }
    if (!rc && strlen(old_pat) == 0) {
        cx_err("sed: empty pattern\n");
        rc = 1;
    }
    if (!rc) {
        int old_len = strlen(old_pat);
        int new_len = strlen(new_pat);
        char buf[4096];
        char out[8192];
        int n = cx_getline(0, buf, 4096);
        while (n > 0) {
            int has_nl = n > 0 && buf[n - 1] == '\n';
            int blen = has_nl ? n - 1 : n;
            int oi = 0;
            int p = 0;
            int done_one = 0;
            while (p < blen) {
                int can_match = global || !done_one;
                int fits = p + old_len <= blen;
                if (can_match && fits &&
                    memcmp(buf + p, old_pat, old_len) == 0) {
                    memcpy(out + oi, new_pat, new_len);
                    oi += new_len;
                    p += old_len;
                    done_one = 1;
                } else {
                    out[oi] = buf[p];
                    oi++;
                    p++;
                }
            }
            write(1, out, oi);
            if (has_nl) {
                cx_out("\n", 1);
            }
            n = cx_getline(0, buf, 4096);
        }
    }
    return rc;
}

static void uniq_emit(char * line, int len, int count, int sc) {
    if (count > 0) {
        if (sc) {
            char num[16];
            int nlen = cx_itopad(count, num, 4);
            write(1, num, nlen);
            cx_out(" ", 1);
        }
        write(1, line, len);
        cx_out("\n", 1);
    }
}

static int cmd_uniq(int argc, char ** argv) {
    int rc = 0;
    int show_count = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
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
            write(1, l.data[z], l.lens[z]);
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
                    write(1, fmt + p, 1);
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
                        write(1, argv[ai], 1);
                        ai++;
                    }
                    p++;
                } else if (fmt[p] == '%') {
                    cx_out("%", 1);
                    p++;
                }
            } else {
                write(1, fmt + p, 1);
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
    write(fd, s, strlen(s));
}

static int dir_exists(char * path) {
    int yes = 0;
    struct cx_stat st;
    if (stat(path, &st) == 0) {
        if ((st.mode & S_IFMT) == S_IFDIR) {
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
        int dfd = open(dst, 1 | CX_O_CREAT | CX_O_TRUNC, 420);
        if (dfd < 0) {
            close(sfd);
            rc = -1;
        }
        if (!rc) {
            char buf[4096];
            int n = read(sfd, buf, 4096);
            while (n > 0) {
                write(dfd, buf, n);
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
    if (stat(path, &st) != 0) {
        rc = -1;
    }
    if (!rc && (st.mode & S_IFMT) == S_IFDIR) {
        char * names = (char*)malloc(64 * 256);
        int ncount = 0;
        int dp = opendir(path);
        if (dp == 0) {
            rc = -1;
        }
        if (!rc) {
            char * name = readdir(dp);
            while (name && ncount < 64) {
                if (strcmp(name, ".") != 0 &&
                    strcmp(name, "..") != 0) {
                    int nl = strlen(name);
                    if (nl < 256) {
                        memcpy(names + ncount * 256, name, nl + 1);
                        ncount++;
                    }
                }
                name = readdir(dp);
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
        int fd = open(argv[i], 1 | CX_O_CREAT, 420);
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
        int mode = 0;
        char * s = argv[1];
        while (*s >= '0' && *s <= '7') {
            mode = mode * 8 + (*s - '0');
            s++;
        }
        int j = 2;
        while (j < argc && !rc) {
            if (chmod(argv[j], mode) != 0) {
                cx_err("chmod: failed: ");
                cx_err(argv[j]);
                cx_err("\n");
                rc = 1;
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
            int fd = open(path, 1 | CX_O_CREAT | CX_O_TRUNC, 493);
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

static int cmd_ls(int argc, char ** argv) {
    int rc = 0;
    int show_all = 0;
    int long_fmt = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        char * f = argv[i] + 1;
        while (*f) {
            if (*f == 'a') {
                show_all = 1;
            }
            if (*f == 'l') {
                long_fmt = 1;
            }
            f++;
        }
        i++;
    }
    char * dir = ".";
    if (i < argc) {
        dir = argv[i];
    }
    int dp = opendir(dir);
    if (dp == 0) {
        cx_err("ls: cannot open: ");
        cx_err(dir);
        cx_err("\n");
        rc = 1;
    }
    if (!rc) {
        char * names = (char*)malloc(256 * 256);
        int ncount = 0;
        char * name = readdir(dp);
        while (name && ncount < 256) {
            if (show_all || name[0] != '.') {
                int nl = strlen(name);
                if (nl < 256) {
                    memcpy(names + ncount * 256, name, nl + 1);
                    ncount++;
                }
            }
            name = readdir(dp);
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
                if (stat(path, &st) == 0) {
                    char ch = '-';
                    if ((st.mode & S_IFMT) == S_IFDIR) {
                        ch = 'd';
                    }
                    write(1, &ch, 1);
                    cx_out(" ", 1);
                    cx_putint(1, st.size);
                    cx_out(" ", 1);
                }
            }
            cx_puts(n);
            cx_out("\n", 1);
            j++;
        }
        free(names);
    }
    return rc;
}

static int dispatch(char * name, int argc, char ** argv);

enum {
    SH_WORD = 1, SH_PIPE, SH_REDOUT, SH_REDAPP, SH_REDIN,
    SH_AND, SH_OR, SH_SEMI, SH_BG
};

char sh_tok_buf[16384];
int sh_tok_types[64];
int sh_tok_count = 0;
char sh_var_names[4096];
char sh_var_vals[16384];
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
        sh_var_count++;
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

static void sh_run_tokens(int start, int end);

static cmd_fn sh_find_cmd(char * name) {
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

static int sh_run_external(int argc, char ** argv) {
    char cmdline[4096];
    int cl = 0;
    int j = 0;
    while (j < argc && cl < 4000) {
        if (j > 0) {
            cmdline[cl] = ' ';
            cl++;
        }
        int al = strlen(argv[j]);
        if (cl + al < 4000) {
            memcpy(cmdline + cl, argv[j], al);
            cl += al;
        }
        j++;
    }
    cmdline[cl] = 0;
    int wstatus = system(cmdline);
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
        int flags = 1 | CX_O_CREAT;
        if (append_stdout) {
            flags = flags | O_APPEND;
        } else {
            flags = flags | CX_O_TRUNC;
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
                char * sb = (char*)malloc(16384);
                int * st = (int*)malloc(64 * 8);
                memcpy(sb, sh_tok_buf, 16384);
                memcpy(st, sh_tok_types, 64 * 8);
                int sc = sh_tok_count;
                char line[4096];
                int n = cx_getline(fd, line, 4096);
                while (n > 0) {
                    sh_tokenize(line);
                    sh_run_tokens(0, sh_tok_count);
                    n = cx_getline(fd, line, 4096);
                }
                close(fd);
                memcpy(sh_tok_buf, sb, 16384);
                memcpy(sh_tok_types, st, 64 * 8);
                sh_tok_count = sc;
                free(sb);
                free(st);
                rc = sh_last_rc;
            }
            handled = 1;
        }
        if (!handled && argc > 0) {
            cmd_fn fn = sh_find_cmd(argv[0]);
            if (fn) {
                rc = fn(argc, argv);
            } else {
                rc = sh_run_external(argc, argv);
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
                    1 | CX_O_CREAT | CX_O_TRUNC, 420);
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

static int cmd_sh(int argc, char ** argv) {
    char line[4096];
    int n = cx_getline(0, line, 4096);
    while (n > 0) {
        sh_tokenize(line);
        sh_run_tokens(0, sh_tok_count);
        n = cx_getline(0, line, 4096);
    }
    return sh_last_rc;
}

static void cmd_reg(char * name, cmd_fn fn) {
    cmds[ncmds].name = name;
    cmds[ncmds].fn = fn;
    ncmds++;
}

static void setup(void) {
    cmd_reg("true", cmd_true);
    cmd_reg("false", cmd_false);
    cmd_reg("echo", cmd_echo);
    cmd_reg("yes", cmd_yes);
    cmd_reg("basename", cmd_basename);
    cmd_reg("dirname", cmd_dirname);
    cmd_reg("seq", cmd_seq);
    cmd_reg("cat", cmd_cat);
    cmd_reg("head", cmd_head);
    cmd_reg("tail", cmd_tail);
    cmd_reg("wc", cmd_wc);
    cmd_reg("tee", cmd_tee);
    cmd_reg("rev", cmd_rev);
    cmd_reg("tac", cmd_tac);
    cmd_reg("nl", cmd_nl);
    cmd_reg("fold", cmd_fold);
    cmd_reg("expand", cmd_expand);
    cmd_reg("paste", cmd_paste);
    cmd_reg("tr", cmd_tr);
    cmd_reg("cut", cmd_cut);
    cmd_reg("grep", cmd_grep);
    cmd_reg("sed", cmd_sed);
    cmd_reg("uniq", cmd_uniq);
    cmd_reg("sort", cmd_sort);
    cmd_reg("printf", cmd_printf);
    cmd_reg("pwd", cmd_pwd);
    cmd_reg("touch", cmd_touch);
    cmd_reg("mkdir", cmd_mkdir);
    cmd_reg("rmdir", cmd_rmdir);
    cmd_reg("rm", cmd_rm);
    cmd_reg("cp", cmd_cp);
    cmd_reg("mv", cmd_mv);
    cmd_reg("ln", cmd_ln);
    cmd_reg("chmod", cmd_chmod);
    cmd_reg("cd", cmd_cd);
    cmd_reg("env", cmd_env);
    cmd_reg("install", cmd_install);
    cmd_reg("ls", cmd_ls);
    cmd_reg("sh", cmd_sh);
}

static int dispatch(char * name, int argc, char ** argv) {
    cmd_fn fn = 0;
    int i = 0;
    while (i < ncmds && !fn) {
        if (strcmp(name, cmds[i].name) == 0) {
            fn = cmds[i].fn;
        } else {
            i++;
        }
    }
    if (!fn) {
        cx_err("toys: unknown command: ");
        cx_err(name);
        cx_err("\n");
    }
    int rc = 1;
    if (fn) {
        rc = fn(argc, argv);
    }
    return rc;
}

int main(int argc, char ** argv) {
    int rc = 0;
    setup();
    if (argc < 2) {
        int i = 0;
        while (i < ncmds) {
            cx_puts(cmds[i].name);
            cx_out("\n", 1);
            i++;
        }
        rc = 1;
    }
    if (!rc) {
        rc = dispatch(argv[1], argc - 1, argv + 1);
    }
    return rc;
}
