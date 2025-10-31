#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define THRESHOLD 1000

int max_threads;
int active_threads = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int cmp(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

void seq_sort(int *arr, int n) {
    qsort(arr, n, sizeof(int), cmp);
}

typedef struct {
    int *arr;
    int n;
} sort_args;

void* parallel_qsort(void *arg);

void* sort_thread(void *arg) {
    sort_args *args = (sort_args*)arg;
    parallel_qsort(args->arr, args->n);
    free(args);
    return NULL;
}

void parallel_qsort(int *arr, int n) {
    if (n <= THRESHOLD) {
        seq_sort(arr, n);
        return;
    }

    int pivot = arr[n / 2];
    int i = 0, j = n - 1;
    
    while (i <= j) {
        while (arr[i] < pivot) i++;
        while (arr[j] > pivot) j--;
        if (i <= j) {
            int tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            i++; j--;
        }
    }

    pthread_t left_thread, right_thread;
    int left_created = 0, right_created = 0;
    sort_args *left_args = NULL, *right_args = NULL;

    pthread_mutex_lock(&mutex);
    if (active_threads < max_threads && j + 1 > THRESHOLD) {
        active_threads++;
        left_created = 1;
    }
    pthread_mutex_unlock(&mutex);

    if (left_created) {
        left_args = malloc(sizeof(sort_args));
        left_args->arr = arr;
        left_args->n = j + 1;
        pthread_create(&left_thread, NULL, sort_thread, left_args);
    } else {
        parallel_qsort(arr, j + 1);
    }

    pthread_mutex_lock(&mutex);
    if (active_threads < max_threads && n - i > THRESHOLD) {
        active_threads++;
        right_created = 1;
    }
    pthread_mutex_unlock(&mutex);

    if (right_created) {
        right_args = malloc(sizeof(sort_args));
        right_args->arr = arr + i;
        right_args->n = n - i;
        pthread_create(&right_thread, NULL, sort_thread, right_args);
    } else {
        parallel_qsort(arr + i, n - i);
    }

    if (left_created) {
        pthread_join(left_thread, NULL);
        pthread_mutex_lock(&mutex);
        active_threads--;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    if (right_created) {
        pthread_join(right_thread, NULL);
        pthread_mutex_lock(&mutex);
        active_threads--;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <max_threads>\n", argv[0]);
        return 1;
    }

    max_threads = atoi(argv[1]);
    int capacity = 1000;
    int *arr = malloc(capacity * sizeof(int));
    int n = 0, num;

    while (read(STDIN_FILENO, &num, sizeof(num)) > 0) {
        if (n >= capacity) {
            capacity *= 2;
            arr = realloc(arr, capacity * sizeof(int));
        }
        arr[n++] = num;
    }

    parallel_qsort(arr, n);

    for (int i = 0; i < n; i++) {
        write(STDOUT_FILENO, &arr[i], sizeof(int));
    }

    free(arr);
    return 0;
}