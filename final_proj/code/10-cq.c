#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define QUEUE_SIZE 100
#define NUM_VARS 2
#define NUM_FUNCS 4
#define load_store_mode 1

typedef int vibe_check_t;

extern int vibe_check(vibe_check_t *cur_vibes);
extern void secure_vibes(vibe_check_t *cur_vibes);
extern void release_vibes(vibe_check_t *cur_vibes);

vibe_check_t cur_vibes;

void vibe_init(vibe_check_t *cur_vibes) {
    *cur_vibes = 0;
}

typedef struct {
    int data[QUEUE_SIZE];
    int front;
    int rear;
    vibe_check_t enqueue_lock;
    vibe_check_t dequeue_lock;
} AtomicQueue;

AtomicQueue queue;
int *global_value;

void queue_init(AtomicQueue *queue) {
    queue->front = -1;
    queue->rear = -1;
    vibe_init(&queue->enqueue_lock);
    vibe_init(&queue->dequeue_lock);
}

void queue_enqueue(AtomicQueue *queue, int value) {
    secure_vibes(&queue->enqueue_lock);
    if ((queue->rear + 1) % QUEUE_SIZE == queue->front) {
        release_vibes(&queue->enqueue_lock);
        return;
    }
    if (queue->front == -1) {
        queue->front = 0;
    }
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    queue->data[queue->rear] = value;
    release_vibes(&queue->enqueue_lock);
}

void queue_dequeue(AtomicQueue *queue, int *value) {
    secure_vibes(&queue->dequeue_lock);
    if (queue->front == -1) {
        release_vibes(&queue->dequeue_lock);
        return; // Queue is empty
    }
    *value = queue->data[queue->front];
    if (queue->front == queue->rear) {
        queue->front = -1;
        queue->rear = -1;
    } else {
        queue->front = (queue->front + 1) % QUEUE_SIZE;
    }
    release_vibes(&queue->dequeue_lock);
}

void producerA(void **arg) {
    queue_enqueue(&queue, 10);
}

void producerB(void **arg) {
    queue_enqueue(&queue, 20);
}

void consumerA(void **arg) {
    queue_dequeue(&queue,global_value);
}

void consumerB(void **arg) {
    queue_dequeue(&queue, global_value);
}

void notmain() {
    queue_init(&queue);
    vibe_init(&cur_vibes);
    
    global_value = kmalloc(sizeof(int));
    *global_value = 0;

    int interleaved_ncs = 2;

    function_exec executables[NUM_FUNCS];
    executables[0].func_addr = (func_ptr)producerA;
    executables[1].func_addr = (func_ptr)producerB;
    executables[2].func_addr = (func_ptr)consumerA;
    executables[3].func_addr = (func_ptr)consumerB;

    int **itl = get_func_permutations(NUM_FUNCS);
    const size_t num_perms = factorial(NUM_FUNCS);
    uint64_t valid_hashes[num_perms];

    void *global_vars[NUM_VARS] = {(void *)global_value, (void *)&queue};
    size_t sizes[NUM_VARS] = {sizeof(int), sizeof(int) * (QUEUE_SIZE + 4)}; 
    
    memory_segments initial_mem_state = {NUM_VARS, (void **)global_vars, NULL, sizes};
    initialize_memory_state(&initial_mem_state);

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);

    run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
