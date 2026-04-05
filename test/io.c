#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv) {
    char *filename = "build/test_io_output.txt";
    // Test 1: File Write (Create + Write)
    printf("--- Testing File I/O ---\n");
    // O_WRONLY | O_CREAT | O_TRUNC: Linux=577, macOS=1537
    int fd = open(filename, 577, 420);
    if (fd < 0) { fd = open(filename, 1537, 420); }
    if (fd < 0) { printf("Failed to open file for write\n"); return -1; }
    int n = write(fd, "File content test\n", 18);
    if (n != 18) { printf("Error: Short write\n");
    } else { printf("Successfully wrote to file\n");
    }
    close(fd);
    // Test 2: File Read
    fd = open(filename, 0); // O_RDONLY
    if (fd < 0) { printf("Failed to open file for read\n"); return -1; }
    char *buf = malloc(100);
    memset(buf, 0, 100);
    n = read(fd, buf, 100);
    close(fd);
    printf("Read from file: %s", buf);
    // Test 3: Stdout Write (using file descriptor 1)
    printf("\n--- Testing Stdout Write ---\n");
    write(1, "Stdout write works!\n", 20);
    return 0;
}
