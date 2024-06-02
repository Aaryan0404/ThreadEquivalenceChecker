#include "rpi.h"
#include <string.h>
#include <stdint.h>

#include "equiv-malloc.h"

#define STATUS_FREE 0x1
#define STATUS_USED 0x0
#define STATUS_MASK 0x1
#define SIZE_MASK ~STATUS_MASK

/* While in implicit this was convenience to avoid
 * zero sized blocks, this is now crucial to give enough
 * space in the free blocks for our pointers!
 */
#define MINIMUM_PAYLOAD 8 + 8 // 2 pointers

/* Type definitions */

typedef size_t metadata_t;
typedef struct header_t header_t;
typedef header_t *header_ptr;
struct header_t {
  metadata_t metadata;
  header_ptr previous_free;
  header_ptr next_free;
};

typedef struct heap_t {
  header_ptr first_free;
  size_t     size;
  header_ptr base;
  int        blocks_used;
  int        blocks_free;
  size_t     bytes_used;
  size_t     bytes_free;
} heap_t;

typedef unsigned char status_t;

/* Globals */

static heap_t heap;

/* Helpers */

/* Function: roundup
 * -----------------
 * This function rounds up a size sz to the nearest multiple of mult.
 */
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

/*
 * Function: get_status
 * --------------------
 * This function returns the status of a block
 */
status_t get_status(header_ptr header) {
  return header->metadata & STATUS_MASK;
}

/*
 * Function: get_size
 * ------------------
 * This function returns the size of a block
 */
size_t get_size(header_ptr header) {
  return header->metadata & SIZE_MASK;
}

/*
 * Function: set_status
 * --------------------
 * This function modifies the status of a block in-place
 */
void set_status(header_ptr header, status_t status) {
  header->metadata = get_size(header) | (status & STATUS_MASK);
}

/*
 * Function: set_size
 * ------------------
 * This function modifies the size of a block in-place
 */
void set_size(header_ptr header, size_t size) {
  header->metadata = (size & SIZE_MASK) | get_status(header);
}

/*
 * Function: get_payload_start
 * ---------------------------
 * This function computes the pointer to the first payload byte of a used block. If
 * header is a free block, behavior is undefined.
 */
void *get_payload_start(header_ptr header) {
  return (char *)header + sizeof(metadata_t);
}

/*
 * Function: get_payload_end
 * -------------------------
 * This function computes the pointer to the first byte after the payload of
 * a block.
 */
void *get_payload_end(header_ptr header) {
  size_t offset = sizeof(metadata_t) + get_size(header);
  char *end = (char *)header + offset;
  return (void *)end;
}

/*
 * Function: header_of_payload
 * ---------------------------
 * This function computes the pointer to the header of a given payload. The
 * payload pointer is expected to be the first byte of the payload. If it is
 * not, behavior is undefined. If payload is in a free block, behavior is
 * undefined.
 */
header_ptr header_of_payload(void *payload) {
  return (header_ptr)((char *)payload - sizeof(metadata_t));
}

/* Function: make_block
 * --------------------
 * This function is a shorthand to set the size and status of a block at location
 * loc at the same time
 */
void make_block(header_ptr loc, status_t status, size_t size) {
  set_size(loc, size);
  set_status(loc, status);
}

/* Function: next_free_block
 * -------------------------
 * This function advances a header pointer to the address of the next free block.
 * If there is no next free block (i.e. header is the last free block), this
 * function returns NULL. Otherwise, header is updated in place to point to
 * the next free block and the updated pointer is returned.
 *
 * If header is not free, behavior is undefined.
 */
header_ptr next_free_block(header_ptr *header) {
  header_ptr header_p = *header;
  // Get next block
  header_ptr next = header_p->next_free;
  if (next == NULL) {
    return NULL;
  }
  // Update in-place and return new header
  *header = next;
  return next;
}

/* Function: remove_free_block
 * ---------------------------
 * This function removes a free block from the free list. If header
 * is a used block, behavior is undefined
 */
void remove_free_block(header_ptr header) {
  header_ptr previous = header->previous_free;
  header_ptr next = header->next_free;
  if (previous == NULL) {
    // a NULL previous indicates that this is the first
    // free block and we should update our global accordingly
    heap.first_free = next;
  } else {
    previous->next_free = next;
  }
  if (next != NULL) {
    next->previous_free = previous;
  }
}

