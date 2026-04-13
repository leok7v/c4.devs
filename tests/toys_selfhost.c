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
    printf("=== Stage 5 self-hosting ===\n");
    system("printf 'echo hello from sh\\n' > "
           "build/selfhost.sh");
    check("printf 'source build/selfhost.sh\\n' | "
          "./build/cx toys.c sh",
          "hello from sh\n");
    system("printf 'echo line1\\necho line2\\n"
           "echo line3\\n' > build/selfhost.sh");
    check("printf 'source build/selfhost.sh\\n' | "
          "./build/cx toys.c sh",
          "line1\nline2\nline3\n");
    system("printf 'X=fromfile\\necho got=$X\\n' > "
           "build/selfhost.sh");
    check("printf 'source build/selfhost.sh; "
          "echo parent=$X\\n' | ./build/cx toys.c sh",
          "got=fromfile\nparent=fromfile\n");
    check("printf 'seq 5 | sort -r | head -n 3\\n' | "
          "./build/cx toys.c sh",
          "5\n4\n3\n");
    system("printf 'apple pie\\nbanana\\n"
           "cherry pie\\nplum\\n' > build/sh_known.txt");
    check("printf 'cat build/sh_known.txt | "
          "grep pie | wc\\n' | ./build/cx toys.c sh",
          "2 4 21\n");
    system("rm -f build/sh_known.txt");
    system("printf 'echo a\\necho b\\necho c\\n' > "
           "build/selfhost.sh");
    check("printf 'source build/selfhost.sh | wc\\n' | "
          "./build/cx toys.c sh",
          "3 3 6\n");
    system("printf 'true\\n' > build/selfhost.sh");
    check("printf 'source build/selfhost.sh && "
          "echo ok\\n' | ./build/cx toys.c sh",
          "ok\n");
    system("printf 'false\\n' > build/selfhost.sh");
    check("printf 'source build/selfhost.sh || "
          "echo recovered\\n' | ./build/cx toys.c sh",
          "recovered\n");
    check("printf '. build/selfhost.sh || "
          "echo dot works\\n' | ./build/cx toys.c sh",
          "dot works\n");
    check("printf 'echo nested via spawn:; "
          "./build/cx toys.c echo deep\\n' | "
          "./build/cx toys.c sh",
          "nested via spawn:\ndeep\n");
    system("rm -f build/selfhost.sh");
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
