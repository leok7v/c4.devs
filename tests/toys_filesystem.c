#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pass = 0;
int fail = 0;

int check(char * cmd, char * expect) {
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
    }
    if (!ok) {
        printf("FAIL: %s\n", cmd);
        printf("  want: [%s]\n", expect);
        printf("  got:  [%s]\n", buf);
        fail++;
    }
    return ok;
}

int check_rc(char * cmd, int expect) {
    int ok = 0;
    int rc = system(cmd);
    if (rc == expect) {
        ok = 1;
    }
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
    printf("=== Stage 4: filesystem ===\n");
    system("rm -rf build/fstest");
    check_rc("./build/cx toys.c mkdir build/fstest", 0);
    check_rc("./build/cx toys.c touch build/fstest/a", 0);
    check_rc("./build/cx toys.c touch build/fstest/b", 0);
    check_rc("./build/cx toys.c touch build/fstest/c", 0);
    check("./build/cx toys.c ls build/fstest",
          "a\nb\nc\n");
    check_rc("./build/cx toys.c cp build/fstest/a "
             "build/fstest/d", 0);
    check("./build/cx toys.c ls build/fstest",
          "a\nb\nc\nd\n");
    check_rc("./build/cx toys.c mv build/fstest/d "
             "build/fstest/e", 0);
    check("./build/cx toys.c ls build/fstest",
          "a\nb\nc\ne\n");
    check_rc("./build/cx toys.c rm build/fstest/e", 0);
    check("./build/cx toys.c ls build/fstest",
          "a\nb\nc\n");
    check_rc("./build/cx toys.c mkdir -p "
             "build/fstest/x/y/z", 0);
    check("./build/cx toys.c ls build/fstest/x/y",
          "z\n");
    check_rc("./build/cx toys.c rm -r build/fstest/x", 0);
    check("./build/cx toys.c ls build/fstest",
          "a\nb\nc\n");
    check_rc("./build/cx toys.c chmod 644 "
             "build/fstest/a", 0);
    check_rc("./build/cx toys.c rmdir build/fstest", 256);
    check_rc("./build/cx toys.c rm build/fstest/a "
             "build/fstest/b build/fstest/c", 0);
    check_rc("./build/cx toys.c rmdir build/fstest", 0);
    check_rc("./build/cx toys.c ls build/fstest", 256);
    check_rc("./build/cx toys.c mkdir build/fstest", 0);
    system("printf 'hello\\n' > build/fstest/file.txt");
    check_rc("./build/cx toys.c cp build/fstest/file.txt "
             "build/fstest/copy.txt", 0);
    check("./build/cx toys.c cat build/fstest/copy.txt",
          "hello\n");
    check_rc("./build/cx toys.c rm -r build/fstest", 0);
    check_rc("./build/cx toys.c install build/bin", 0);
    check("./build/cx toys.c true && build/bin/echo OK",
          "OK\n");
    check_rc("./build/cx toys.c rm -r build/bin", 0);
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
