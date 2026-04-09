#include <stdio.h>
#include <stdarg.h>

#ifdef __cx__
typedef int64_t * va_list;
#define va_start(ap, last) ap = va_start(&last)
#define va_copy(dest, src) dest = va_copy(src)
#define va_end(ap) va_end(ap)
#endif

void my_printf(char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("Arg 1: %d\n", *ap);
    ap = ap - 1;
    printf("Arg 2: %d\n", *ap);
}

int main() {
    my_printf("test", 42, 100);
    return 0;
}
