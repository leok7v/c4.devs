#include <stdio.h>
#include <stdint.h>

struct point_s { int x; int y; };
typedef struct point_s point_t;
typedef int myint;
typedef char* string;
typedef int* intptr;

void init_point(point_t *p, int x, int y) { p->x = x; p->y = y; }

point_t make_point(int x, int y) {
    point_t p;
    p.x = x;
    p.y = y;
    return p;
}

myint add(myint a, myint b) { return a + b; }

int main(int argc, char **argv) {
    printf("Testing typedef\n");
    point_t p;
    p.x = 10;
    p.y = 20;
    if (p.x == 10 && p.y == 20) { printf("OK: point_t works\n");
    } else { printf("FAIL: point_t\n"); return -1;
    }
    point_t p2;
    init_point(&p2, 30, 40);
    if (p2.x == 30 && p2.y == 40) { printf("OK: point_t param\n");
    } else { printf("FAIL: point_t param\n"); return -1;
    }
    point_t *pp = &p;
    if (pp->x == 10) { printf("OK: point_t pointer\n");
    } else { printf("FAIL: point_t pointer\n"); return -1;
    }
    point_t p3 = make_point(50, 60);
    if (p3.x == 50 && p3.y == 60) { printf("OK: struct return\n");
    } else {
        printf("FAIL: struct return x=%d y=%d\n", (int)p3.x, (int)p3.y);
        return -1;
    }
    myint x = 42;
    if (x == 42) { printf("OK: myint\n");
    } else { printf("FAIL: myint\n"); return -1;
    }
    myint sum = add(10, 20);
    if (sum == 30) { printf("OK: myint params\n");
    } else { printf("FAIL: myint params\n"); return -1;
    }
    int val = 99;
    intptr ip = &val;
    if (*ip == 99) { printf("OK: intptr\n");
    } else { printf("FAIL: intptr\n"); return -1;
    }
    string s = "hello";
    printf("string: %s\n", s);
    printf("sizeof(point_t) = %d\n", (int)sizeof(point_t));
    printf("sizeof(myint) = %d\n", (int)sizeof(myint));
    int64_t big = 12345;
    myint small = (myint)big;
    if (small == 12345) { printf("OK: cast to typedef\n");
    } else { printf("FAIL: cast\n"); return -1;
    }
    printf("All typedef tests passed!\n");
    return 0;
}
