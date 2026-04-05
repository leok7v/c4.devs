#include <stdio.h>

int passed;
int failed;

void check(int n, int cond, char *msg) {
    if (cond) { passed = passed + 1;
    } else { printf("FAIL %d: %s\n", n, msg); failed = failed + 1;
    }
}

int add3(int a, int b, int c) { return a + b + c; }
int mul2(int a, int b) { return a * b; }

int main() {
    passed = 0;
    failed = 0;
    int t = 1;
    int a, b, c, x, y, z;
    int *p, *q;
    int arr[10];
    // Basic arithmetic
    a = 10; b = 3;
    check(t++, a + b == 13, "10 + 3 == 13");
    check(t++, a - b == 7, "10 - 3 == 7");
    check(t++, a * b == 30, "10 * 3 == 30");
    check(t++, a / b == 3, "10 / 3 == 3");
    check(t++, a % b == 1, "10 % 3 == 1");
    // Unary operators
    check(t++, -a == -10, "-10 == -10");
    check(t++, +a == 10, "+10 == 10");
    check(t++, !0 == 1, "!0 == 1");
    check(t++, !5 == 0, "!5 == 0");
    check(t++, ~0 == -1, "~0 == -1");
    // Bitwise operators
    a = 0x0F; b = 0xF0;
    check(t++, (a | b) == 0xFF, "0x0F | 0xF0 == 0xFF");
    check(t++, (a & b) == 0, "0x0F & 0xF0 == 0");
    check(t++, (a ^ b) == 0xFF, "0x0F ^ 0xF0 == 0xFF");
    check(t++, (a << 4) == 0xF0, "0x0F << 4 == 0xF0");
    check(t++, (b >> 4) == 0x0F, "0xF0 >> 4 == 0x0F");
    // Comparison operators
    a = 5; b = 10;
    check(t++, (a < b) == 1, "5 < 10");
    check(t++, (a > b) == 0, "5 > 10 is false");
    check(t++, (a <= 5) == 1, "5 <= 5");
    check(t++, (b >= 10) == 1, "10 >= 10");
    check(t++, (a == 5) == 1, "5 == 5");
    check(t++, (a != b) == 1, "5 != 10");
    // Logical operators (short-circuit)
    a = 1; b = 0;
    check(t++, (a || b) == 1, "1 || 0 == 1");
    check(t++, (a && b) == 0, "1 && 0 == 0");
    check(t++, (b || a) == 1, "0 || 1 == 1");
    check(t++, (a && 1) == 1, "1 && 1 == 1");
    x = 0;
    y = (0 && (x = 1));
    check(t++, x == 0, "short-circuit && skips RHS");
    x = 0;
    y = (1 || (x = 1));
    check(t++, x == 0, "short-circuit || skips RHS");
    // Conditional operator
    a = 5; b = 10;
    check(t++, (a < b ? 1 : 0) == 1, "5 < 10 ? 1 : 0");
    check(t++, (a > b ? 1 : 0) == 0, "5 > 10 ? 1 : 0");
    check(t++, (a ? b : 0) == 10, "5 ? 10 : 0");
    check(t++, (0 ? a : b) == 10, "0 ? 5 : 10");
    // Nested ternary
    a = 1; b = 2; c = 3;
    check(t++, (a == 1 ? (b == 2 ? c : 0) : 0) == 3, "nested ternary");
    // Increment/decrement
    a = 5;
    check(t++, ++a == 6, "++a == 6");
    check(t++, a == 6, "a == 6 after ++a");
    check(t++, a++ == 6, "a++ == 6");
    check(t++, a == 7, "a == 7 after a++");
    check(t++, --a == 6, "--a == 6");
    check(t++, a-- == 6, "a-- == 6");
    check(t++, a == 5, "a == 5 after a--");
    // Compound expressions
    a = 2; b = 3; c = 4;
    check(t++, a + b * c == 14, "2 + 3 * 4 == 14 (precedence)");
    check(t++, (a + b) * c == 20, "(2 + 3) * 4 == 20");
    check(t++, a * b + c == 10, "2 * 3 + 4 == 10");
    check(t++, a + b + c == 9, "2 + 3 + 4 == 9");
    check(t++, c - b - a == -1, "4 - 3 - 2 == -1 (left assoc)");
    // Complex precedence
    a = 2; b = 3; c = 4;
    check(t++, a | b & c == 2, "2 | 3 & 4 (& higher than |)");
    check(t++, (a < b) + (b < c) == 2, "comparison in arithmetic");
    check(t++, a + b * c / 2 == 8, "2 + 3 * 4 / 2 == 8");
    // Pointer arithmetic
    arr[0] = 100; arr[1] = 200; arr[2] = 300;
    p = arr;
    check(t++, *p == 100, "*p == 100");
    check(t++, *(p + 1) == 200, "*(p + 1) == 200");
    check(t++, *(p + 2) == 300, "*(p + 2) == 300");
    check(t++, p[0] == 100, "p[0] == 100");
    check(t++, p[1] == 200, "p[1] == 200");
    check(t++, p[2] == 300, "p[2] == 300");
    // Pointer increment
    p = arr;
    check(t++, *p++ == 100, "*p++ == 100");
    check(t++, *p == 200, "*p == 200 after p++");
    check(t++, *++p == 300, "*++p == 300");
    // Pointer subtraction
    p = arr + 2;
    q = arr;
    check(t++, p - q == 2, "ptr - ptr == 2");
    // Address-of and dereference
    a = 42;
    p = &a;
    check(t++, *p == 42, "*(&a) == 42");
    *p = 99;
    check(t++, a == 99, "a == 99 after *p = 99");
    // Assignment in expression
    a = (b = 5) + 3;
    check(t++, b == 5, "b == 5 from (b = 5)");
    check(t++, a == 8, "a == 8 from (b = 5) + 3");
    // Chained assignment
    a = b = c = 7;
    check(t++, a == 7 && b == 7 && c == 7, "a = b = c = 7");
    // Function calls in expressions
    check(t++, add3(1, 2, 3) == 6, "add3(1,2,3) == 6");
    check(t++, mul2(3, 4) + 2 == 14, "mul2(3,4) + 2 == 14");
    check(t++, add3(mul2(2, 3), 4, 5) == 15, "add3(mul2(2,3),4,5) == 15");
    // Sizeof in expressions
    check(t++, sizeof(int) == 8, "sizeof(int) == 8");
    check(t++, sizeof(char) == 1, "sizeof(char) == 1");
    check(t++, sizeof(int *) == 8, "sizeof(int *) == 8");
    // Cast in expressions
    a = (int)((char)255);
    check(t++, a == -1 || a == 255, "cast char to int");
    // Complex nested expressions
    a = 2; b = 3; c = 4;
    x = (a + b) * (c - a) + (b > a ? c : a);
    check(t++, x == 14, "(2+3)*(4-2)+(3>2?4:2) == 14");
    // Array indexing with expressions
    arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40;
    a = 1;
    check(t++, arr[a] == 20, "arr[a] where a=1");
    check(t++, arr[a + 1] == 30, "arr[a + 1] where a=1");
    check(t++, arr[a * 2] == 30, "arr[a * 2] where a=1");
    // Mixed pointer and array
    p = arr;
    check(t++, (p + a)[1] == 30, "(p + a)[1] where a=1");
    check(t++, *(p + a + 1) == 30, "*(p + a + 1)");
    printf("SUMMARY: %d/%d expression tests passed\n", passed, passed + failed);
    return failed;
}
