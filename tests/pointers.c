#include <stdio.h>

struct Point { int x; int y; };

int main() {
    int pass = 0;
    // Test 1: basic int pointer
    int x = 42;
    int *p = &x;
    if (*p != 42) { printf("FAIL 1: *p=%d\n", (int)*p); return 1; }
    printf("Test 1 PASS: int pointer *p=%d\n", (int)*p);
    pass++;
    // Test 2: store through pointer
    *p = 99;
    if (x != 99) { printf("FAIL 2: x=%d\n", (int)x); return 1; }
    printf("Test 2 PASS: store through pointer x=%d\n", (int)x);
    pass++;
    // Test 3: struct pointer with arrow
    struct Point pt;
    struct Point *pp = &pt;
    pp->x = 10;
    pp->y = 20;
    if (pp->x != 10 || pp->y != 20) {
        printf("FAIL 3: pp->x=%d pp->y=%d\n", (int)pp->x, (int)pp->y);
        return 1;
    }
    printf("Test 3 PASS: struct pointer pp->x=%d pp->y=%d\n",
           (int)pp->x, (int)pp->y);
    pass++;
    // Test 4: pointer arithmetic on int array
    int arr[4];
    arr[0] = 100; arr[1] = 200; arr[2] = 300; arr[3] = 400;
    int *ip = arr;
    if (*ip != 100) { printf("FAIL 4a\n"); return 1; }
    ip++;
    if (*ip != 200) { printf("FAIL 4b\n"); return 1; }
    ip = ip + 2;
    if (*ip != 400) { printf("FAIL 4c\n"); return 1; }
    printf("Test 4 PASS: int pointer arithmetic\n");
    pass++;
    // Test 5: struct pointer arithmetic
    struct Point pts[3];
    pts[0].x = 1; pts[0].y = 2;
    pts[1].x = 3; pts[1].y = 4;
    pts[2].x = 5; pts[2].y = 6;
    struct Point *sp = pts;
    sp++;
    if (sp->x != 3 || sp->y != 4) { printf("FAIL 5\n"); return 1; }
    printf("Test 5 PASS: struct pointer arithmetic sp->x=%d\n", (int)sp->x);
    pass++;
    // Test 6: pointer subtraction
    sp = pts + 2;
    int diff = sp - pts;
    if (diff != 2) { printf("FAIL 6: diff=%d\n", (int)diff); return 1; }
    printf("Test 6 PASS: pointer subtraction diff=%d\n", (int)diff);
    pass++;
    printf("All pointer tests passed! (%d tests)\n", pass);
    return 0;
}