/* Function: append_free_block
 * ---------------------------
 * This function appends a free block to the free list. If
 * header is a used block, behavior is undefined
 */
void append_free_block(header_ptr header) {
  header_ptr old_first_free = heap.first_free;
  if (old_first_free != NULL) {
    header->next_free = old_first_free;
    header->previous_free = NULL;

    old_first_free->previous_free = header;
  }
  heap.first_free = header;
}

/* Function: next_block
 * --------------------
 * This function computes the address of the next block. If the next block is a
 * valid one (i.e. not off the heap), header is updated in-place to point to
 * the next block and its address is returned. However, if the block is invalid,
 * header is left unmodified and NULL is returned.
 */
header_ptr next_block(header_ptr *header) {
  void *end = get_payload_end(*header);
  // Is the end in bounds?
  if ((char *)end >= (char *)heap.base + heap.size) {
    return NULL;
  }
  // Update in-place and return new header
  *header = (header_ptr)end;
  return (header_ptr)end;
}

/* Function: coalesce_right
 * ------------------------
 * This function coalesces the free blocks to the right of block base into
 * block base. Each coalesce occurs in O(1) time, however the entire operation
 * is O(n) with n free blocks to the right of base. After the function has
 * finished, base is guaranteeed to be a free block that is immediately
 * succeeded by a used block or the end of the heap.
 */
void coalesce_right(header_ptr base) {
  header_ptr next = base;
  while(next_block(&next) != NULL && get_status(next) == STATUS_FREE) {
    // Coalesce this block in O(1)
    size_t new_space = get_size(next) + sizeof(metadata_t);
    set_size(base, get_size(base) + new_space);
    remove_free_block(next);

    // Update stats
    heap.blocks_free--;
    heap.bytes_free += sizeof(metadata_t);
  }
}

/* Funtion: trim_block_if_possible
 * -------------------------------
 * This function computes the excess space at the end of a block after needs
 * bytes has been allocated and creates a new free block out of said space if
 * it is possible.
 */
void trim_block_if_possible(header_ptr header, size_t needs) {
  size_t size = get_size(header);

  size_t excess = size - needs;
  if (excess >= sizeof(metadata_t) + MINIMUM_PAYLOAD) {
    // Our current block should be resized
    set_size(header, needs);

    // Make our new block at the end of our resized current block
    size_t excess_payload = excess - sizeof(metadata_t);
    // (we can make this cast because new_block is guaranteed to be
    // within the extent of our old block, thus it will always be valid)
    header_ptr new_block = (header_ptr)get_payload_end(header);
    make_block(new_block, STATUS_FREE, excess_payload);

    // Replace this free block in the free list with our new free block
    // (yes header is technically in use but it is effectively free)
    append_free_block(new_block);

    // Update stat variables
    heap.blocks_free++;
    heap.bytes_free += excess_payload;
    heap.bytes_used -= excess; // we entirely lose sizeof(metadata_t) bytes
  }
}

/* Function: allocate_block
 * ------------------------
 * This function allocates needs bytes in the block header. header must
 * already be a free block, otherwise behavior is undefined. The block
 * will be trimmed if possible and a new free block created with excess
 * space. A pointer to the first payload byte is returned.
 */
void *allocate_block(header_ptr header, size_t needs) {
  size_t size = get_size(header);

  set_status(header, STATUS_USED);

  // Update stat variables
  heap.blocks_free--;
  heap.blocks_used++;
  heap.bytes_free -= size;
  heap.bytes_used += size;

  // Can we make a new block?
  trim_block_if_possible(header, needs);

  // header is now inuse
  remove_free_block(header);

  return get_payload_start(header);
}

/* Allocator Implementation */

/*
 * Function: equiv_malloc_init
 * ----------------
 * This function initializes the global heap based on a base address and heap size. It
 * ensures that the resulting heap will be valid else it returns 0. If the resulting
 * heap state is valid, it returns 1.
 */
