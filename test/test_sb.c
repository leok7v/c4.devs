#include "test/sb.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
    struct sb b;
    b.count = 0;
    b.capacity = 0;
    b.data = 0;

    printf("Testing sb_putc...\n");
    sb_putc(&b, 'H');
    sb_putc(&b, 'e');
    sb_putc(&b, 'l');
    sb_putc(&b, 'l');
    sb_putc(&b, 'o');
    if (strcmp(b.data, "Hello") != 0) { printf("FAIL: sb_putc data\n"); return 1; }
    if (b.count != 5) { printf("FAIL: sb_putc count\n"); return 1; }

    printf("Testing sb_puts...\n");
    sb_puts(&b, " world");
    if (strcmp(b.data, "Hello world") != 0) { printf("FAIL: sb_puts data\n"); return 1; }
    if (b.count != 11) { printf("FAIL: sb_puts count\n"); return 1; }

    printf("Testing sb_put...\n");
    sb_put(&b, "!!!", 3);
    if (strcmp(b.data, "Hello world!!!") != 0) { printf("FAIL: sb_put data\n"); return 1; }
    if (b.count != 14) { printf("FAIL: sb_put count\n"); return 1; }

#ifndef __cx__
    printf("Testing sb_printf...\n");
    sb_printf(&b, " %d + %d = %d", 1, 2, 3);
    if (strcmp(b.data, "Hello world!!! 1 + 2 = 3") != 0) { printf("FAIL: sb_printf\n"); return 1; }
    if (b.count != 24) { printf("FAIL: sb_printf count\n"); return 1; }
#endif

    printf("Testing large grow...\n");
    char large[1024];
    memset(large, 'A', 1023);
    large[1023] = '\0';
    sb_puts(&b, large);
    int expected_count = 14 + 1023;
#ifndef __cx__
    expected_count = 24 + 1023;
#endif
    if (b.count != expected_count) { printf("FAIL: large grow count %d != %d\n", b.count, expected_count); return 1; }
    if (b.data[b.count] != '\0') { printf("FAIL: large grow null term\n"); return 1; }
    if (b.data[14] != 'A') { printf("FAIL: large grow data char\n"); return 1; }

    printf("Testing sb_free...\n");
    sb_free(&b);
    if (b.data != 0) { printf("FAIL: sb_free data\n"); return 1; }
    if (b.count != 0) { printf("FAIL: sb_free count\n"); return 1; }
    if (b.capacity != 0) { printf("FAIL: sb_free capacity\n"); return 1; }

    printf("All sb.h tests passed!\n");
    return 0;
}
