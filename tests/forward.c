#include <stdio.h>

int bar(int x);
int baz(int a, int b);

int foo(int x) {
    return bar(x + 1);
}

int bar(int x) {
    return x * 2;
}

int baz(int a, int b) {
    return a + b;
}

int main() {
    int r1;
    int r2;
    int r3;
    r1 = foo(5);
    r2 = bar(10);
    r3 = baz(3, 4);
    printf("foo(5)=%d bar(10)=%d baz(3,4)=%d\n", r1, r2, r3);
    if (r1 == 12 && r2 == 20 && r3 == 7) {
        printf("forward declaration test passed\n");
        return 0;
    }
    printf("forward declaration test FAILED\n");
    return -1;
}
