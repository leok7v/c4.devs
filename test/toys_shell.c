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

int main(void) {
    printf("=== Stage 5: shell ===\n");
    check("printf 'echo hello\\n' | "
          "./build/cx toys.c sh",
          "hello\n");
    check("printf 'echo a; echo b\\n' | "
          "./build/cx toys.c sh",
          "a\nb\n");
    check("printf 'true && echo yes\\n' | "
          "./build/cx toys.c sh",
          "yes\n");
    check("printf 'false && echo no\\n' | "
          "./build/cx toys.c sh",
          "");
    check("printf 'false || echo fallback\\n' | "
          "./build/cx toys.c sh",
          "fallback\n");
    check("printf 'true || echo skipped\\n' | "
          "./build/cx toys.c sh",
          "");
    check("printf 'X=world; echo hello $X\\n' | "
          "./build/cx toys.c sh",
          "hello world\n");
    check("printf 'A=1; B=2; echo $A $B\\n' | "
          "./build/cx toys.c sh",
          "1 2\n");
    check("printf 'echo $UNDEFINED end\\n' | "
          "./build/cx toys.c sh",
          " end\n");
    check("printf 'true; echo $?\\n' | "
          "./build/cx toys.c sh",
          "0\n");
    check("printf 'false; echo $?\\n' | "
          "./build/cx toys.c sh",
          "1\n");
    check("printf 'X=hi; X=bye; echo $X\\n' | "
          "./build/cx toys.c sh",
          "bye\n");
    check("printf 'echo a; echo b; echo c\\n' | "
          "./build/cx toys.c sh",
          "a\nb\nc\n");
    check("printf 'true && echo a && echo b\\n' | "
          "./build/cx toys.c sh",
          "a\nb\n");
    check("printf 'echo first\\necho second\\n' | "
          "./build/cx toys.c sh",
          "first\nsecond\n");
    check("printf 'Y=$HOME; echo got=$Y\\n' | "
          "HOME=/x ./build/cx toys.c sh",
          "got=/x\n");
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
