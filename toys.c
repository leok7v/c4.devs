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

typedef int (*cmd_fn)(int, char **);

struct cmd {
    char * name;
    cmd_fn fn;
};

struct cmd cmds[32];
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
