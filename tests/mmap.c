#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    char* filename = "build/mmap_test.dat";
    char* test_data = "This is a test of the memmap intrinsics.";
    int64_t data_len = strlen(test_data);
    int passed = 1;
    printf("--- Testing mmap Intrinsics ---\n");
    // 1. Test memwrite
    printf("Testing memwrite...\n");
    bool write_success = memwrite(filename, test_data, data_len);
    if (write_success) {
        printf("memwrite successful.\n");
    } else {
        printf("memwrite failed.\n");
        passed = 0;
    }
    // 2. Test memread
    if (passed) {
        printf("Testing memread...\n");
        char* read_data = memread(filename);
        if (read_data) {
            printf("memread successful.\n");
            // 3. Verify content
            if (strncmp(test_data, read_data, data_len) == 0) {
                printf("Data verification successful.\n");
            } else {
                printf("Data verification FAILED.\n");
                passed = 0;
            }
            // 4. Test memclose
            printf("Testing memclose...\n");
            memclose(read_data);
            printf("memclose called.\n");
        } else {
            printf("memread failed.\n");
            passed = 0;
        }
    }
    if (passed) {
        printf("mmap intrinsics test PASSED.\n");
        return 0;
    } else {
        printf("mmap intrinsics test FAILED.\n");
        return -1;
    }
}
