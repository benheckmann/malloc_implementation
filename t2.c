#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "membench.h"

typedef struct block_header {
    struct block_header *next_header;
    struct block_header *prev_header;
    char *block_start;
    size_t block_size;
    bool is_allocated;
} block_header;

#ifndef LOCAL_THREAD
static char *first_block_of_pool = NULL;
static char *max_address_of_pool = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#else
_Thread_local static char *first_block_of_pool = NULL;
_Thread_local static char *max_address_of_pool = NULL;
#endif

void *my_malloc(size_t size) {
#ifndef LOCAL_THREAD
    pthread_mutex_lock(&mutex);
#endif

    block_header *current_header = (block_header *) first_block_of_pool;

    while (true) {
        if (current_header->block_size < (size + sizeof(block_header)) || current_header->is_allocated) {
            if (current_header->next_header == NULL) {
                return NULL;
            }

            current_header = current_header->next_header;
            continue;
        }

        if ((((char *) current_header) + sizeof(block_header) + size) > max_address_of_pool) {
            return NULL;
        }

        block_header *new_split_block = (block_header *) (((char *) current_header) + sizeof(block_header) + size);
        new_split_block->prev_header = current_header;
        new_split_block->next_header = current_header->next_header;
        new_split_block->block_size = current_header->block_size - sizeof(block_header) - size;
        new_split_block->block_start = (((char *) new_split_block) + sizeof(block_header));
        new_split_block->is_allocated = false;

        current_header->next_header = new_split_block;
        current_header->block_size = size;
        current_header->block_start = (((char *) current_header) + sizeof(block_header));
        current_header->is_allocated = true;

        break;
    }
#ifndef LOCAL_THREAD
    pthread_mutex_unlock(&mutex);
#endif

    return (char *) current_header->block_start;
}

static void merge_with_left_block(block_header *current_header) {
    current_header->prev_header->next_header = current_header->next_header;
    current_header->prev_header->block_size = current_header->prev_header->block_size + current_header->block_size + sizeof(block_header);

    if (current_header->next_header == NULL) return;

    current_header->next_header->prev_header = current_header->prev_header;
}

void my_free(void *ptr) {
#ifndef LOCAL_THREAD
    pthread_mutex_lock(&mutex);
#endif

    block_header *current_header = (block_header *) (((char *) ptr) - sizeof(block_header));
    current_header->is_allocated = false;

    while (current_header->prev_header != NULL && !current_header->prev_header->is_allocated) {
        merge_with_left_block(current_header);

        current_header = current_header->prev_header;

        if (current_header->prev_header == NULL) break;
    }

    while (current_header->next_header != NULL && !current_header->next_header->is_allocated) {
        merge_with_left_block(current_header->next_header);

        if (current_header->next_header == NULL) break;

        current_header = current_header->next_header;
    }
#ifndef LOCAL_THREAD
    pthread_mutex_unlock(&mutex);
#endif
}

void my_allocator_init(size_t size) {
    first_block_of_pool = (char *) malloc(sizeof(char *) * size);

    block_header *current_header = (block_header *) first_block_of_pool;
    current_header->block_start = (((char *) current_header) + sizeof(block_header));
    current_header->block_size = size;
    current_header->prev_header = NULL;
    current_header->next_header = NULL;
    current_header->is_allocated = false;

    max_address_of_pool = first_block_of_pool + size;
}

void my_allocator_destroy() {
    free(first_block_of_pool);
}

int32_t main() {
#ifndef LOCAL_THREAD
    printf("Running membench global!\n");
    run_membench_global(my_allocator_init, my_allocator_destroy, my_malloc, my_free);
#else
    printf("Running membench local!\n");
    run_membench_thread_local(my_allocator_init, my_allocator_destroy, my_malloc, my_free);
#endif

    exit(EXIT_SUCCESS);
}
