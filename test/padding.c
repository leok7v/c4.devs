#include <stdio.h>
#include <stdint.h>

struct Point { int x; int y; };
struct Padded { char c; int i; char d; };
struct Nested { char a; struct Padded inner; };
struct StrictPad { char c; int64_t i; char d; };

int main() {
    int pass = 0;
    // Test 1: struct padding with multiple chars
    struct Padded m;
    m.c = 'A'; m.i = 42; m.d = 'B';
    if (m.c != 'A' || m.i != 42 || m.d != 'B') {
        printf("FAIL 1: c=%d i=%d d=%d\n", m.c, m.i, m.d);
        return 1;
    }
    printf("Test 1 PASS: padding c=%c i=%d d=%c\n", m.c, m.i, m.d);
    pass++;
    // Test 2: nested struct padding
    struct Nested n;
    n.a = 'X'; n.inner.c = 'Y'; n.inner.i = 99; n.inner.d = 'Z';
    if (n.a != 'X' || n.inner.c != 'Y' || n.inner.i != 99 || n.inner.d != 'Z') {
        printf("FAIL 2\n"); return 1;
    }
    printf("Test 2 PASS: nested padding a=%c inner.i=%d\n", n.a, n.inner.i);
    pass++;
    // Test 3: post-increment on int32_t
    int32_t x = 10;
    int32_t y = x++;
    if (x != 11 || y != 10) { printf("FAIL 3\n"); return 1; }
    printf("Test 3 PASS: post-inc x=%d y=%d\n", x, y);
    pass++;
    // Test 4: char array
    char buf[10];
    int i = 0;
    while (i < 9) { buf[i] = 'A' + i; i++; }
    buf[9] = 0;
    printf("Test 4 PASS: char array: %s\n", buf);
    pass++;
    // Test 5: struct pointer arithmetic
    struct Point pts[3];
    pts[0].x = 1; pts[0].y = 2;
    pts[1].x = 3; pts[1].y = 4;
    pts[2].x = 5; pts[2].y = 6;
    struct Point *pp = pts;
    pp++;
    if (pp->x != 3 || pp->y != 4) { printf("FAIL 5\n"); return 1; }
    printf("Test 5 PASS: struct ptr++ (%d,%d)\n", pp->x, pp->y);
    pass++;
    // Test 6: sizeof(struct)
    if (sizeof(struct Point) != 2 * sizeof(int)) {
        printf("FAIL 6: sizeof=%d\n", (int)sizeof(struct Point));
        return 1;
    }
    printf("Test 6 PASS: sizeof(Point)=%d\n", (int)sizeof(struct Point));
    pass++;
    // Test 7: cast to struct pointer
    pp = (struct Point *)pts;
    if (pp->x != 1 || pp->y != 2) { printf("FAIL 7\n"); return 1; }
    printf("Test 7 PASS: (struct Point *) cast\n");
    pass++;
    // Test 8: strict padding with int64_t (no overlap in array)
    struct StrictPad arr[2];
    arr[0].c = 'A'; arr[0].i = 0x7FFFFFFFFFFFFFFF; arr[0].d = 'B';
    arr[1].c = 'C';
    if (arr[0].c != 'A' || arr[0].d != 'B' || arr[1].c != 'C') {
        printf("FAIL 8: overlap corruption\n"); return 1;
    }
    printf("Test 8 PASS: strict int64 padding, no overlap\n");
    pass++;
    printf("SUMMARY: %d/8 tests passed\n", pass);
    return (pass == 8) ? 0 : 1;
}
