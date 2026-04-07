#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int run(char *test) {
    int rc = 0;
    char cmd[256];
    strcpy(cmd, "./build/cx ");
    strcat(cmd, test);
    printf("--- %s ---\n", test);
    rc = system(cmd);
    if (rc != 0) {
        printf("FAIL: %s (status %d)\n", test, rc);
    }
    return rc;
}

int main(void) {
    int fail = 0;
    int total = 0;
    total++;
    if (run("test/toys_foundation.c")) { fail++; }
    total++;
    if (run("test/toys_stream.c")) { fail++; }
    total++;
    if (run("test/toys_text.c")) { fail++; }
    printf("\n%d/%d passed\n", total - fail, total);
    return fail ? -1 : 0;
}
