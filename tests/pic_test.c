#include <stdio.h>

char * global_str = "Global Hello";
int global_int = 42;

void hello() {
    printf("hello\n");
}

int main() {
    printf("str: %s\n", global_str);
    printf("int: %d\n", global_int);
    hello();
    
    if (global_int == 42) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    return 0;
}
