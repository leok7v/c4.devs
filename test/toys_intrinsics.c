#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int pass = 0;
int fail = 0;

struct cx_stat {
    int mode;
    int size;
    int mtime;
    int nlink;
    int uid;
};

void ok(char * what) {
    printf("  OK: %s\n", what);
    pass++;
}

void bad(char * what) {
    printf("FAIL: %s\n", what);
    fail++;
}

int main(void) {
    printf("=== Stage 3: filesystem intrinsics ===\n");
    char * dir = "build/intrin_dir";
    char * file = "build/intrin_dir/file.txt";
    char * payload = "hello, intrin";
    int payload_len = 13;
    int rc = mkdir(dir, 493);
    if (rc == 0) {
        ok("mkdir");
    } else {
        bad("mkdir");
    }
    struct cx_stat st;
    rc = stat(dir, &st);
    if (rc == 0 && (st.mode & S_IFMT) == S_IFDIR) {
        ok("stat: directory recognized");
    } else {
        bad("stat: directory recognized");
    }
    int fd = open(file, 1 | 0x200 | 0x400, 420);
    if (fd >= 0) {
        write(fd, payload, payload_len);
        close(fd);
        ok("open + write + close");
    } else {
        bad("open + write + close");
    }
    rc = stat(file, &st);
    if (rc == 0 && (st.mode & S_IFMT) == S_IFREG &&
        st.size == payload_len) {
        ok("stat: regular file size");
    } else {
        bad("stat: regular file size");
    }
    rc = access(file, F_OK);
    if (rc == 0) {
        ok("access F_OK on existing file");
    } else {
        bad("access F_OK on existing file");
    }
    rc = access("build/no_such_file_xyz", F_OK);
    if (rc != 0) {
        ok("access F_OK on missing file returns nonzero");
    } else {
        bad("access F_OK on missing file returns nonzero");
    }
    rc = access(file, R_OK);
    if (rc == 0) {
        ok("access R_OK");
    } else {
        bad("access R_OK");
    }
    int dp = opendir(dir);
    if (dp != 0) {
        int found = 0;
        char * name = readdir(dp);
        while (name) {
            if (strcmp(name, "file.txt") == 0) {
                found = 1;
            }
            name = readdir(dp);
        }
        closedir(dp);
        if (found) {
            ok("opendir + readdir finds file.txt");
        } else {
            bad("opendir + readdir finds file.txt");
        }
    } else {
        bad("opendir returned null");
    }
    char cwd[1024];
    char * cp = getcwd(cwd, 1024);
    if (cp != 0 && strlen(cwd) > 0) {
        ok("getcwd");
    } else {
        bad("getcwd");
    }
    char * path_env = getenv("PATH");
    if (path_env != 0 && strlen(path_env) > 0) {
        ok("getenv PATH");
    } else {
        bad("getenv PATH");
    }
    char * nope = getenv("THIS_VAR_DOES_NOT_EXIST_XYZ");
    if (nope == 0) {
        ok("getenv missing returns null");
    } else {
        bad("getenv missing returns null");
    }
    rc = chdir(dir);
    if (rc == 0) {
        char inner[1024];
        getcwd(inner, 1024);
        int len = strlen(inner);
        int dlen = strlen(dir);
        int matches = len >= dlen &&
            memcmp(inner + len - dlen, dir, dlen) == 0;
        if (matches) {
            ok("chdir + getcwd shows new dir");
        } else {
            bad("chdir + getcwd shows new dir");
        }
        chdir("../..");
    } else {
        bad("chdir into intrin_dir");
    }
    rc = unlink(file);
    if (rc == 0) {
        ok("unlink");
    } else {
        bad("unlink");
    }
    rc = access(file, F_OK);
    if (rc != 0) {
        ok("file gone after unlink");
    } else {
        bad("file gone after unlink");
    }
    rc = rmdir(dir);
    if (rc == 0) {
        ok("rmdir");
    } else {
        bad("rmdir");
    }
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? -1 : 0;
}
