#include <stdio.h>

void f(int a, int b) {
    int * pa = &a;
    int * pb = &b;
    printf("a: %d, b: %d\n", *pa, *pb);
    if (pb == pa + 1) {
        printf("Stack grows UP (normal for VM)\n");
    } else if (pa == pb + 1) {
        printf("Stack grows DOWN\n");
    }
}

int main() {
    f(10, 20);
    return 0;
}
