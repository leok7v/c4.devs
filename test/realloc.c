#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("Testing realloc...\n");
    char * p = malloc(10);
    strcpy(p, "012345678");
    printf("Initial: %s\n", p);
    
    p = realloc(p, 20);
    strcat(p, "9abcdefgh");
    printf("Reallocated: %s\n", p);
    
    if (strcmp(p, "0123456789abcdefgh") == 0) {
        printf("PASS: realloc works\n");
    } else {
        printf("FAIL: realloc failed\n");
        return 1;
    }
    
    free(p);
    return 0;
}
