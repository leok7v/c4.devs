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
    int lines = 0;
    int words = 0;
    int bytes = 0;
    int n = read(fd, buf, 4096);
    while (n > 0) {
        bytes = bytes + n;
        int j = 0;
        while (j < n) {
            if (buf[j] == '\n') {
                lines++;
            }
            if (isspace(buf[j])) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
            j++;
        }
        n = read(fd, buf, 4096);
    }
    *wl = *wl + lines;
    *ww = *ww + words;
    *wb = *wb + bytes;
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

typedef int (*cmd_fn)(int, char **);

struct cmd {
    char * name;
    cmd_fn fn;
};

struct cmd cmds[64];
int ncmds = 0;

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
