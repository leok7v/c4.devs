#include <stdio.h>

typedef int (*compare_fn)(int, int);

int ascending(int a, int b) { return a - b; }
int descending(int a, int b) { return b - a; }

void bubble_sort(int *arr, int n, int (*cmp)(int, int)) {
    int i = 0;
    while (i < n - 1) {
        int j = 0;
        while (j < n - i - 1) {
            if (cmp(arr[j], arr[j + 1]) > 0) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

void print_array(int *arr, int n) {
    int i = 0;
    while (i < n) { printf("%d ", arr[i]); i = i + 1; }
    printf("\n");
}

int main() {
    int arr[5];
    int arr2[5];
    printf("Testing function pointers with bubble sort\n");
    arr[0] = 64; arr[1] = 34; arr[2] = 25; arr[3] = 12; arr[4] = 22;
    arr2[0] = 64; arr2[1] = 34; arr2[2] = 25; arr2[3] = 12; arr2[4] = 22;
    printf("Original: ");
    print_array(arr, 5);
    bubble_sort(arr, 5, ascending);
    printf("Ascending: ");
    print_array(arr, 5);
    if (arr[0] == 12 && arr[1] == 22 && arr[2] == 25 && 
        arr[3] == 34 && arr[4] == 64) {
        printf("OK: ascending sort\n");
    } else {
        printf("FAIL: ascending sort\n");
        return 1;
    }
    bubble_sort(arr2, 5, descending);
    printf("Descending: ");
    print_array(arr2, 5);
    if (arr2[0] == 64 && arr2[1] == 34 && arr2[2] == 25 && 
        arr2[3] == 22 && arr2[4] == 12) {
        printf("OK: descending sort\n");
    } else {
        printf("FAIL: descending sort\n");
        return 1;
    }
    compare_fn cmp;
    cmp = ascending;
    if (cmp(5, 3) == 2) { printf("OK: typedef fnptr\n");
    } else { printf("FAIL: typedef fnptr\n"); return 1;
    }
    cmp = descending;
    if (cmp(5, 3) == -2) { printf("OK: fnptr reassign\n");
    } else { printf("FAIL: fnptr reassign\n"); return 1;
    }
    printf("All function pointer tests passed!\n");
    return 0;
}
