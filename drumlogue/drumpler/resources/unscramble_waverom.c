/*
 * Roland JV-880 / SR-JV80 Waverom Unscrambler
 *
 * Roland wave ROMs use address + data bit permutation ("scrambling").
 * This tool reads a scrambled waverom binary and writes the unscrambled output.
 *
 * Algorithm from jv880_juce rom.cpp unscrambleRom() and mcu.cpp unscramble().
 *
 * Usage: unscramble_waverom <input.bin> <output.bin>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Address bit permutation table (20-bit address space, 1MB blocks) */
static const int aa[20] = {
    2, 0, 3, 4, 1, 9, 13, 10, 18, 17, 6, 15, 11, 16, 8, 5, 12, 7, 14, 19
};

/* Data bit permutation table (8-bit) */
static const int dd[8] = {
    2, 0, 4, 5, 7, 6, 3, 1
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.bin> <output.bin>\n", argv[0]);
        fprintf(stderr, "Unscrambles Roland JV-880 / SR-JV80 wave ROM files.\n");
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];

    /* Read input file */
    FILE *fin = fopen(input_path, "rb");
    if (!fin) {
        fprintf(stderr, "Error: cannot open input file: %s\n", input_path);
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "Error: input file is empty: %s\n", input_path);
        fclose(fin);
        return 1;
    }

    uint8_t *src = (uint8_t *)malloc((size_t)file_size);
    uint8_t *dst = (uint8_t *)malloc((size_t)file_size);
    if (!src || !dst) {
        fprintf(stderr, "Error: failed to allocate %ld bytes\n", file_size);
        fclose(fin);
        free(src);
        free(dst);
        return 1;
    }

    size_t read_count = fread(src, 1, (size_t)file_size, fin);
    fclose(fin);

    if ((long)read_count != file_size) {
        fprintf(stderr, "Error: short read (%zu of %ld bytes)\n", read_count, file_size);
        free(src);
        free(dst);
        return 1;
    }

    /* Unscramble: address + data bit permutation */
    for (long i = 0; i < file_size; i++) {
        /* Address unscramble: permute address bits within 1MB (0x100000) blocks */
        int address = (int)(i & ~0xfffff);  /* Keep upper bits (block selector) */
        for (int j = 0; j < 20; j++) {
            if (i & (1 << j))
                address |= 1 << aa[j];
        }

        /* Bounds check (shouldn't happen with properly sized ROMs) */
        if (address < 0 || address >= file_size) {
            fprintf(stderr, "Error: address out of bounds at offset %ld → %d\n", i, address);
            free(src);
            free(dst);
            return 1;
        }

        /* Read source byte from scrambled address */
        uint8_t srcdata = src[address];

        /* Data unscramble: permute data bits */
        uint8_t data = 0;
        for (int j = 0; j < 8; j++) {
            if (srcdata & (1 << dd[j]))
                data |= 1 << j;
        }

        dst[i] = data;
    }

    /* Write output file */
    FILE *fout = fopen(output_path, "wb");
    if (!fout) {
        fprintf(stderr, "Error: cannot open output file: %s\n", output_path);
        free(src);
        free(dst);
        return 1;
    }

    size_t written = fwrite(dst, 1, (size_t)file_size, fout);
    fclose(fout);

    if ((long)written != file_size) {
        fprintf(stderr, "Error: short write (%zu of %ld bytes)\n", written, file_size);
        free(src);
        free(dst);
        return 1;
    }

    fprintf(stderr, "Unscrambled %ld bytes: %s → %s\n", file_size, input_path, output_path);

    free(src);
    free(dst);
    return 0;
}
