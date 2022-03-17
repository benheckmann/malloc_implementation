#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "membench.h"

#define BLOCK_BODY_SIZE 1024
#define BLOCK_SIZE 1048

typedef struct block {
    char block_body[BLOCK_BODY_SIZE];
} block;

typedef struct block_header {
    struct block_header *next_free_block;
    struct block *block;
    bool is_allocated;
} block_header;

#ifndef LOCAL_THREAD
static char *first_free_block = NULL;
static char *first_block_of_pool = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#else
_Thread_local static char *first_free_block = NULL;
_Thread_local static char *first_block_of_pool = NULL;
#endif

void *my_malloc(size_t size) {
    if (size > BLOCK_BODY_SIZE) return NULL;

#ifndef LOCAL_THREAD
    pthread_mutex_lock(&mutex);
#endif

    block_header *block = (block_header *) first_free_block;
    block->is_allocated = true;

    block_header *current_block = block;

    while (true) {
        if (!current_block->is_allocated) {
            first_free_block = (char *) current_block;
            block->next_free_block = NULL;

            break;
        }

        current_block = current_block->next_free_block;
    }

#ifndef LOCAL_THREAD
    pthread_mutex_unlock(&mutex);
#endif

    return (char *) block->block;
}

void my_free(void *ptr) {
#ifndef LOCAL_THREAD
    pthread_mutex_lock(&mutex);
#endif

    block_header *current_block = (block_header *) (((char *) ptr) - sizeof(block_header));
    current_block->is_allocated = false;

    block_header *next_block = (block_header *) (((char *) current_block) + BLOCK_SIZE);

    while (true) {
        if (next_block == NULL) {
            current_block->next_free_block = NULL;
            break;
        }

        if (!next_block->is_allocated) {
            current_block->next_free_block = next_block;
            break;
        }

        next_block = (block_header *) (((char *) next_block) + BLOCK_SIZE);
    }

    block_header *prev_block = (block_header *) (((char *) current_block) - BLOCK_SIZE);

    while (true) {
        if (((char *) prev_block) < first_block_of_pool) {
            first_free_block = (char *) current_block;
            break;
        }

        if (!prev_block->is_allocated) {
            prev_block->next_free_block = current_block;
            break;
        }

        prev_block = (block_header *) (((char *) prev_block) - BLOCK_SIZE);
    }

#ifndef LOCAL_THREAD
    pthread_mutex_unlock(&mutex);
#endif
}

void my_allocator_init(size_t size) {
    int32_t number_of_blocks = size / (sizeof(block_header) + sizeof(block));

    if (number_of_blocks == 0) return;

    first_free_block = (char *) malloc(sizeof(char *) * size);
    first_block_of_pool = first_free_block;

    for (int32_t i = 0; i < number_of_blocks; i++) {
        block_header *current_header = (block_header *) (first_free_block + (BLOCK_SIZE * i));

        if (i == number_of_blocks) {
            current_header->next_free_block = NULL;
        } else {
            current_header->next_free_block = (block_header *) (first_free_block + (BLOCK_SIZE * (i + 1)));
        }

        current_header->block = (block *) (((char *) current_header) + sizeof(block_header));
        current_header->is_allocated = false;
    }
}

void my_allocator_destroy() {
    free(first_free_block);
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
