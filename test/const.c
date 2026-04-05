#include <stdio.h>

const int g;
const char *msg;

int foo(const char *s) {
    printf("foo: %s\n", s);
    return 0;
}

int main() {
    const char *s;
    const int x;
    s = "hello const";
    x = 42;
    msg = "global msg";
    printf("s=%s x=%d msg=%s\n", s, x, msg);
    foo(s);
    printf("const test passed\n");
    return 0;
}
