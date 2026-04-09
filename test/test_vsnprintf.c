#include <stdio.h>
#include <stdarg.h>

#ifdef __cx__
typedef int64_t * va_list;
#define va_start(ap, last) ap = va_start(&last)
#define va_copy(dest, src) dest = va_copy(src)
#define va_end(ap) va_end(ap)
#endif

int my_vsnprintf(char * s, int n, char * f, ...) {
    va_list ap;
    va_start(ap, f);
    int res = vsnprintf(s, n, f, ap);
    va_end(ap);
    return res;
}

int main() {
    char buf[100];
    int n = my_vsnprintf(buf, 100, "test %d %s", 42, "hello");
    printf("res: %d, buf: %s\n", n, buf);
    return 0;
}
