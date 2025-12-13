#include "ring_buffer.h"

#include <stdlib.h>
#include <string.h>

static uint32_t is_power_of_two(uint32_t x) {
    return x && ((x & (x - 1)) == 0);
}

int ring_buffer_init(ring_buffer_t* rb, uint32_t capacity_pow2, uint32_t item_size) {
    if (!rb || !is_power_of_two(capacity_pow2) || item_size == 0) {
        return -1;
    }
    memset(rb, 0, sizeof(*rb));
    rb->capacity = capacity_pow2;
    rb->mask = capacity_pow2 - 1;
    rb->item_size = item_size;
    rb->data = calloc(capacity_pow2, item_size);
    if (!rb->data) {
        return -1;
    }
    atomic_init(&rb->write_pos, 0);
    atomic_init(&rb->read_pos, 0);
    return 0;
}

void ring_buffer_free(ring_buffer_t* rb) {
    if (!rb) return;
    free(rb->data);
    rb->data = NULL;
    rb->capacity = 0;
}

int ring_buffer_push(ring_buffer_t* rb, const void* item) {
    uint32_t write = atomic_load_explicit(&rb->write_pos, memory_order_relaxed);
    uint32_t read = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    if (((write + 1) & rb->mask) == (read & rb->mask)) {
        return -1;  // full
    }
    uint8_t* base = (uint8_t*)rb->data;
    memcpy(base + (write & rb->mask) * rb->item_size, item, rb->item_size);
    atomic_store_explicit(&rb->write_pos, write + 1, memory_order_release);
    return 0;
}

int ring_buffer_pop(ring_buffer_t* rb, void* out_item) {
    uint32_t read = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    uint32_t write = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    if ((read & rb->mask) == (write & rb->mask)) {
        return -1;  // empty
    }
    uint8_t* base = (uint8_t*)rb->data;
    memcpy(out_item, base + (read & rb->mask) * rb->item_size, rb->item_size);
    atomic_store_explicit(&rb->read_pos, read + 1, memory_order_release);
    return 0;
}

uint32_t ring_buffer_size(const ring_buffer_t* rb) {
    uint32_t write = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    uint32_t read = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    return (write - read) & rb->mask;
}
