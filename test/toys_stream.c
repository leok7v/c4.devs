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

void create_test_file(void) {
    system("printf 'line1\\nline2\\nline3\\n"
           "line4\\nline5\\n' > build/test_stream.txt");
}

int main(void) {
    printf("=== Stage 1: stream ===\n");
    create_test_file();
    check("./build/cx toys.c cat build/test_stream.txt",
          "line1\nline2\nline3\nline4\nline5\n");
    check("printf 'hello' | ./build/cx toys.c cat",
          "hello");
    check("./build/cx toys.c head -n 2 "
          "build/test_stream.txt",
          "line1\nline2\n");
    check("./build/cx toys.c tail -n 2 "
          "build/test_stream.txt",
          "line4\nline5\n");
    check("./build/cx toys.c tail -n 1 "
          "build/test_stream.txt",
          "line5\n");
    check("./build/cx toys.c wc "
          "build/test_stream.txt",
          "5 5 30 build/test_stream.txt\n");
    check("printf 'hello' | ./build/cx toys.c wc",
          "0 1 5\n");
    check("printf 'hello\\n' | ./build/cx toys.c rev",
          "olleh\n");
    check("printf 'ab\\ncd\\n' | ./build/cx toys.c rev",
          "ba\ndc\n");
    check("printf 'a\\nb\\nc\\n' | "
          "./build/cx toys.c tac",
          "c\nb\na\n");
    check("printf 'x\\ny\\n' | ./build/cx toys.c nl",
          "     1\tx\n     2\ty\n");
    check("printf 'abcdefgh\\n' | "
          "./build/cx toys.c fold -w 4",
          "abcd\nefgh\n");
    check("printf 'a\\tb\\n' | "
          "./build/cx toys.c expand -t 4",
          "a   b\n");
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