uint32_t equiv_malloc_init(void *heap_start, size_t heap_size) {
  // We must be bigger than our minimum size
  size_t minimum_size = sizeof(header_t) + MINIMUM_PAYLOAD;
  if (heap_size < minimum_size) {
    return 0;
  }

  // Initialize empty heap
  heap.base = heap_start;
  heap.first_free = heap_start;
  heap.size = heap_size;
  heap.bytes_used = 0;
  heap.blocks_used = 0;
  heap.bytes_free = 0;
  heap.bytes_used = 0;

  // The size of the first block is the entire heap minus the
  // size of the metadatadata
  size_t first_block_size = heap_size - sizeof(metadata_t);

  // Make our block
  make_block((header_ptr)heap.base, STATUS_FREE, first_block_size);

  // NULL initialize first linked list element
  heap.first_free->previous_free = NULL;
  heap.first_free->next_free = NULL;

  // Update statistics
  heap.bytes_free = first_block_size;
  heap.blocks_free = 1;

  return 1;
}

/*
 * Function: equiv_malloc
 * ------------------
 * This function satisfies an allocation request by searching all known
 * free blocks for the free block closest in size to the requested size.
 * If one is found but it is too large, this function with split the free
 * block to ony allocate the requested size (plus overhead) and nothing more.
 * In either case, it will return a pointer to the first byte of allocated
 * space if a suitable block is found, otherwise it returns NULL.
 *
 * A request of size 0 or of a size greater than MAX_REQUEST_SIZE is not
 * serviced and NULL is returned.
 */
void *equiv_malloc(size_t requested_size) {
    // Validate
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE) {
      return NULL;
    }

    // Minimum size
    if (requested_size < MINIMUM_PAYLOAD) {
      requested_size = MINIMUM_PAYLOAD;
    }

    // Align requested size (roundup)
    size_t needs = roundup(requested_size, ALIGNMENT);

    // Loop over free blocks to find the best one
    header_ptr best_block = NULL;
    size_t best_difference = SIZE_MAX;
    header_ptr current_block = heap.first_free;
    do {
      size_t size = get_size(current_block);
      if (size >= needs && size - needs < best_difference) {
        best_difference = size - needs;
        best_block = current_block;
      }
    } while(next_free_block(&current_block) != NULL);

    // If we found a valid block, allocate it
    if (best_block != NULL) {
      return allocate_block(best_block, needs);
    }

    // We could not service the request, so return NULL
    return NULL;
}

/* Function: equiv_free
 * ----------------
 * This function services a free request by flagging the provided block
 * as free and coalescing all free blocks to its right into it. If ptr
 * is NULL, nothing is done the function returns immediately.
 */
void equiv_free(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  // Free
  header_ptr base = header_of_payload(ptr);
  size_t size = get_size(base);
  set_status(base, STATUS_FREE);

  // Add to free list
  append_free_block(base);

  // Update stats
  heap.blocks_used--;
  heap.blocks_free++;
  heap.bytes_free += size;
  heap.bytes_used -= size;

  // Coalesce right
  coalesce_right(base);
}

/*
 * Function: equiv_realloc
 * -------------------
 * This function services a reallocation request. If the current allocation is
 * large enough to hold new_size, the same allocation is returned. Otherwise,
 * equiv_realloc will coalesce free blocks to the right of the current allocation into
 * the current allocation. If the post-coalesce block is large enough to hold
 * new_size, it is returned. If it is not, equiv_realloc will allocate a new block
 * to service the request. If this is not possible, it will return NULL, otherwise,
 * it will return the newly allocated block.
 *
 * If old_ptr is NULL, the request is treated as an allocation request of size
 * new_size, and if new_size is 0 it is treated as a free request of old_ptr.
 * However, if both old_ptr is NULL and new_size is 0, NULL is returned.
 * new_size must be at or below MAX_REQUEST_SIZE, otherwise NULL is returned.
 */
