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
    printf("=== Stage 6 advanced ===\n");
    system("rm -rf build/advtest");
    system("mkdir -p build/advtest/sub");
    system("touch build/advtest/a.txt build/advtest/b.txt "
           "build/advtest/sub/c.txt");
    check_rc("./build/cx toys.c find build/advtest "
             "-name a.txt", 0);
    check("./build/cx toys.c find build/advtest "
          "-name a.txt",
          "build/advtest/a.txt\n");
    check("./build/cx toys.c find build/advtest "
          "-name c.txt",
          "build/advtest/sub/c.txt\n");
    check("./build/cx toys.c find build/advtest -type d "
          "| sort",
          "build/advtest\nbuild/advtest/sub\n");
    check_rc("./build/cx toys.c test -e "
             "build/advtest/a.txt", 0);
    check_rc("./build/cx toys.c test -e "
             "build/advtest/missing", 256);
    check_rc("./build/cx toys.c test -f "
             "build/advtest/a.txt", 0);
    check_rc("./build/cx toys.c test -f build/advtest",
             256);
    check_rc("./build/cx toys.c test -d build/advtest", 0);
    check_rc("./build/cx toys.c test -d "
             "build/advtest/a.txt", 256);
    check_rc("./build/cx toys.c test foo = foo", 0);
    check_rc("./build/cx toys.c test foo = bar", 256);
    check_rc("./build/cx toys.c test foo != bar", 0);
    check_rc("./build/cx toys.c test 5 -lt 10", 0);
    check_rc("./build/cx toys.c test 10 -lt 5", 256);
    check_rc("./build/cx toys.c test 7 -eq 7", 0);
    check_rc("./build/cx toys.c test 7 -ne 8", 0);
    check_rc("./build/cx toys.c test 10 -ge 10", 0);
    check_rc("./build/cx toys.c test -z ''", 0);
    check_rc("./build/cx toys.c test -n hi", 0);
    check_rc("./build/cx toys.c '[' -d build/advtest ']'",
             0);
    check_rc("./build/cx toys.c '[' -f build/advtest ']'",
             256);
    check("printf 'one two three\\n' | "
          "./build/cx toys.c xargs ./build/cx toys.c echo",
          "one two three\n");
    check("printf 'a\\nb\\nc\\n' | "
          "./build/cx toys.c xargs ./build/cx toys.c echo",
          "a b c\n");
    check_rc("./build/cx toys.c which sh > /dev/null", 0);
    check_rc("./build/cx toys.c which "
             "nonexistent_cmd_xyz 2>/dev/null", 256);
    system("rm -rf build/advtest");
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
