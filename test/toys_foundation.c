#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pass = 0;
int fail = 0;

int check(char *cmd, char *expect) {
    int ok = 0;
    char buf[4096];
    memset(buf, 0, 4096);
    int fp = popen(cmd, "r");
    if (fp) {
        fread(buf, 1, 4095, fp);
        pclose(fp);
        if (strcmp(buf, expect) == 0) { ok = 1; }
    }
    if (ok) {
        printf("  OK: %s\n", cmd);
        pass++;
    }
    if (!ok) {
        printf("FAIL: %s\n", cmd);
        printf("  want: [%s]\n", expect);
        printf("  got:  [%s]\n", buf);
        fail++;
    }
    return ok;
}
int check_rc(char *cmd, int expect) {
    int ok = 0;
    int rc = system(cmd);
    if (rc == expect) { ok = 1; }
    if (ok) {
        printf("  OK: %s -> %d\n", cmd, rc);
        pass++;
    }
    if (!ok) {
        printf("FAIL: %s -> %d (want %d)\n",
               cmd, rc, expect);
        fail++;
    }
    return ok;
}

int main(void) {
    printf("=== Stage 0: foundation ===\n");
    check_rc("./build/cx toys.c true", 0);
    check_rc("./build/cx toys.c false", 256);
    check("./build/cx toys.c echo hello", "hello\n");
    check("./build/cx toys.c echo -n hi", "hi");
    check("./build/cx toys.c echo a b c", "a b c\n");
    check("./build/cx toys.c echo", "\n");
    check("./build/cx toys.c basename /a/b/c", "c\n");
    check("./build/cx toys.c basename f.c .c", "f\n");
    check("./build/cx toys.c basename /a/b/c .c", "c\n");
    check("./build/cx toys.c basename /", "/\n");
    check("./build/cx toys.c dirname /a/b/c", "/a/b\n");
    check("./build/cx toys.c dirname foo", ".\n");
    check("./build/cx toys.c dirname /foo", "/\n");
    check("./build/cx toys.c seq 3", "1\n2\n3\n");
    check("./build/cx toys.c seq 2 4", "2\n3\n4\n");
    check("./build/cx toys.c seq 0 2 6", "0\n2\n4\n6\n");
    check("./build/cx toys.c seq 5 1", "");
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
