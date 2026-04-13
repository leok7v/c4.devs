#include <stdio.h>
#include <stdint.h>

struct Point { int x; int y; };

int g_arr[4];
char g_buf[16];

int main(int argc, char **argv) {
    // local int array
    int a[5];
    a[0] = 10; a[1] = 20; a[2] = 30; a[3] = 40; a[4] = 50;
    printf("a: %d %d %d %d %d\n", a[0], a[1], a[2], a[3], a[4]);
    // local char array
    char buf[10];
    buf[0] = 'H'; buf[1] = 'i'; buf[2] = 0;
    printf("buf: %s\n", buf);
    // local int32 array
    int32_t d[3];
    d[0] = 100; d[1] = 200; d[2] = 300;
    printf("d: %d %d %d\n", d[0], d[1], d[2]);
    // pointer arithmetic on array
    printf("*(a+2): %d\n", *(a + 2));
    // array of pointers
    int *ptrs[4];
    ptrs[0] = a;
    ptrs[1] = a + 1;
    printf("*ptrs[0]=%d *ptrs[1]=%d\n", *ptrs[0], *ptrs[1]);
    // array of structs
    struct Point pts[3];
    pts[0].x = 1; pts[0].y = 2;
    pts[1].x = 3; pts[1].y = 4;
    pts[2].x = 5; pts[2].y = 6;
    printf("pts[0]: (%d,%d) pts[1]: (%d,%d)\n",
           pts[0].x, pts[0].y, pts[1].x, pts[1].y);
    printf("pts[2]: (%d,%d)\n", pts[2].x, pts[2].y);
    // global array with mid-block loop variable
    int i = 0;
    while (i < 5) { g_arr[i] = (i + 1) * 100; ++i; }
    printf("g_arr: %d %d %d %d\n", g_arr[0], g_arr[1], g_arr[2], g_arr[3]);
    // global char array
    g_buf[0] = 'O'; g_buf[1] = 'K'; g_buf[2] = 0;
    printf("g_buf: %s\n", g_buf);
    printf("All array tests passed!\n");
    return 0;
}
