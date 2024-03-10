/*********************************************************************
* Filename:   sha256.c
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Implementation of the SHA-256 hashing algorithm.
              SHA-256 is one of the three algorithms in the SHA2
              specification. The others, SHA-384 and SHA-512, are not
              offered in this implementation.
              Algorithm specification can be found here:
               * http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf
              This implementation uses little endian byte order.
*********************************************************************/

/*************************** HEADER FILES ***************************/
#include <stdlib.h>
#include <memory.h>
#include "sha256.h"

/****************************** MACROS ******************************/
#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

/**************************** VARIABLES *****************************/
static const WORD k[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

/*********************** FUNCTION DEFINITIONS ***********************/
void sha256_transform(SHA256_CTX *ctx, const BYTE data[])
{
	WORD a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

	for (i = 0, j = 0; i < 16; ++i, j += 4)
		m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
	for ( ; i < 64; ++i)
		m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for (i = 0; i < 64; ++i) {
		t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
		t2 = EP0(a) + MAJ(a,b,c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
	ctx->state[5] += f;
	ctx->state[6] += g;
	ctx->state[7] += h;
}

void sha256_init(SHA256_CTX *ctx)
{
	ctx->datalen = 0;
	ctx->bitlen = 0;
	ctx->state[0] = 0x6a09e667;
	ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372;
	ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f;
	ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab;
	ctx->state[7] = 0x5be0cd19;
}

void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len)
{
	WORD i;

	for (i = 0; i < len; ++i) {
		ctx->data[ctx->datalen] = data[i];
		ctx->datalen++;
		if (ctx->datalen == 64) {
			sha256_transform(ctx, ctx->data);
			ctx->bitlen += 512;
			ctx->datalen = 0;
		}
	}
}

// void sha256_update_state(SHA256_CTX *ctx, variable_state* var_states){
//     size_t num_vars = sizeof(var_states)/sizeof(var_states[0]);
//     size_t total_bytes = 0;
//     for(size_t i = 0; i < num_vars; i++){
//         total_bytes += var_states[i].num_bytes;
//     }
//     void* data = malloc(total_bytes);
//     void* curr_data_pointer = data;
//     for(size_t i = 0; i < num_vars; i++){
//         memcpy(curr_data_pointer, var_states[i].var_addr, var_states[i].num_bytes);
//         curr_data_pointer += var_states[i].num_bytes;
//     }

//     sha256_update(ctx, (BYTE*)data, total_bytes);
//     free(data);
// }

void sha256_final(SHA256_CTX *ctx, BYTE hash[])
{
	WORD i;

	i = ctx->datalen;

	// Pad whatever data is left in the buffer.
	if (ctx->datalen < 56) {
		ctx->data[i++] = 0x80;
		while (i < 56)
			ctx->data[i++] = 0x00;
	}
	else {
		ctx->data[i++] = 0x80;
		while (i < 64)
			ctx->data[i++] = 0x00;
		sha256_transform(ctx, ctx->data);
		memset(ctx->data, 0, 56);
	}

	// Append to the padding the total message's length in bits and transform.
	ctx->bitlen += ctx->datalen * 8;
	ctx->data[63] = ctx->bitlen;
	ctx->data[62] = ctx->bitlen >> 8;
	ctx->data[61] = ctx->bitlen >> 16;
	ctx->data[60] = ctx->bitlen >> 24;
	ctx->data[59] = ctx->bitlen >> 32;
	ctx->data[58] = ctx->bitlen >> 40;
	ctx->data[57] = ctx->bitlen >> 48;
	ctx->data[56] = ctx->bitlen >> 56;
	sha256_transform(ctx, ctx->data);

	// Since this implementation uses little endian byte ordering and SHA uses big endian,
	// reverse all the bytes when copying the final state to the output hash.
	for (i = 0; i < 4; ++i) {
		hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
	}
}

#include "rpi.h"
#include "state-space.h"

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void sha256_update_state(SHA256_CTX *ctx, variable_state* var_states){
    size_t num_vars = 3;
    for(size_t i = 0; i < num_vars; i++){
        sha256_update(ctx, (BYTE*)var_states[i].var_addr, var_states[i].num_bytes);
    }
}

void generate_permutations(int *arr, int start, int end, int np, int num_funcs, int permutations[][num_funcs], int *perm_idx) {
    if (start == end) {
        for (int i = 0; i < num_funcs; i++) {
            permutations[*perm_idx][i] = arr[i];
        }
        (*perm_idx)++;
        return;
    }

    for (int i = start; i <= end; i++) {
        swap(&arr[start], &arr[i]);
        generate_permutations(arr, start + 1, end, np, num_funcs, permutations, perm_idx);
        swap(&arr[start], &arr[i]); // Backtrack
    }
}

void duplicate_memory_state(variable_state* memory_state, size_t num_states, variable_state* new_state, void* variables[]) {
    for (int i = 0; i < num_states; i++) {        
        void *var_addr = kmalloc(memory_state[i].num_bytes);
        memcpy(var_addr, memory_state[i].var_addr, memory_state[i].num_bytes);

        new_state[i].num_bytes = memory_state[i].num_bytes;
        new_state[i].var_addr = var_addr;

        variables[i] = var_addr;
    }
}

void get_sequential_state_outcomes(void (*functions[])(void**), variable_state* memory_state, size_t num_funcs, size_t num_states) {
    // all permutations of the functions
    int np = 1; 
    for (int i = 1; i <= num_funcs; i++) {
        np *= i; 
    }

    int *sequence = kmalloc(num_funcs * sizeof(int));
    for (int i = 0; i < num_funcs; i++) {
        sequence[i] = i;
    }

    // create a 2d array
    // dim 0 = idx of permutation (from 0 to num_permutations - 1)
    // dim 1 = actual permutation of function idxs
    int permutations[np][num_funcs];
    int perm_idx = 0;
    generate_permutations(sequence, 0, num_funcs - 1, np, num_funcs, permutations, &perm_idx);

    uint32_t hash[np]; 

    // go through all perms
    for (int p_idx = 0; p_idx < np; p_idx++) {
        // retrieve sequence of function idxs
        int *sequence = permutations[p_idx];

        variable_state initial_mem_state[num_states]; 
        void* variables[num_states];
        duplicate_memory_state(memory_state, num_states, initial_mem_state, variables);
        
        for (int f_idx = 0; f_idx < num_funcs; f_idx++) {
            functions[sequence[f_idx]](variables);
            // printk("func: %d, x: %d, y[0]: %d, y[1]: %d, z: %d\n", sequence[f_idx], *(int *)variables[0], *(int *)variables[1], *((int *)variables[1] + 1), *(int *)variables[2]);
        }
        SHA256_CTX ctx;
        BYTE buf[SHA256_BLOCK_SIZE];
        sha256_init(&ctx);
        sha256_update_state(&ctx, initial_mem_state);
        sha256_final(&ctx, buf);
        uint32_t trunc_hash = *((uint32_t*)buf);
        hash[p_idx] = trunc_hash;
        // printk("hash: %x\n", trunc_hash);
        // printk("\n");

    }

    // print out the hash values
    for (int i = 0; i < np; i++) {
        printk("hash: %x\n", hash[i]);
    }
}


// USER CODE
void func0(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // set y[0] to 1
    *y = 1;

    // set y[1] to 2
    *(y + 1) = 2;
}

void func1(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // if y[0] is 1, set x to 2
    if (*y == 1) {
        *x = 2;
    }
}

void func2(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // if y[1] is 2, set x to 3
    if (*(y + 1) == 2) {
        *x = 3;
    }
}

void func3(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // set z to 1
    *z = 1;
}

void func4(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // increment z
    *z += 1;
}

void func5(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // multiply z by 2
    *z *= 2;
}

void notmain() {

    printk("Hello, world!\n"); 
    
    // array of function ptrs
    void (*functions[6])(void**) = {func0, func1, func2, func3, func4, func5};
    // initial state and then store in struct
    int x = 1;
    int y[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int z = 2; 

    variable_state states[3] = {{&x, sizeof(int)}, 
                                {&y[0], 10 * sizeof(int)}, 
                                {&z, sizeof(int)}};

    // run the function
    get_sequential_state_outcomes(functions, states, 6, 3);
}