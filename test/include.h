/* Header for testing #include "file.h", block comments, and error reporting */
#pragma once
#define HELPER_VERSION 42
#define SQUARE(x) ((x) * (x))
int helper_add(int a, int b) { return a + b; }
// line comment in header
int helper_mul(int a, int b) {
    /* inline block comment */
    return a * b;
}
#ifdef TRIGGER_ERROR
int helper_bad(int x) {
    return undefined_variable;
}
#endif
#ifdef CHECK_FLAG
int get_flag(void) { return CHECK_FLAG; }
#endif
