#include <stdio.h>

#define MAX_SIZE 100
#define GREETING "Hello, Preprocessor!"
#define EMPTY_MACRO

int main() {
    int pass = 0;
    int fail = 0;
    printf("Testing preprocessor\n");
    // Test 1: simple define constant
    int arr[MAX_SIZE];
    arr[0] = 42;
    if (MAX_SIZE == 100 && arr[0] == 42) {
        printf("OK: #define constant MAX_SIZE=%d\n", MAX_SIZE);
        pass++;
    } else {
        printf("FAIL: MAX_SIZE\n");
        fail++;
    }
    // Test 2: define string
    char *msg = GREETING;
    if (msg[0] == 'H') {
        printf("OK: #define string %s\n", GREETING);
        pass++;
    } else {
        printf("FAIL: GREETING\n");
        fail++;
    }
    // Test 3: ifdef with defined macro
#ifdef MAX_SIZE
    printf("OK: #ifdef MAX_SIZE is true\n");
    pass++;
#else
    printf("FAIL: #ifdef MAX_SIZE should be true\n");
    fail++;
#endif
    // Test 4: ifdef with undefined macro
#ifdef UNDEFINED_MACRO
    printf("FAIL: #ifdef UNDEFINED_MACRO should be false\n");
    fail++;
#else
    printf("OK: #ifdef UNDEFINED_MACRO is false\n");
    pass++;
#endif
    // Test 5: ifndef
#ifndef UNDEFINED_MACRO
    printf("OK: #ifndef UNDEFINED_MACRO is true\n");
    pass++;
#else
    printf("FAIL: #ifndef UNDEFINED_MACRO should be true\n");
    fail++;
#endif
    // Test 6: undef
#define TEMP_MACRO 42
#undef TEMP_MACRO
#ifdef TEMP_MACRO
    printf("FAIL: TEMP_MACRO should be undefined\n");
    fail++;
#else
    printf("OK: #undef removed TEMP_MACRO\n");
    pass++;
#endif
    // Test 7: nested ifdef
#define OUTER 1
#ifdef OUTER
    #define INNER 2
    #ifdef INNER
        printf("OK: nested #ifdef works\n");
        pass++;
    #else
        printf("FAIL: nested ifdef\n");
        fail++;
    #endif
#endif
    // Test 8: os() conditional (at least one should be true)
    int os_detected = 0;
#if os(linux)
    printf("OK: os(linux) detected\n");
    os_detected = 1;
#endif
#if os(apple)
    printf("OK: os(apple) detected\n");
    os_detected = 1;
#endif
#if os(windows)
    printf("OK: os(windows) detected\n");
    os_detected = 1;
#endif
    if (os_detected) {
        pass++;
    } else {
        printf("WARN: no os() detected (may be expected)\n");
    }
    // Test 9: empty macro (defined but no value)
#ifdef EMPTY_MACRO
    printf("OK: empty macro is defined\n");
    pass++;
#else
    printf("FAIL: empty macro should be defined\n");
    fail++;
#endif
    // Test 10: pragma once (just syntax check - no crash)
#pragma once
    printf("OK: #pragma once parsed\n");
    pass++;
    // Summary
    printf("\nPreprocessor tests: %d passed, %d failed\n", pass, fail);
    return fail;
}
