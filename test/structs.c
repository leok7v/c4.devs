#include <stdio.h>

struct Point { int x; int y; };
struct Padded { char c; int i; };
struct Nested { char a; struct Padded inner; };

struct Point make_point(int x, int y) {
    struct Point p;
    p.x = x;
    p.y = y;
    return p;
}

int sum_point(struct Point p) { return p.x + p.y; }

void modify_point(struct Point p) { p.x = 999; p.y = 888; }

int main() {
    int pass = 0;
    // Test 1: basic struct, member access
    struct Point p;
    p.x = 10; p.y = 20;
    if (p.x != 10 || p.y != 20) { printf("FAIL 1\n"); return 1; }
    printf("Test 1 PASS: basic struct p=(%d,%d)\n", (int)p.x, (int)p.y);
    pass++;
    // Test 2: nested struct
    struct Nested n;
    n.a = 'X'; n.inner.c = 'Y'; n.inner.i = 42;
    if (n.a != 'X' || n.inner.c != 'Y' || n.inner.i != 42) {
        printf("FAIL 2\n"); return 1;
    }
    printf("Test 2 PASS: nested struct n.a=%c n.inner.i=%d\n",
           n.a, (int)n.inner.i);
    pass++;
    // Test 3: struct padding
    struct Padded m;
    m.c = 'A'; m.i = 99;
    if (m.c != 'A' || m.i != 99) { printf("FAIL 3\n"); return 1; }
    printf("Test 3 PASS: struct padding c=%c i=%d\n", m.c, (int)m.i);
    pass++;
    // Test 4: struct array and pointer arithmetic
    struct Point pts[3];
    pts[0].x = 1; pts[0].y = 2;
    pts[1].x = 3; pts[1].y = 4;
    pts[2].x = 5; pts[2].y = 6;
    struct Point *pp = pts;
    pp++;
    if (pp->x != 3 || pp->y != 4) { printf("FAIL 4\n"); return 1; }
    printf("Test 4 PASS: struct array ptr++ pp=(%d,%d)\n",
           (int)pp->x, (int)pp->y);
    pass++;
    // Test 5: sizeof(struct) = 2 * sizeof(int)
    if (sizeof(struct Point) != 2 * sizeof(int)) {
        printf("FAIL 5: sizeof=%d expected %d\n",
               (int)sizeof(struct Point), (int)(2 * sizeof(int)));
        return 1;
    }
    printf("Test 5 PASS: sizeof(struct Point)=%d\n", (int)sizeof(struct Point));
    pass++;
    // Test 6: struct return by value
    struct Point r = make_point(100, 200);
    if (r.x != 100 || r.y != 200) { printf("FAIL 6\n"); return 1; }
    printf("Test 6 PASS: struct return r=(%d,%d)\n", (int)r.x, (int)r.y);
    pass++;
    // Test 7: struct return in loop (10K iterations)
    int i = 0;
    while (i < 10000) {
        struct Point lp = make_point(i, i + 1);
        if (i == 0 && (lp.x != 0 || lp.y != 1)) {
            printf("FAIL 7\n"); return 1;
        }
        i++;
    }
    printf("Test 7 PASS: struct return in loop (%d iters)\n", i);
    pass++;
    // Test 8: member access on returned struct
    int mx = make_point(50, 60).x;
    int my = make_point(50, 60).y;
    if (mx != 50 || my != 60) { printf("FAIL 8\n"); return 1; }
    printf("Test 8 PASS: member access on return .x=%d .y=%d\n", mx, my);
    pass++;
    // Test 9: struct param by value
    p.x = 10; p.y = 20;
    if (sum_point(p) != 30) { printf("FAIL 9\n"); return 1; }
    printf("Test 9 PASS: struct param sum=%d\n", sum_point(p));
    pass++;
    // Test 10: pass-by-value semantics (callee can't modify caller)
    p.x = 10; p.y = 20;
    modify_point(p);
    if (p.x != 10 || p.y != 20) {
        printf("FAIL 10: modified caller p=(%d,%d)\n", (int)p.x, (int)p.y);
        return 1;
    }
    printf("Test 10 PASS: pass-by-value semantics\n");
    pass++;
    // Test 11: pass struct return directly to function
    int s = sum_point(make_point(7, 8));
    if (s != 15) { printf("FAIL 11\n"); return 1; }
    printf("Test 11 PASS: struct return to param sum=%d\n", s);
    pass++;
    printf("All struct tests passed! (%d tests)\n", pass);
    return 0;
}
