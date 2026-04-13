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
    printf("=== Stage 2: text ===\n");
    check("printf 'c\\na\\nb\\n' | "
          "./build/cx toys.c sort",
          "a\nb\nc\n");
    check("printf 'c\\na\\nb\\n' | "
          "./build/cx toys.c sort -r",
          "c\nb\na\n");
    check("printf 'a\\na\\nb\\n' | "
          "./build/cx toys.c uniq",
          "a\nb\n");
    check("printf 'a\\na\\nb\\n' | "
          "./build/cx toys.c uniq -c",
          "   2 a\n   1 b\n");
    check("echo 'a:b:c' | "
          "./build/cx toys.c cut -d: -f2",
          "b\n");
    check("echo 'a:b:c' | "
          "./build/cx toys.c cut -d: -f1",
          "a\n");
    check("echo 'a:b:c' | "
          "./build/cx toys.c cut -d: -f3",
          "c\n");
    check("echo abc | ./build/cx toys.c tr abc xyz",
          "xyz\n");
    check("echo abc | ./build/cx toys.c tr abc x",
          "xxx\n");
    check("echo 'hello world' | "
          "./build/cx toys.c grep hello",
          "hello world\n");
    check_rc("echo 'hello' | "
             "./build/cx toys.c grep nope", 256);
    check("echo 'Hello' | "
          "./build/cx toys.c grep -i hello",
          "Hello\n");
    check("printf 'a\\nb\\nc\\n' | "
          "./build/cx toys.c grep -v b",
          "a\nc\n");
    check("printf 'a\\nb\\na\\n' | "
          "./build/cx toys.c grep -c a",
          "2\n");
    check("echo 'hello world' | "
          "./build/cx toys.c sed s/hello/bye/",
          "bye world\n");
    check("echo 'foo bar foo' | "
          "./build/cx toys.c sed s/foo/X/g",
          "X bar X\n");
    check("echo 'hello' | "
          "./build/cx toys.c sed s/x/y/",
          "hello\n");
    check("./build/cx toys.c printf 'hello %s\\n' world",
          "hello world\n");
    check("./build/cx toys.c printf '%d + %d = %d\\n' 1 2 3",
          "1 + 2 = 3\n");
    check("./build/cx toys.c printf '%%\\n'",
          "%\n");
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
