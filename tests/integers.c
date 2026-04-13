#include <stdio.h>
#include <stdint.h>

int main() {
    int pass = 0;
    // Test 1: int32_t basic
    int32_t a32 = 42;
    if (a32 != 42) { printf("FAIL 1\n"); return 1; }
    printf("Test 1 PASS: int32_t a32=%d\n", a32);
    pass++;
    // Test 2: int64_t basic
    int64_t b64 = 123456789;
    if (b64 != 123456789) { printf("FAIL 2\n"); return 1; }
    printf("Test 2 PASS: int64_t b64=%d\n", (int)b64);
    pass++;
    // Test 3: sizeof types
    if (sizeof(char) != 1 || sizeof(int32_t) != 4 || sizeof(int64_t) != 8) {
        printf("FAIL 3\n"); return 1;
    }
    printf("Test 3 PASS: sizeof char=%d int32=%d int64=%d\n",
           (int)sizeof(char), (int)sizeof(int32_t), (int)sizeof(int64_t));
    pass++;
    // Test 4: arithmetic
    a32 = 100 + 50;
    b64 = a32 * 2;
    if (a32 != 150 || b64 != 300) { printf("FAIL 4\n"); return 1; }
    printf("Test 4 PASS: arithmetic a32=%d b64=%d\n", a32, (int)b64);
    pass++;
    // Test 5: increment/decrement
    a32++; b64--;
    if (a32 != 151 || b64 != 299) { printf("FAIL 5\n"); return 1; }
    printf("Test 5 PASS: inc/dec a32=%d b64=%d\n", a32, (int)b64);
    pass++;
    // Test 6: post-increment returns old value
    int32_t x = 10;
    int32_t y = x++;
    if (x != 11 || y != 10) { printf("FAIL 6\n"); return 1; }
    printf("Test 6 PASS: post-inc x=%d y=%d\n", x, y);
    pass++;
    // Test 7: pointers to sized types
    int32_t *p32 = &a32;
    int64_t *p64 = &b64;
    *p32 = 999; *p64 = 888;
    if (a32 != 999 || b64 != 888) { printf("FAIL 7\n"); return 1; }
    printf("Test 7 PASS: typed pointers a32=%d b64=%d\n", a32, (int)b64);
    pass++;
    // Test 8: char type
    char c = 'Z';
    if (c != 'Z') { printf("FAIL 8\n"); return 1; }
    printf("Test 8 PASS: char c=%c\n", c);
    pass++;
    printf("All integer tests passed! (%d tests)\n", pass);
    return 0;
}
