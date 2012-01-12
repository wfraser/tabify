/*
 * Tabify
 *
 * by William R. Fraser, 1/11/2012
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sysexits.h>
#include <string.h>
#include <errno.h>

#include "growbuf.h"

#define CHECK_GROWBUF(gb) if (NULL == (gb)) { fprintf(stderr, "malloc failed!\n"); goto cleanup; }

void read_fields(growbuf *fields, growbuf *field_lengths, const char *filename)
{
    FILE* in = stdin;

    if (NULL != filename) {
        in = fopen(filename, "r");
        if (NULL == in) {
            fprintf(stderr, "%s:", filename);
            perror("open failed");
            goto cleanup;
        }
    }

    growbuf *row      = growbuf_create(10);
    growbuf *current  = growbuf_create(10);
    size_t   colnum   = 0;
    bool     eof_seen = false;

    CHECK_GROWBUF(current)
    CHECK_GROWBUF(row)

    do { // iterate over rows

        while (1) { // iterate over columns
            int c = fgetc(in);
            
            if (c == EOF) {
                eof_seen = true;

                if (current->size == 0) {
                    break;
                }
            }

            if ((char)c == '\t' || (char)c == '\n' || eof_seen) { 
                growbuf_append_byte(current, '\0');

                // update length
                if (colnum > growbuf_num_elems(field_lengths, size_t)) {
                    //
                    // append a length.
                    //
                    size_t temp = current->size - 1;
                    growbuf_append(field_lengths, &temp, sizeof(size_t));
                }
                else {
                    //
                    // check and update length
                    //
                    size_t len = growbuf_index(field_lengths, colnum, size_t);
                    if (len < current->size - 1) {
                        growbuf_index(field_lengths, colnum, size_t) = current->size - 1;
                    }
                }

                //
                // append field to current row
                //
                growbuf_append(row, &current, sizeof(growbuf*));
                
                colnum++;
                current = growbuf_create(10);
                CHECK_GROWBUF(current)

                if ((char)c == '\n' || eof_seen) {
                    //
                    // append current row to fields
                    //
                    growbuf_append(fields, &row, sizeof(growbuf*));
                    row = growbuf_create(10);
                    CHECK_GROWBUF(row)
                    colnum = 0;

                    break; // next row
                }
            }
            else {
                growbuf_append(current, (char*)(&c), sizeof(char));
            }
        }
    } while (!eof_seen);

cleanup:
    if (row->size == 0) {
        growbuf_free(row);
    }
    if (current->size == 0) {
        growbuf_free(current);
    }

    fclose(in);
}

void print_fields(growbuf *fields, growbuf *field_lengths)
{
    if (NULL == fields || NULL == field_lengths) {
        return;
    }

    for (size_t rownum = 0; rownum < growbuf_num_elems(fields, growbuf*); rownum++) {
        growbuf *row = growbuf_index(fields, rownum, growbuf*);
        for (size_t colnum = 0; colnum < growbuf_num_elems(row, growbuf*); colnum++) {
            growbuf *field = growbuf_index(row, colnum, growbuf*);
            size_t   len   = growbuf_index(field_lengths, colnum, size_t);

            size_t printed = printf("%s", (char*)(field->buf));
            for (size_t i = 0; i <= len - printed; i++) {
                printf(" ");
            }
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    int      retval        = EX_OK;
    bool     use_stdin     = true;
    growbuf *fields        = NULL;
    growbuf *field_lengths = NULL;

    if (argc > 1) {
        if (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0) {
            fprintf(stderr, 
"usage: %s [infile ...]\n"
"\n"
"TAB-character delimited fields read from the given file(s) (or stdin if\n"
"none are given) are printed so that the starts of all fields line up with\n"
"each other, like a table.\n",
                argv[0]);
            retval = EX_USAGE;
            goto cleanup;
        }
        else {
            use_stdin = false;
        }
    }

    fields = growbuf_create(10);
    field_lengths = growbuf_create(10);
    if (NULL == fields || NULL == field_lengths) {
        fprintf(stderr, "failed to allocate growbuf\n");
        retval = EX_OSERR;
        goto cleanup;
    }

    if (use_stdin) {
        read_fields(fields, field_lengths, NULL);
    }
    else {
        for (int i = 1; i < argc; i++) {
            read_fields(fields, field_lengths, argv[i]);
        }
    }

    print_fields(fields, field_lengths);

cleanup:
    if (NULL != fields) {
        for (size_t row = 0; row < growbuf_num_elems(fields, growbuf*); row++) {
            for (size_t field = 0;
                 field < growbuf_num_elems(growbuf_index(fields, row, growbuf*), growbuf*);
                 field++) 
            {
                growbuf_free(
                    growbuf_index(
                        growbuf_index(fields, row, growbuf*),
                        field,
                        growbuf*));
            }
            growbuf_free(growbuf_index(fields, row, growbuf*));
        }
        growbuf_free(fields);
    }

    if (NULL != field_lengths) {
        growbuf_free(field_lengths);
    }

    return retval;
}
