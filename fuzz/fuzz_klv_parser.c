/*
 * AFL Fuzzing Harness for DSV4L2 KLV Parser
 *
 * Fuzzes the dsv4l2_parse_klv() function with random KLV data
 * to find crashes, hangs, and memory errors.
 *
 * Build with AFL:
 *   make fuzz
 *
 * Run fuzzing:
 *   afl-fuzz -i fuzz/seeds -o fuzz/findings -- ./fuzz/fuzz_klv_parser
 */

#include "dsv4l2_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define MAX_INPUT_SIZE (64 * 1024)  /* 64 KB max input */

int main(int argc, char **argv)
{
    uint8_t input_buf[MAX_INPUT_SIZE];
    ssize_t input_len;
    dsv4l2_klv_buffer_t klv_buffer;
    dsv4l2_klv_item_t *items = NULL;
    size_t item_count = 0;
    int rc;

    /* Read fuzzing input from stdin (AFL mode) or file (standalone mode) */
    if (argc > 1) {
        /* Standalone mode: read from file */
        FILE *f = fopen(argv[1], "rb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
            return 1;
        }
        input_len = fread(input_buf, 1, MAX_INPUT_SIZE, f);
        fclose(f);
    } else {
        /* AFL mode: read from stdin */
        input_len = read(STDIN_FILENO, input_buf, MAX_INPUT_SIZE);
    }

    /* Reject empty inputs */
    if (input_len <= 0) {
        return 0;
    }

    /* Set up KLV buffer */
    klv_buffer.data = input_buf;
    klv_buffer.size = input_len;
    klv_buffer.used = input_len;
    klv_buffer.format = DSV4L2_META_FORMAT_KLV;

    /* Fuzz target: Parse KLV metadata */
    rc = dsv4l2_parse_klv(&klv_buffer, &items, &item_count);

    /* Check for successful parse */
    if (rc == 0 && items != NULL) {
        /* Iterate through parsed items to exercise more code */
        for (size_t i = 0; i < item_count; i++) {
            /* Access item fields */
            volatile uint8_t key_byte = items[i].key.data[0];
            volatile size_t length = items[i].length;
            volatile const uint8_t *value = items[i].value;

            /* Prevent compiler optimization */
            (void)key_byte;
            (void)length;
            (void)value;
        }

        /* Test find function with first key (if any items) */
        if (item_count > 0) {
            const dsv4l2_klv_item_t *found =
                dsv4l2_find_klv_item(items, item_count, &items[0].key);
            (void)found;
        }

        /* Clean up */
        free(items);
    }

    return 0;
}
