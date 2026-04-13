#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    int errors;
    int line1;
    int line2;
    errors = 0;

    // Test 1: assert(true) should pass through
    printf("test 1: assert(1) ... ");
    assert(1);
    printf("ok\n");

    // Test 2: assert with expression
    printf("test 2: assert(2 + 2 == 4) ... ");
    assert(2 + 2 == 4);
    printf("ok\n");

    // Test 3: assert with pointer
    printf("test 3: assert(non-null pointer) ... ");
    assert("hello");
    printf("ok\n");

    // Test 4: __LINE__ advances between uses
    printf("test 4: __LINE__ advances ... ");
    line1 = __LINE__;
    line2 = __LINE__;
    if (line2 != line1 + 1) {
        printf("FAIL: expected %d, got %d\n", line1 + 1, line2);
        errors++;
    } else {
        printf("ok (lines %d, %d)\n", line1, line2);
    }

    // Test 5: __FILE__ returns filename string
    printf("test 5: __FILE__ ... ");
    if (!__FILE__) {
        printf("FAIL: __FILE__ is null\n");
        errors++;
    } else {
        printf("ok (%s)\n", __FILE__);
    }

    // Test 6: assert(false) via subprocess — should exit non-zero
    printf("test 6: assert(0) in subprocess ... ");
    {
        char *fn;
        int fd;
        int rc;
        fn = "build/assert_fail_test.c";
        fd = open(fn, 577, 420);
        if (fd < 0) { fd = open(fn, 1537, 420); }
        if (fd < 0) {
            printf("FAIL: cannot create test file\n");
            errors++;
        } else {
            write(fd, "int main() { assert(0); return 0; }\n", 36);
            close(fd);
            rc = system("./build/cx build/assert_fail_test.c");
            if (rc == 0) {
                printf("FAIL: assert(0) should have exited non-zero\n");
                errors++;
            } else {
                printf("ok (exit %d)\n", rc);
            }
        }
    }

    if (errors == 0) {
        printf("All assertion tests passed.\n");
    } else {
        printf("%d assertion test(s) failed.\n", errors);
    }
    return errors;
}
