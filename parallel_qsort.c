#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define THRESHOLD 1000

int max_threads;
int active_threads = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct {
    struct timespec start;
    struct timespec end;
} sort_timer_t;

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

void parallel_qsort(int *arr, int n);

void* sort_thread(void *arg) {
    sort_args *args = (sort_args*)arg;
    parallel_qsort(args->arr, args->n);
    free(args);
    
    pthread_mutex_lock(&mutex);
    active_threads--;
    pthread_mutex_unlock(&mutex);
    
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
        while (i < n && arr[i] < pivot) i++;
        while (j >= 0 && arr[j] > pivot) j--;
        if (i <= j) {
            int tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            i++; j--;
        }
    }

    pthread_t left_thread = 0, right_thread = 0;
    int create_left = 0, create_right = 0;

    // Создание потока для левой части
    pthread_mutex_lock(&mutex);
    if (active_threads < max_threads && j + 1 > THRESHOLD) {
        active_threads++;
        create_left = 1;
    }
    pthread_mutex_unlock(&mutex);

    if (create_left) {
        sort_args *left_args = malloc(sizeof(sort_args));
        left_args->arr = arr;
        left_args->n = j + 1;
        pthread_create(&left_thread, NULL, sort_thread, left_args);
    } else if (j + 1 > 0) {
        parallel_qsort(arr, j + 1);
    }

    // Создание потока для правой части
    pthread_mutex_lock(&mutex);
    if (active_threads < max_threads && n - i > THRESHOLD) {
        active_threads++;
        create_right = 1;
    }
    pthread_mutex_unlock(&mutex);

    if (create_right) {
        sort_args *right_args = malloc(sizeof(sort_args));
        right_args->arr = arr + i;
        right_args->n = n - i;
        pthread_create(&right_thread, NULL, sort_thread, right_args);
    } else if (n - i > 0) {
        parallel_qsort(arr + i, n - i);
    }

    // Ожидание завершения потоков
    if (create_left) pthread_join(left_thread, NULL);
    if (create_right) pthread_join(right_thread, NULL);
}

void get_current_time(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

double get_elapsed_time_ms(const struct timespec *start, const struct timespec *end) {
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    
    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += 1000000000L;
    }
    
    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

double get_elapsed_time_sec(const struct timespec *start, const struct timespec *end) {
    return get_elapsed_time_ms(start, end) / 1000.0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <max_threads>\n", argv[0]);
        return 1;
    }

    max_threads = atoi(argv[1]);
    

    if (max_threads <= 0) {
        fprintf(stderr, "Error: max_threads must be positive\n");
        return 1;
    }
    
    int capacity = 1000;
    int *arr = malloc(capacity * sizeof(int));
    int n = 0;
    char buffer[1024];
    
    sort_timer_t total_timer;
    
    get_current_time(&total_timer.start);
    
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (n >= capacity) {
            capacity *= 2;
            arr = realloc(arr, capacity * sizeof(int));
        }
        arr[n++] = atoi(buffer);
    }

    fprintf(stderr, "Read %d numbers, sorting with %d threads\n", n, max_threads);
    
    struct timespec sort_start, sort_end;
    get_current_time(&sort_start);
    
    parallel_qsort(arr, n);
    
    get_current_time(&sort_end);
    
    get_current_time(&total_timer.end);
    
    for (int i = 0; i < n; i++) {
        printf("%d\n", arr[i]);
    }
    
    double sort_time = get_elapsed_time_ms(&sort_start, &sort_end);
    double total_time = get_elapsed_time_ms(&total_timer.start, &total_timer.end);
    double read_write_time = total_time - sort_time;
    
    fprintf(stderr, "=== TIMING RESULTS ===\n");
    fprintf(stderr, "Sorting time:    %.2f ms (%.3f seconds)\n", sort_time, sort_time / 1000.0);
    fprintf(stderr, "I/O time:        %.2f ms (reading + writing)\n", read_write_time);
    fprintf(stderr, "Total time:      %.2f ms (%.3f seconds)\n", total_time, total_time / 1000.0);
    fprintf(stderr, "Elements:        %d\n", n);
    fprintf(stderr, "Threads:         %d\n", max_threads);
    fprintf(stderr, "Performance:     %.0f elements/ms\n", n / sort_time);

    free(arr);
    return 0;
}