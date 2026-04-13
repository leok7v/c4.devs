#include <stdio.h>

// Simulates what va_start/va_arg will do once real varargs land.
// After the R-to-L switch, cdecl layout is:
//   test_var(a, b, v1, v2) called  =>  bp[2]=a, bp[3]=b, bp[4]=v1, bp[5]=v2
//   &a = bp+2, &b = bp+3, &v1 = bp+4, &v2 = bp+5
// So "ap = &lastNamedParam + 1" points at the first variadic slot,
// and successive args are read by ap = ap + 1.  This is the whole
// reason R-to-L matters: static offset from the last named param.

void my_vprintf(char * fmt, int64_t * ap) {
    printf("First vararg:  %d\n", (int)*ap);
    ap = ap + 1;
    printf("Second vararg: %d\n", (int)*ap);
}

void test_var(int a, int b, int v1, int v2) {
    printf("a: %d, b: %d\n", a, b);
    // fake va_start(ap, b): start one slot past the last named param.
    my_vprintf("test", (int64_t *)(&b) + 1);
}

int main() {
    test_var(1, 2, 3, 4);
    // Expected:
    //   a: 1, b: 2
    //   First vararg:  3
    //   Second vararg: 4
    return 0;
}
