#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int run_test(char *filename) {
    char *cmd = malloc(256);
    strcpy(cmd, "./build/cx ");
    strcat(cmd, filename);
    printf("\n=== Running Test: %s ===\n", filename);
    int status = system(cmd);
    printf("Result: %d\n", status);
    free(cmd);
    return (status == 0) ? 1 : 0;
}

int main(int argc, char **argv) {
    int passed = 0, total = 0;
    if (argc > 1) {
        int i = 1;
        while (i < argc) {
            total++;
            if (run_test(argv[i])) { passed++; }
            i++;
        }
    } else {
        printf("Discovering tests in test/ directory...\n");
        int fp = popen("ls test/*.c", "r");
        if (!fp) { printf("Failed to list tests\n"); return -1; }
        char *buf = malloc(1024);
        memset(buf, 0, 1024);
        int n = fread(buf, 1, 1023, fp);
        pclose(fp);
        if (n <= 0) { printf("No tests found\n"); return 0; }
        char *ptr = buf;
        char *filename = buf;
        while (*ptr) {
            if (*ptr == '\n') {
                *ptr = 0;
                int len = strlen(filename);
                int is_self = 0;
                if (len >= 7) {
                    char *end = filename + len - 7;
                    if (strcmp(end, "tests.c") == 0) {
                        is_self = (len == 7) ? 1 : (*(end-1) == '/');
                    }
                }
                if (!is_self && len > 0) {
                    total++;
                    if (run_test(filename)) { passed++; }
                }
                filename = ptr + 1;
            }
            ptr++;
        }
    }
    printf("\nExpected failures notes:\n");
    printf("- meta.c prints 'System returned status: 10752' ");
    printf("(exit 42) which is correct behavior for that test.\n");
    printf("\nSUMMARY: %d/%d tests passed.\n", passed, total);
    return passed == total ? 0 : -1;
}
