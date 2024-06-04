#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define QUEUE_SIZE 100
#define NUM_FUNCS 4
#define NUM_CTX 1

/// GPT GENERATED circular queue

// PROMPT: 
// Here is an example of a 
// thread-safe implementation of stack 
// with two consumers - now, I want you to 
// replicate this for a circular queue implementation 
// (define it custom like the stack) - and, define more 
// functions (like funcA and funcB) to make this a thread 
// safe implementation of multi-producer multi-consumer
// scenario - use the vibe_check locks api shown in the
// example, but to optimize concurrency, use a different lock 
// for the enqueue and dequeue operations.

// RESPONSE: Below

typedef struct {
    int data[QUEUE_SIZE];
    int front;
    int rear;
    vibe_check_t enqueue_lock;
    vibe_check_t dequeue_lock;
} AtomicQueue;

AtomicQueue queue;
int *global_value;
int *global_value2;

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
        return; 
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
    queue_enqueue(&queue, 10);
}

void consumerA(void **arg) {
    queue_dequeue(&queue, global_value2);
}

void consumerB(void **arg) {
    queue_dequeue(&queue, global_value);
}

void init_memory() {
  queue_init(&queue);

  *global_value = 0;
  *global_value2 = 0;
}

void notmain() {
    equiv_checker_init();
    
    global_value2 = kmalloc(sizeof(int));
    global_value = kmalloc(sizeof(int));

    function_exec executables[NUM_FUNCS];
    executables[0].func_addr = (func_ptr)producerA;
    executables[1].func_addr = (func_ptr)producerB;
    executables[2].func_addr = (func_ptr)consumerA;
    executables[3].func_addr = (func_ptr)consumerB;

    set_t* shared_mem_hint = NULL;

    memory_tags_t t = mk_tags(5);
    add_tag(&t, global_value, "global 1");
    add_tag(&t, global_value2, "global 2");
    add_tag(&t, &queue.front, "queue front");
    add_tag(&t, &queue.rear, "queue rear");
    add_tag(&t, &queue.data, "queue data");

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, shared_mem_hint, &t);
}
