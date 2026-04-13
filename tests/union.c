#include <stdio.h>

union IntOrChar {
    int i;
    char c;
};

union Mixed {
    int64_t big;
    int32_t small;
    char ch;
};

union PtrOrInt {
    int val;
    char *str;
};

struct Point { int x; int y; };

union Value {
    int i;
    struct Point p;
};

typedef union Mixed mixed_t;

int main() {
    int passed = 0;
    int total = 0;
    union IntOrChar u;
    u.i = 0x41424344;
    total++; if (u.c == 'D') { passed++; printf("OK: overlay works\n");
    } else { printf("FAIL: u.c = %c\n", u.c); }
    union Mixed m;
    m.big = 0x123456789ABCDEF0;
    total++; if (m.big == 0x123456789ABCDEF0) { passed++;
        printf("OK: m.big\n");
    } else { printf("FAIL: m.big\n"); }
    m.small = 42;
    total++; if (m.small == 42) { passed++; printf("OK: m.small\n");
    } else { printf("FAIL: m.small\n"); }
    m.ch = 'X';
    total++; if (m.ch == 'X') { passed++; printf("OK: m.ch\n");
    } else { printf("FAIL: m.ch\n"); }
    union PtrOrInt p;
    p.val = 999;
    total++; if (p.val == 999) { passed++; printf("OK: p.val\n");
    } else { printf("FAIL: p.val\n"); }
    p.str = "hello";
    total++; if (p.str[0] == 'h') { passed++;
        printf("OK: p.str = %s\n", p.str);
    } else { printf("FAIL: p.str\n"); }
    mixed_t mt;
    mt.big = 777;
    total++; if (mt.big == 777) { passed++; printf("OK: typedef\n");
    } else { printf("FAIL: typedef\n"); }
    total++; if (sizeof(union Mixed) == 8) { passed++;
        printf("OK: sizeof(Mixed) = 8\n");
    } else { printf("FAIL: sizeof = %d\n", (int)sizeof(union Mixed)); }
    union Value v;
    v.p.x = 10;
    v.p.y = 20;
    total++; if (v.p.x == 10 && v.p.y == 20) { passed++;
        printf("OK: struct in union\n");
    } else { printf("FAIL: struct in union\n"); }
    total++; if (sizeof(union Value) == 16) { passed++;
        printf("OK: sizeof(Value) = 16\n");
    } else { printf("FAIL: sizeof(Value) = %d\n", (int)sizeof(union Value)); }
    printf("Union tests: %d/%d passed\n", passed, total);
    return passed == total ? 0 : -1;
}
