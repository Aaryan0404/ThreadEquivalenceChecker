#include "permutations.h"
#include "rpi.h"

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void permute(int **itl, int *arr, int start, int end, int *count) {
    if (start == end) {
        for (int i = 0; i <= end; i++) {
            itl[*count][i] = arr[i];
        }
        (*count)++;
    } else {
        for (int i = start; i <= end; i++) {
            swap(&arr[start], &arr[i]);
            permute(itl, arr, start + 1, end, count);
            swap(&arr[start], &arr[i]); // backtrack
        }
    }
}

int factorial(int n) {
    if (n == 0) return 1;
    int fact = 1;
    for (int i = 1; i <= n; i++) {
        fact *= i;
    }
    return fact;
}

void find_permutations(int **itl, int num_funcs) {
    int *arr = kmalloc(num_funcs * sizeof(int)); 
    for (int i = 0; i < num_funcs; i++) {
        arr[i] = i;
    }

    int count = 0;
    permute(itl, arr, 0, num_funcs - 1, &count);
}

int** get_func_permutations(int num_funcs){
    const size_t num_perms = factorial(num_funcs);
    int **itl = kmalloc(num_perms * sizeof(int *));
    for (int i = 0; i < num_perms; i++) {
        itl[i] = kmalloc(num_funcs * sizeof(int));
    }
    find_permutations(itl, num_funcs);
    return itl;
}