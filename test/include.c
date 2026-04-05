#include <stdio.h>
#include "test/include.h"
int check_substr(char *buf, char *sub) {
    char *b = buf;
    while (*b) {
        char *p = b;
        char *s = sub;
        while (*s && *p == *s) { p++; s++; }
        if (*s == 0) { return 1; }
        b++;
    }
    return 0;
}
#ifdef CHECK_FLAG
int main() {
    printf("FLAG=%d\n", get_flag());
    return 0;
}
#else
int main() {
    int passed = 0;
    int total = 0;
    char cmd[512];
    char buf[256];
    char *cx = "./build/cx";
    char *s;
    int len;
    int fp;
    int n;
    /* Test 1: local include with macro */
    total++;
    if (HELPER_VERSION == 42) {
        printf("PASS: HELPER_VERSION is 42\n");
        passed++;
    } else {
        printf("FAIL: HELPER_VERSION is %d, expected 42\n", HELPER_VERSION);
    }
    /* Test 2: function from included file */
    total++;
    if (helper_add(3, 4) == 7) {
        printf("PASS: helper_add(3, 4) == 7\n");
        passed++;
    } else {
        printf("FAIL: helper_add(3, 4) == %d, expected 7\n", helper_add(3, 4));
    }
    /* Test 3: another function */
    total++;
    if (helper_mul(5, 6) == 30) {
        printf("PASS: helper_mul(5, 6) == 30\n");
        passed++;
    } else {
        printf("FAIL: helper_mul(5, 6) == %d, expected 30\n", helper_mul(5, 6));
    }
    /* Test 4: function-like macro from header */
    total++;
    if (SQUARE(7) == 49) {
        printf("PASS: SQUARE(7) == 49\n");
        passed++;
    } else {
        printf("FAIL: SQUARE(7) == %d, expected 49\n", SQUARE(7));
    }
    /* Test 5: verify cx reports correct file:line for error in header */
    total++;
    len = 0;
    s = cx;
    while (*s) { cmd[len++] = *s++; }
    s = " -DTRIGGER_ERROR test/include.c 2>&1";
    while (*s) { cmd[len++] = *s++; }
    cmd[len] = 0;
    fp = popen(cmd, "r");
    if (fp) {
        n = fread(buf, 1, 255, fp);
        pclose(fp);
        buf[n] = 0;
        if (check_substr(buf, "include.h:13:")) {
            printf("PASS: error reports include.h:13\n");
            passed++;
        } else {
            printf("FAIL: expected 'include.h:12:' in output\n");
            printf("  got: %s\n", buf);
        }
    } else {
        printf("FAIL: popen failed\n");
    }
    /* Test 6: verify -D without value works (define to 1) */
    total++;
    len = 0;
    s = cx;
    while (*s) { cmd[len++] = *s++; }
    s = " -DCHECK_FLAG test/include.c 2>&1";
    while (*s) { cmd[len++] = *s++; }
    cmd[len] = 0;
    fp = popen(cmd, "r");
    if (fp) {
        n = fread(buf, 1, 255, fp);
        pclose(fp);
        buf[n] = 0;
        if (check_substr(buf, "FLAG=1")) {
            printf("PASS: -DCHECK_FLAG sets value to 1\n");
            passed++;
        } else {
            printf("FAIL: expected 'FLAG=1' in output\n");
            printf("  got: %s\n", buf);
        }
    } else {
        printf("FAIL: popen failed\n");
    }
    /* Test 7: verify -Dname=value works */
    total++;
    len = 0;
    s = cx;
    while (*s) { cmd[len++] = *s++; }
    s = " -DCHECK_FLAG=42 test/include.c 2>&1";
    while (*s) { cmd[len++] = *s++; }
    cmd[len] = 0;
    fp = popen(cmd, "r");
    if (fp) {
        n = fread(buf, 1, 255, fp);
        pclose(fp);
        buf[n] = 0;
        if (check_substr(buf, "FLAG=42")) {
            printf("PASS: -DCHECK_FLAG=42 works\n");
            passed++;
        } else {
            printf("FAIL: expected 'FLAG=42' in output\n");
            printf("  got: %s\n", buf);
        }
    } else {
        printf("FAIL: popen failed\n");
    }
    printf("%d/%d tests passed\n", passed, total);
    return passed == total ? 0 : 1;
}
#endif
