#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bof.h"
#include "instruction.h"
#include "utilities.h"
#include "vm.h"

// imported in vm.h
// void print_bof(BOFFILE bof);
// void execute_instructions(BOFFILE bof);
// void remove_spaces(char *s);

void print_bof(BOFFILE bof)
{
    // read the bof header
    BOFHeader header = bof_read_header(bof);

    // find the number of instructions
    word_type num_instructions = header.text_length;

    // print the header information

    // debug print functions
    // printf("BOF File: %s\n", bof.filename);
    // printf("Starting address: %u\n", header.text_start_address);
    // printf("Number of Instructions %u\n", num_instructions);
    printf("Address Instruction\n");

    // loop through the instructions and print them
    for (word_type i = 0; i < num_instructions; i++)
    {
        // read the instruction
        bin_instr_t instruction = instruction_read(bof);

        // convert it to assembly
        const char *assembly_instruction = instruction_assembly_form(header.text_start_address + i, instruction);

        // print the instruction
        // >> %6u reserves spaces needed
        printf("%6u: %s\n", i + header.text_start_address, assembly_instruction);

        // debug
        // const char *comp = instruction_compFunc2name(instruction);
        // const char *otherComp = instruction_otherCompFunc2name(instruction);

        // printf("comp: %s\n", comp);
        // printf("other comp: %s", otherComp);
    }
}

void execute_instructions(BOFFILE bof)
{
    // read the bof header
    BOFHeader header = bof_read_header(bof);

    // initialize PC and SP
    word_type PC = header.text_start_address;
    word_type SP = header.stack_bottom_addr;

    word_type num_instructions = header.text_length;

    // make list of instructions
    char **instructions = (char **)malloc(num_instructions * sizeof(char *));

    for (word_type i = 0; i < num_instructions; i++)
    {
        // read the instruction
        bin_instr_t instruction = instruction_read(bof);

        // convert it to assembly
        const char *assembly_instruction = instruction_assembly_form(header.text_start_address + i, instruction);

        // add it to instructions
        instructions[i] = assembly_instruction;
    }

    // bool running = true;
    // while (running)
    // {

    //     switch ()
    //     {
    //     }
    // }
}

int main(int argc, char *argv[])
{
    // if argc < 2 throw an error and provide usage instructions
    if (argc < 2)
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
