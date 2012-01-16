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
#include "csvformat.h"

#define CHECK_GROWBUF(gb) if (NULL == (gb)) { fprintf(stderr, "malloc failed!\n"); goto cleanup; }

#define GB_INIT 128

void add_field(size_t rownum, size_t colnum, growbuf *field, growbuf *fields, growbuf *field_lengths)
{
    if (colnum >= growbuf_num_elems(field_lengths, size_t)) {
        //
        // append a length.
        //
        size_t temp = field->size - 1;
        growbuf_append(field_lengths, &temp, sizeof(size_t));
    }
    else {
        //
        // check and update length
        //
        size_t len = growbuf_index(field_lengths, colnum, size_t);
        if (len < field->size - 1) {
            growbuf_index(field_lengths, colnum, size_t) = field->size - 1;
        }
    }

    //
    // check if new row is needed
    //
    if (rownum >= growbuf_num_elems(fields, growbuf*)) {
        growbuf *row = growbuf_create(GB_INIT);
        CHECK_GROWBUF(row)

        growbuf_append(fields, &row, sizeof(growbuf*));
    }

    //
    // append field to current row
    //
    growbuf *row = growbuf_index(fields, rownum, growbuf*);
    growbuf_append(row, &field, sizeof(growbuf*));

    cleanup:
    ;
}

void read_fields(growbuf *fields, growbuf *field_lengths, FILE *in)
{
    growbuf *current  = growbuf_create(10);
    size_t   rownum   = 0;
    size_t   colnum   = 0;
    bool     eof_seen = false;

    CHECK_GROWBUF(current)

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

                add_field(rownum, colnum, current, fields, field_lengths);
                
                colnum++;
                current = growbuf_create(GB_INIT);
                CHECK_GROWBUF(current)

                if ((char)c == '\n' || eof_seen) {
                    colnum = 0;
                    rownum++;
                    break; // next row
                }
            }
            else {
                growbuf_append(current, (char*)(&c), sizeof(char));
            }
        }
    } while (!eof_seen);

cleanup:
    if (current->size == 0) {
        growbuf_free(current);
    }
}

typedef struct {
    growbuf *fields, *field_lengths;
} csv_row_evaluator_context;

void csv_row_evaluator(growbuf *rowfields, size_t rownum, void *pcontext)
{
    csv_row_evaluator_context *context = (csv_row_evaluator_context*)pcontext;

    growbuf *fields        = context->fields;
    growbuf *field_lengths = context->field_lengths;

    for (size_t colnum = 0; colnum < growbuf_num_elems(rowfields, growbuf*); colnum++) {
        growbuf *field = growbuf_index(rowfields, colnum, growbuf*);

        add_field(rownum, colnum, field, fields, field_lengths);
    }
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
    bool     parse_flags   = true;
    bool     csv           = false;
    csv_row_evaluator_context csv_context = { .fields = NULL, .field_lengths = NULL };

    while (argc > 1) {
        if (0 == strcmp("--", argv[1])) {
            parse_flags = false;
            argv++;
            argc--;
        }
        else if (parse_flags && 
                    (0 == strcmp("-h", argv[1])
                        || 0 == strcmp("--help", argv[1])))
        {
             fprintf(stderr, 
"usage: %s [--csv] [infile ...]\n"
"\n"
"TAB-character delimited fields read from the given file(s) (or stdin if\n"
"none are given) are printed so that the starts of all fields line up with\n"
"each other, like a table.\n",
                argv[0]);
            retval = EX_USAGE;
            goto cleanup;
        }            
        else if (parse_flags && 0 == strcmp("--csv", argv[1])) {
            csv = true;
            argv++;
            argc--;
        }
        else {
            use_stdin = false;
            break;
        }
    }

    fields = growbuf_create(10);
    field_lengths = growbuf_create(10);
    if (NULL == fields || NULL == field_lengths) {
        fprintf(stderr, "failed to allocate growbuf\n");
        retval = EX_OSERR;
        goto cleanup;
    }

    if (csv) {
        csv_context.fields = fields;
        csv_context.field_lengths = field_lengths;
    }

    if (use_stdin) {
        if (csv) {
            read_csv(stdin, &csv_row_evaluator, &csv_context);
        }
        else {
            read_fields(fields, field_lengths, stdin);
        }
    }
    else {
        for (int i = 1; i < argc; i++) {
            FILE *in = fopen(argv[i], "r");
            if (NULL == in) {
                fprintf(stderr, "%s: ", argv[i]);
                perror("error opening file");
                retval = EX_NOINPUT;
            }

            if (csv) {
                read_csv(in, &csv_row_evaluator, &csv_context);
            }
            else {
                read_fields(fields, field_lengths, in);
            }
            
            fclose(in);
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
