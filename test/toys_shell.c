#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pass = 0;
int fail = 0;

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
    check("printf 'echo \"hello world\"\\n' | "
          "./build/cx toys.c sh",
          "hello world\n");
    check("printf \"echo 'hello world'\\n\" | "
          "./build/cx toys.c sh",
          "hello world\n");
    check("printf 'echo a b   c\\n' | "
          "./build/cx toys.c sh",
          "a b c\n");
    system("rm -f build/sh_redir");
    check_rc("printf 'echo redirected > build/sh_redir\\n'"
             " | ./build/cx toys.c sh", 0);
    check("cat build/sh_redir",
          "redirected\n");
    check_rc("printf 'echo more >> build/sh_redir\\n'"
             " | ./build/cx toys.c sh", 0);
    check("cat build/sh_redir",
          "redirected\nmore\n");
    system("printf 'one\\ntwo\\nthree\\n' "
           "> build/sh_in");
    check("printf 'cat < build/sh_in\\n' | "
          "./build/cx toys.c sh",
          "one\ntwo\nthree\n");
    system("rm -f build/sh_redir build/sh_in");
    check("printf 'echo hello | wc\\n' | "
          "./build/cx toys.c sh",
          "1 1 6\n");
    check("printf 'seq 5 | tac\\n' | "
          "./build/cx toys.c sh",
          "5\n4\n3\n2\n1\n");
    check("printf 'seq 5 | tac | head -n 2\\n' | "
          "./build/cx toys.c sh",
          "5\n4\n");
    check("printf 'seq 20 | grep 1 | sort\\n' | "
          "./build/cx toys.c sh",
          "1\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n");
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}