void *equiv_realloc(void *old_ptr, size_t new_size) {
  // Handle null pointer
  if (old_ptr == NULL) {
    return equiv_malloc(new_size);
  }

  // Handle 0 size
  if (new_size == 0) {
    equiv_free(old_ptr);
    return NULL;
  }

  // We assume that ptr is a valid payload pointer
  header_ptr header = header_of_payload(old_ptr);

  // Validate & align size
  if (new_size > MAX_REQUEST_SIZE) {
    return NULL;
  }
  size_t needs = roundup(new_size, ALIGNMENT);

  // Is the block already big enough?
  size_t old_size = get_size(header);
  if (old_size >= needs) {
    return old_ptr;
  }

  // Absorb free blocks to the right
  header_ptr right = header_of_payload(old_ptr);
  size_t additional_space = 0;
  if (next_block(&right) != NULL && get_status(right) == STATUS_FREE) {
    coalesce_right(right);

    // How much space did we gain?
    additional_space = get_size(right) + sizeof(metadata_t);

    // Merge the free block into the current block
    remove_free_block(right);
    set_size(header, old_size + additional_space);

    // Update stats
    heap.blocks_free--;
    heap.bytes_free -= get_size(right);
    heap.bytes_used += additional_space;
  }

  old_size += additional_space;


  // Did we make enough space?
  if (needs <= old_size) {
    // Trim down to just what we need
    trim_block_if_possible(header, needs);
    return old_ptr;
  }

  // Try to find a new spot
  void *new_block = equiv_malloc(needs);
  if (new_block == NULL) {
    return NULL;
  }

  // If we could service the request, move the data
  // We use old_size since that's guaranteed to all be payload
  memcpy(new_block, old_ptr, old_size);

  // Free the old allocation
  equiv_free(old_ptr);

  // Whew all done!
  return new_block;
}

/* Validation Code */

uint32_t validate_heap() {
    /* Validate whole heap */
    size_t header_bytes = (heap.blocks_used + heap.blocks_free) * sizeof(metadata_t);
    size_t payload_bytes = heap.bytes_used + heap.bytes_free;

    // All three of these cases should be impossible in a valid heap

    // Check for no used blocks but bytes used
    if (heap.blocks_used == 0 && heap.bytes_used > 0) {
      
      return 0;
    }
    // Check for no free blocks but bytes free
    if (heap.blocks_free == 0 && heap.bytes_free > 0) {
      
      return 0;
    }
    // Check for missing bytes
    if (header_bytes + payload_bytes != heap.size) {
      
      return 0;
    }

    /* Validate free list */
    int n_free = 0;
    header_ptr current_free_block = heap.first_free;
    do {
      // Every free list block must be free
      status_t status = get_status(current_free_block);
      if (status != STATUS_FREE) {
        
        return 0;
      }
      n_free++;
    } while(next_free_block(&current_free_block) != NULL);
    // We should have counted the same amount of free blocks
    if (n_free != heap.blocks_free) {
      
      return 0;
    }

    /* Validate individual blocks */
    header_ptr current_block = heap.base;
    do {
      // Sanity check block size
      size_t size = get_size(current_block);
      if (size >= heap.size || size == 0) {
        
        return 0;
      }
    } while(next_block(&current_block) != NULL);


    /* Everything is good. Yay! */
    return 1;
}

/*
 * Function: dump_block
 * --------------------
 * This function dumps a single block to stdout. It logs the payload address,
 * raw header, size, and status of the block.
 */
void dump_block(header_ptr header) {
  // Print block header
  size_t size = get_size(header);
  status_t status = get_status(header);
  printk("Block @ %x = %x : %d bytes : %c\n",
    header,
    header->metadata,
    size,
    status == STATUS_FREE ? 'F' : 'U'
  );
  if (status == STATUS_FREE) {
    printk("\t P: %x\n\t N: %x\n", header->previous_free, header->next_free);
  }
}

/* Function: dump_heap
 * -------------------
 * This function prints out the the block contents of the heap.  It is not
 * called anywhere, but is a useful helper function to call from gdb when
 * tracing through programs.  It prints out the total range of the heap, and
 * information about each block within it.
 */
void equiv_dump_heap() {
  header_ptr current_block = heap.base;
  printk("--- Heap ---\n");
  printk("Expected Size: %d bytes\n", heap.size);
  printk("Used: %d blocks / %d bytes\n", heap.blocks_used, heap.bytes_used);
  printk("Free: %d blocks / %d bytes\n", heap.blocks_free, heap.bytes_free);
  printk("\t First Free: %x\n", heap.first_free);
  printk("--- Blocks ---\n");
  int skipped_zeros = 0;
  do {
    // Skip zero blocks
    if (current_block->metadata == 0) {
      skipped_zeros++;
      continue;
    }
    if (skipped_zeros > 0) {
      printk("Skipped %d zero blocks\n", skipped_zeros);
      skipped_zeros = 0;
    }
    dump_block(current_block);
  } while(next_block(&current_block) != NULL);

}
