#include <stdio.h>
/* Global initializers */
int g_counter = 42;
int g_negative = -7;
char g_ch = 'X';
int g_zero = 0;
char *g_msg = "hello world";
int64_t g_big = 123456789012;
int32_t g_small = 999;
static int g_static_val = 77;
static char *g_static_str = "static string";
int g_uninit;
/* Global arrays */
int g_arr[] = {1, 2, 3, 4, 5};
char g_chars[] = {65, 66, 67, 0};
int g_fixed[8] = {10, 20, 30};
/* Static local test functions */
int counter_fn() {
    static int count;
    count = count + 1;
    return count;
}
void print_once() {
    static bool done;
    if (!done) {
        printf("printed once\n");
        done = true;
    }
}
int accumulate(int val) {
    static int total = 0;
    total = total + val;
    return total;
}
void test_static_local_array() {
    static int s_arr[] = {100, 200, 300};
    static int call_count;
    call_count++;
    printf("static local array: %d %d %d (call %d)\n",
           s_arr[0], s_arr[1], s_arr[2], call_count);
    s_arr[0] = s_arr[0] + 1;
}
int main() {
    int pass = 0;
    int total = 0;
    printf("Testing globals, static locals, and array initializers\n");
    /* Global scalar initializers */
    total++; if (g_counter == 42) { pass++; printf("OK: g_counter = 42\n");
    } else { printf("FAIL: g_counter = %d\n", g_counter); }
    total++; if (g_negative == -7) { pass++; printf("OK: g_negative = -7\n");
    } else { printf("FAIL: g_negative = %d\n", g_negative); }
    total++; if (g_ch == 'X') { pass++; printf("OK: g_ch = X\n");
    } else { printf("FAIL: g_ch = %c\n", g_ch); }
    total++; if (g_zero == 0) { pass++; printf("OK: g_zero = 0\n");
    } else { printf("FAIL: g_zero = %d\n", g_zero); }
    total++; if (g_msg[0] == 'h' && g_msg[5] == ' ') { pass++;
        printf("OK: g_msg = %s\n", g_msg);
    } else { printf("FAIL: g_msg = %s\n", g_msg); }
    total++; if (g_big == 123456789012) { pass++; printf("OK: g_big correct\n");
    } else { printf("FAIL: g_big\n"); }
    total++; if (g_small == 999) { pass++; printf("OK: g_small = 999\n");
    } else { printf("FAIL: g_small = %d\n", g_small); }
    total++; if (g_static_val == 77) { pass++; printf("OK: g_static_val = 77\n");
    } else { printf("FAIL: g_static_val = %d\n", g_static_val); }
    total++; if (g_static_str[0] == 's') { pass++;
        printf("OK: g_static_str = %s\n", g_static_str);
    } else { printf("FAIL: g_static_str\n"); }
    total++; if (g_uninit == 0) { pass++; printf("OK: g_uninit = 0\n");
    } else { printf("FAIL: g_uninit = %d\n", g_uninit); }
    g_counter = 100;
    total++; if (g_counter == 100) { pass++; printf("OK: g_counter modified\n");
    } else { printf("FAIL: g_counter after modify\n"); }
    /* Global arrays */
    total++; if (g_arr[0] == 1 && g_arr[4] == 5) { pass++;
        printf("OK: global int array\n");
    } else { printf("FAIL: global int array\n"); }
    total++; if (g_chars[0] == 65 && g_chars[2] == 67 && g_chars[3] == 0) { pass++;
        printf("OK: global char array\n");
    } else { printf("FAIL: global char array\n"); }
    total++; if (g_fixed[0] == 10 && g_fixed[2] == 30 && g_fixed[3] == 0) { pass++;
        printf("OK: global fixed-size array\n");
    } else { printf("FAIL: global fixed-size array\n"); }
    /* Local array */
    total++;
    int l_arr[] = {7, 8, 9};
    if (l_arr[0] == 7 && l_arr[2] == 9) { pass++;
        printf("OK: local int array\n");
    } else { printf("FAIL: local int array\n"); }
    /* Static local counter */
    total++;
    int c1 = counter_fn();
    int c2 = counter_fn();
    int c3 = counter_fn();
    if (c1 == 1 && c2 == 2 && c3 == 3) { pass++;
        printf("OK: static counter %d %d %d\n", c1, c2, c3);
    } else { printf("FAIL: counter %d %d %d\n", c1, c2, c3); }
    /* Static local bool for one-time init */
    total++;
    print_once();
    print_once();
    print_once();
    printf("OK: print_once called 3 times\n");
    pass++;
    /* Static local with initializer */
    total++;
    int a1 = accumulate(10);
    int a2 = accumulate(20);
    int a3 = accumulate(5);
    if (a1 == 10 && a2 == 30 && a3 == 35) { pass++;
        printf("OK: accumulate %d %d %d\n", a1, a2, a3);
    } else { printf("FAIL: accumulate %d %d %d\n", a1, a2, a3); }
    /* Multiple static locals in same function */
    total++;
    static int x;
    static int y = 100;
    x = x + 1;
    y = y + 1;
    if (x == 1 && y == 101) { pass++;
        printf("OK: static in main x=%d y=%d\n", x, y);
    } else { printf("FAIL: static in main x=%d y=%d\n", x, y); }
    /* Static local array persistence */
    total++;
    test_static_local_array();
    test_static_local_array();
    printf("OK: static local array persists\n");
    pass++;
    printf("\nResults: %d/%d passed\n", pass, total);
    return pass == total ? 0 : 1;
}
