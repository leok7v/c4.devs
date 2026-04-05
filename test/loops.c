#include <stdio.h>

int main() {
    printf("--- for loop (existing var) ---");
    int i = 0;
    for (i = 0; i < 5; i = i + 1) {
        if (i == 1) {
            continue;
        }
        if (i == 4) {
            break;
        }
        printf("i = %d", i);
    }
    printf("--- for loop (scoped int j) ---");
    for (int j = 0; j < 5; j = j + 1) {
        if (j == 1) {
            continue;
        }
        if (j == 4) {
            break;
        }
        printf("j = %d", j);
    }
    printf("--- while loop ---");
    i = 0;
    while (i < 5) {
        if (i == 1) {
            i = i + 1;
            continue;
        }
        if (i == 4) {
            break;
        }
        printf("i = %d", i);
        i = i + 1;
    }
    printf("--- do-while loop ---");
    i = 0;
    do {
        if (i == 1) {
            i = i + 1;
            continue;
        }
        if (i == 4) {
            break;
        }
        printf("i = %d", i);
        i = i + 1;
    } while (i < 5);
    return 0;
}
