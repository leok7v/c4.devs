#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main() {
    char *filename = "build/generated_hello.c";
    char *code = "int main() { printf(\"Hello from generated code!\\n\"); "
                  "return 42; }";
    printf("--- 1. Writing generated code to %s ---\n", filename);
    // O_WRONLY | O_CREAT | O_TRUNC: Linux=577, macOS=1537
    int fd = open(filename, 577, 420);
    if (fd < 0) { fd = open(filename, 1537, 420); }
    if (fd < 0) { printf("Failed to create file\n"); return -1; }
    int len = strlen(code);
    printf("Code length: %d\n", len);
    write(fd, code, len);
    close(fd);
    printf("--- 2. Compiling and running with ./build/cx ---\n");
    char *cmd = "./build/cx build/generated_hello.c";
    printf("Command: %s\n", cmd);
    int status = system(cmd);
    printf("System returned status: %d\n", status);
    return 0;
}
