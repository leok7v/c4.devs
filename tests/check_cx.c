#include <stdio.h>

int main() {
#ifdef __cx__
    printf("__cx__ is defined\n");
#else
    printf("__cx__ is NOT defined\n");
#endif
    return 0;
}
