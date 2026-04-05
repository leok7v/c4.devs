#include <stdio.h>
#include <stdbool.h>

bool returns_true(void) { return true; }
bool returns_false(void) { return false; }

int main(int argc, char **argv) {
    printf("Testing bool type\n");
    bool b = true;
    if (b != false) { printf("OK: b is true\n");
    } else { printf("FAIL: b should be true\n"); return -1;
    }
    b = false;
    if (b == false) { printf("OK: b is false\n");
    } else { printf("FAIL: b should be false\n"); return -1;
    }
    if (true) { printf("OK: true is truthy\n");
    } else { printf("FAIL: true should be truthy\n"); return -1;
    }
    if (!false) { printf("OK: !false is truthy\n");
    } else { printf("FAIL: !false should be truthy\n"); return -1;
    }
    bool t = returns_true();
    bool f = returns_false();
    if (t && !f) { printf("OK: function returns work\n");
    } else { printf("FAIL: function returns\n"); return -1;
    }
    int x = true + true;
    if (x == 2) { printf("OK: true + true == 2\n");
    } else { printf("FAIL: true + true = %d\n", x); return -1;
    }
    printf("sizeof(bool) = %d\n", (int)sizeof(bool));
    printf("All bool tests passed!\n");
    return 0;
}
