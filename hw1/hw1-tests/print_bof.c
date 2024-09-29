#include "print_bof.h"
#include "instruction.h"

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