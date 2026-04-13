#include <stdio.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SQUARE(x) ((x) * (x))
#define ADD(x, y) ((x) + (y))
#define TRIPLE(a, b, c) ((a) + (b) + (c))

int main() {
    int pass = 0;
    int total = 0;
    printf("Testing function-like macros\n");
    total++;
    if (MAX(3, 5) == 5) {
        printf("PASS: MAX(3, 5) = 5\n");
        pass++;
    } else {
        printf("FAIL: MAX(3, 5) = %d\n", MAX(3, 5));
    }
    total++;
    if (MIN(3, 5) == 3) {
        printf("PASS: MIN(3, 5) = 3\n");
        pass++;
    } else {
        printf("FAIL: MIN(3, 5) = %d\n", MIN(3, 5));
    }
    total++;
    if (SQUARE(4) == 16) {
        printf("PASS: SQUARE(4) = 16\n");
        pass++;
    } else {
        printf("FAIL: SQUARE(4) = %d\n", SQUARE(4));
    }
    total++;
    if (ADD(10, 20) == 30) {
        printf("PASS: ADD(10, 20) = 30\n");
        pass++;
    } else {
        printf("FAIL: ADD(10, 20) = %d\n", ADD(10, 20));
    }
    total++;
    if (TRIPLE(1, 2, 3) == 6) {
        printf("PASS: TRIPLE(1, 2, 3) = 6\n");
        pass++;
    } else {
        printf("FAIL: TRIPLE(1, 2, 3) = %d\n", TRIPLE(1, 2, 3));
    }
    total++;
    int x = 7;
    int y = 3;
    if (MAX(x, y) == 7) {
        printf("PASS: MAX(x, y) with vars = 7\n");
        pass++;
    } else {
        printf("FAIL: MAX(x, y) = %d\n", MAX(x, y));
    }
    total++;
    if (MAX(x + 1, y * 2) == 8) {
        printf("PASS: MAX(x+1, y*2) = 8\n");
        pass++;
    } else {
        printf("FAIL: MAX(x+1, y*2) = %d\n", MAX(x + 1, y * 2));
    }
    printf("\nResults: %d/%d passed\n", pass, total);
    return pass == total ? 0 : 1;
}
