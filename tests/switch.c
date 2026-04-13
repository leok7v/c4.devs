#include <stdio.h>

int main() {
    int i = 0;
    while (i < 5) {
        switch (i) {
            case 0:
                printf("zero\n");
                break;
            case 1:
                printf("one\n");
                break;
            case 2:
                printf("two\n");
                break;
            case 3:
                printf("three\n");
                break;
            case 4:
                printf("four\n");
                break;
            default:
                printf("other\n");
        }
        i = i + 1;
    }
    return 0;
}