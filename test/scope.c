#include <stdio.h>

int passed;
int failed;

void check(int test_num, int cond, char *msg) {
    if (cond) { printf("Test %d PASS: %s\n", test_num, msg); passed++;
    } else    { printf("Test %d FAIL: %s\n", test_num, msg); failed++;
    }
}

int returns_42(void) { return 42; }
int add(int a, int b) { return a + b; }

int main() {
    passed = 0;
    failed = 0;
    int t = 1;
    int x;
    check(t++, returns_42() == 42, "(void) param function");
    { int a = 10; check(t++, a == 10, "init-in-decl int a = 10"); }
    x = 100;
    { int x = 200; check(t++, x == 200, "block x shadows outer x"); }
    check(t++, x == 100, "outer x restored after block");
    x = 1;
    {
        int x = 2;
        { int x = 3; check(t++, x == 3, "inner nested x == 3"); }
        check(t++, x == 2, "middle x == 2 after inner block");
    }
    check(t++, x == 1, "outer x == 1 after all blocks");
    { int a = 10; int b = 20; int c = a + b; check(t++, c == 30, "c=a+b"); }
    { int r = returns_42(); check(t++, r == 42, "init r = returns_42()"); }
    { char ch = 'Z'; check(t++, ch == 'Z', "char init ch = Z"); }
    { int val = 99; int *p = &val; check(t++, *p == 99, "ptr init *p==99"); }
    { int a = 5, b = 6; check(t++, a + b == 11, "multi-decl a=5 b=6"); }
    { int z = 10; check(t++, z == 10, "sibling block 1 z=10"); }
    { int z = 20; check(t++, z == 20, "sibling block 2 z=20"); }
    check(t++, add(3, 4) == 7, "non-void params still work");
    { int ft = 77; check(t++, ft == 77, "function-top init ft = 77"); }
    printf("SUMMARY: %d/%d tests passed\n", passed, passed + failed);
    return failed;
}
