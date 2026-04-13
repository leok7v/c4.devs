#include <stdio.h>
#include <string.h>

enum Color { RED, GREEN, BLUE };
enum Status { OK = 0, ERROR = -1, PENDING = 100 };

char* color_name(int c) {
    switch (c) {
        case RED:   return "red";
        case GREEN: return "green";
        case BLUE:  return "blue";
        default:    return "unknown";
    }
}

int main() {
    printf("Testing enums\n");
    int pass = 0;
    // Test basic enum values
    if (RED == 0 && GREEN == 1 && BLUE == 2) {
        printf("OK: sequential enum values\n"); pass++;
    } else {
        printf("FAIL: RED=%d GREEN=%d BLUE=%d\n", RED, GREEN, BLUE);
        return 1;
    }
    // Test explicit enum values
    if (OK == 0 && ERROR == -1 && PENDING == 100) {
        printf("OK: explicit enum values\n"); pass++;
    } else {
        printf("FAIL: OK=%d ERROR=%d PENDING=%d\n", OK, ERROR, PENDING);
        return 1;
    }
    // Test switch with enum
    if (strcmp(color_name(RED), "red") == 0) {
        printf("OK: switch RED -> red\n"); pass++;
    } else {
        printf("FAIL: color_name(RED)\n"); return 1;
    }
    if (strcmp(color_name(GREEN), "green") == 0) {
        printf("OK: switch GREEN -> green\n"); pass++;
    } else {
        printf("FAIL: color_name(GREEN)\n"); return 1;
    }
    if (strcmp(color_name(BLUE), "blue") == 0) {
        printf("OK: switch BLUE -> blue\n"); pass++;
    } else {
        printf("FAIL: color_name(BLUE)\n"); return 1;
    }
    if (strcmp(color_name(99), "unknown") == 0) {
        printf("OK: switch default -> unknown\n"); pass++;
    } else {
        printf("FAIL: color_name(99)\n"); return 1;
    }
    // Test enum in expressions
    int x = RED + GREEN + BLUE;
    if (x == 3) {
        printf("OK: enum arithmetic RED+GREEN+BLUE=%d\n", x); pass++;
    } else {
        printf("FAIL: enum arithmetic\n"); return 1;
    }
    // Test enum comparison
    int c = GREEN;
    if (c > RED && c < BLUE) {
        printf("OK: enum comparison\n"); pass++;
    } else {
        printf("FAIL: enum comparison\n"); return 1;
    }
    printf("All enum tests passed! (%d tests)\n", pass);
    return 0;
}
