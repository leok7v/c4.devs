#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("--- Testing system() ---\n");
    int status = system("echo 'Hello from system()!'");
    printf("system() returned: %d\n", status);
    printf("\n--- Testing popen() and fread() ---\n");
    int fp = popen("echo 'Hello from popen!'", "r");
    if (!fp) { printf("popen failed\n"); return -1; }
    printf("popen returned FILE* = %d\n", fp);
    char *buf = malloc(128);
    memset(buf, 0, 128);
    int n = fread(buf, 1, 127, fp);
    printf("fread read %d bytes\n", n);
    printf("Output: %s", buf);
    pclose(fp);
    printf("\n--- Testing popen() with curl ---\n");
    fp = popen("curl -s ifconfig.me", "r");
    if (!fp) { printf("popen curl failed\n"); return -1; }
    memset(buf, 0, 128);
    n = fread(buf, 1, 127, fp);
    if (n > 0) { printf("\nMy IP is: %s\n", buf);
    } else { printf("Failed to read from curl or no output\n");
    }
    pclose(fp);
    return 0;
}
