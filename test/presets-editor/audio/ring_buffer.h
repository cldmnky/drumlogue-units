#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

// Single-producer/single-consumer ring buffer for small messages
// Not thread-safe for multiple producers or multiple consumers.
typedef struct ring_buffer {
    uint32_t capacity;
    uint32_t mask;      // capacity - 1 (capacity must be power of two)
    atomic_uint write_pos;
    atomic_uint read_pos;
    void* data;         // byte buffer of item_size * capacity
    uint32_t item_size;
} ring_buffer_t;

int ring_buffer_init(ring_buffer_t* rb, uint32_t capacity_pow2, uint32_t item_size);
void ring_buffer_free(ring_buffer_t* rb);

// Returns 0 on success, -1 if full
int ring_buffer_push(ring_buffer_t* rb, const void* item);

// Returns 0 on success, -1 if empty
int ring_buffer_pop(ring_buffer_t* rb, void* out_item);

// Returns number of items available to pop
uint32_t ring_buffer_size(const ring_buffer_t* rb);
