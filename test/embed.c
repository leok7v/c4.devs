#include <stdio.h>
#include <string.h>

char embedded[] = {
    #embed "test/embed_data.txt", 0
};

int raw_bytes[] = {
    #embed "test/embed_data.txt"
};

int main() {
    int pass = 0;
    int total = 0;
    printf("Testing #embed directive\n");
    total++;
    if (strcmp(embedded, "ABC") == 0) {
        printf("PASS: embedded string = \"%s\"\n", embedded);
        pass++;
    } else {
        printf("FAIL: embedded string = \"%s\" (expected \"ABC\")\n", embedded);
    }
    total++;
    if (raw_bytes[0] == 65 && raw_bytes[1] == 66 && raw_bytes[2] == 67) {
        printf("PASS: raw_bytes = {%d, %d, %d}\n", 
               raw_bytes[0], raw_bytes[1], raw_bytes[2]);
        pass++;
    } else {
        printf("FAIL: raw_bytes = {%d, %d, %d}\n",
               raw_bytes[0], raw_bytes[1], raw_bytes[2]);
    }
    total++;
    if (embedded[3] == 0 && embedded[2] == 67) {
        printf("PASS: embedded[3]=0 (null terminator)\n");
        pass++;
    } else {
        printf("FAIL: embedded[3]=%d (expected 0)\n", embedded[3]);
    }
    printf("\nResults: %d/%d passed\n", pass, total);
    return pass == total ? 0 : 1;
}
