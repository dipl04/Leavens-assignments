#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bof.h"
#include "instruction.h"
#include "utilities.h"
#include "vm.h"
#include "execute_instructions.h"
#include "print_bof.h"

#define MAGIC "BO32"

// imported in vm.h
// void print_bof(BOFFILE bof);
// void execute_instructions(BOFFILE bof);
// void remove_spaces(char *s);

// moved print_bof definition in print_bof.c

// moved execute_instructions definition in execute_instructions.c

int main(int argc, char *argv[])
{
    // if argc < 2 throw an error and provide usage instructions
    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage: %s [-p] <bof_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int print_mode = 0; // flag for if we are using -p option
    char *bof_file;

    // check for -p in the command line arguments
    if (argc == 3 && strcmp(argv[1], "-p") == 0)
    {
        print_mode = 1;
        bof_file = argv[2];
    }
    else
    {
        bof_file = argv[1];
    }

    // open and read the bof file
    BOFFILE bof = bof_read_open(bof_file);

    // check validity with magic number
    BOFHeader bof_head = bof_read_header(bof);
    if (!bof_has_correct_magic_number(bof_head)) { // ASCII for BO32
        fprintf(stderr, "Error: magic number in bof file (%s) should be (%s)", bof_head.magic, MAGIC);
        exit(EXIT_FAILURE);
    }

    // check which mode we are in
    if (print_mode)
    {
        // print the bof file
        print_bof(bof);
    }
    else
    {
        // execute the program
        execute_instructions(bof);
    }

    return 0;
}
