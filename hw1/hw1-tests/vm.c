#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bof.h"
#include "instruction.h"
#include "utilities.h"

#define MEMORY_SIZE_IN_WORDS 32768

static uword_type HI;
static uword_type LO;

static union mem_u
{
    word_type words[MEMORY_SIZE_IN_WORDS];
    uword_type uwords[MEMORY_SIZE_IN_WORDS];
    bin_instr_t instrs[MEMORY_SIZE_IN_WORDS];
} memory;

void print_bof(BOFFILE bof)
{
    // read the bof header
    BOFHeader header = bof_read_header(bof);

    // find the number of instructions
    word_type num_instructions = header.text_length;

    // print the header information
    // printf("BOF File: %s\n", bof.filename);
    // printf("Starting address: %u\n", header.text_start_address);
    // printf("Number of Instructions %u\n", num_instructions);
    printf("Address Instructions:\n");

    // loop through the instructions and print them
    for (word_type i = 0; i < num_instructions; i++)
    {
        // read the instruction
        bin_instr_t instruction = instruction_read(bof);
        // convert it to assembly
        const char *assembly_instruction = instruction_assembly_form(header.text_start_address + i, instruction);
        // print the instruction
        printf("%u: %s\n", i + header.text_start_address, assembly_instruction);
    }
}

void execute_instructions(BOFFILE bof)
{
    // read the bof header
    BOFHeader header = bof_read_header(bof);

    // initialize PC and SP
    word_type PC = header.text_start_address;
    word_type SP = header.stack_bottom_addr;
    word_type RA;
    word_type num_instructions = header.text_length;

    for (word_type i = 0; i < num_instructions; i++)
    {
        // put all instructions in memory
        memory.instrs[header.text_start_address + i] = instruction_read(bof);
    }

    bool running = true;
    while (running)
    {
        // fetch instruction from memory
        bin_instr_t instruction = memory.instrs[PC];

        // first determine the type of instruction
        instr_type type = instruction_type(instruction);

        // now the logic depends on the type of instruction
        switch (type)
        {
        // these are enums provided in instruction.h
        case comp_instr_type: // computational instructions
        {
            opcode_type opcode = instruction.comp.op;
            switch (opcode)
            {
            case NOP_F:
                break;
            // once we know the instruction type, we can use information from the instruction's struct
            // see instruction.h.  Then, use the format on the ssm-vm.pdf file to run the operations.
            case ADD_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[SP] + memory.words[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)];
                SP--;
                break;

            case SUB_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[SP] - memory.words[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)];
                SP--;
                break;

            case CPW_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)];
                break;

            case AND_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    memory.uwords[SP] & (memory.uwords[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)]);
                SP--;
                break;

            case BOR_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    memory.uwords[SP] | (memory.uwords[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)]);
                SP--;
                break;

            case NOR_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    ~memory.uwords[SP] | (memory.uwords[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)]);
                SP--;
                break;

            case XOR_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    memory.uwords[SP] ^ (memory.uwords[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)]);
                SP--;
                break;

            case LWR_F:
                instruction.comp.rt =
                    memory.words[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)];
                break;

            case SWR_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    instruction.comp.rs;
                break;

            case SCA_F: // store computed address
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    instruction.comp.rs + machine_types_formOffset(instruction.comp.os);
                break;

            case LWI_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)];
                break;

            case NEG_F:
                memory.words[instruction.comp.rt + machine_types_formOffset(instruction.comp.ot)] =
                    -memory.words[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)];
                break;

            default:
                printf("Uknown computation opcode...\n");
            }
            break;
        }

        // TODO: rest of them
        case other_comp_instr_type: // other computational instructions
        {
            opcode_type opcode = instruction.othc.op;
            switch (opcode)
            {
            case LIT_F:
                memory.words[instruction.othc.reg + machine_types_formOffset(instruction.othc.offset)] =
                    machine_types_sgnExt(instruction.othc.arg);

            case ARI_F:
                instruction.othc.reg = instruction.othc.reg + machine_types_sgnExt(instruction.othc.arg);
                break;

            case SRI_F:
                instruction.othc.reg = instruction.othc.reg - machine_types_sgnExt(instruction.othc.arg);
                break;

            case MUL_F:
                // fetch operands
                uword_type operand1 = memory.uwords[SP];                                                                  // top of the stack (memory[GPR[$sp]])
                uword_type operand2 = memory.uwords[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)]; // memory[GPR[s] + formOffset(o)]

                // perform the multiplication, which results in a 64-bit value
                uint64_t result = (uint64_t)operand1 * (uint64_t)operand2;

                // store the most significant 32 bits of the result in HI
                HI = (uword_type)(result >> 32);

                // store the least significant 32 bits of the result in LO
                LO = (uword_type)(result & 0xFFFFFFFF);

                // decrement the stack pointer
                SP--;
                break;

            case DIV_F:
                // fetch operands
                uword_type operand1 = memory.uwords[SP];                                                                  // top of the stack (memory[GPR[$sp]])
                uword_type operand2 = memory.uwords[instruction.comp.rs + machine_types_formOffset(instruction.comp.os)]; // memory[GPR[s] + formOffset(o)]

                // check for division by zero
                if (operand2 == 0)
                {
                    fprintf(stderr, "Division by zero error @ PC=%u\n", PC);
                    running = false; // stop execution
                    break;
                }

                // perform the division
                LO = operand1 / operand2; // store quotient in LO
                HI = operand1 % operand2; // store remainder in HI

                // decrement the stack pointer
                SP--;
                break;

            case CFHI_F:
                memory.words[instruction.othc.reg + machine_types_formOffset(instruction.othc.offset)] = HI;
                break;

            case CFLO_F:
                memory.words[instruction.othc.reg + machine_types_formOffset(instruction.othc.offset)] = LO;
                break;

            case SLL_F:
                memory.uwords[instruction.othc.reg + machine_types_formOffset(instruction.othc.offset)] =
                    memory.uwords[SP] << instruction.othc.arg;
                SP--;
                break;

            case SRL_F:
                memory.uwords[instruction.othc.reg + machine_types_formOffset(instruction.othc.offset)] =
                    memory.uwords[SP] >> instruction.othc.arg;
                SP--;
                break;

            case JMP_F:
                PC = memory.uwords[instruction.othc.reg + machine_types_formOffset(instruction.othc.offset)];
                break;

            case CSI_F:
                RA = PC; // Save the current program counter (PC) into the return address (RA)

                // calculate the new program counter (PC)
                word_type target_address = instruction.othc.reg + machine_types_formOffset(instruction.othc.offset);

                // check if the target address is within valid memory range
                if (target_address < 0 || target_address >= MEMORY_SIZE_IN_WORDS)
                {
                    fprintf(stderr, "Error: Attempting to jump to an invalid address: %u\n", target_address);
                    running = false; // stop execution
                    break;
                }

                PC = memory.words[target_address]; // update PC to the new address
                break;

            case JREL_F:
                PC = (PC - 1) + machine_types_formOffset(instruction.othc.offset);
                break;
            }
        }

        case syscall_instr_type: // system call instructions
        {
            opcode_type opcode = instruction.syscall.op;
            switch (opcode)
            {
            }
            break;
        }

        case immed_instr_type: // immediate instructions
        {
            opcode_type opcode = instruction.immed.op;
            switch (opcode)
            {
            case ADDI_O:
                memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] =
                    memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] + machine_types_sgnExt(instruction.immed.immed);
                break;

            case ANDI_O:
                memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] =
                    memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] & machine_types_zeroExt(instruction.immed.immed);
                break;

            case BORI_O:
                memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] =
                    memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] | machine_types_zeroExt(instruction.immed.immed);
                break;

            case NORI_O:
                memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] =
                    !(memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] | machine_types_zeroExt(instruction.immed.immed));
                break;

            case XORI_O:
                memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] =
                    memory.uwords[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)] ^ machine_types_zeroExt(instruction.immed.immed);
                break;

            case BEQ_O:
                if (memory.words[SP] == memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                SP--;
                break;

            case BGEZ_O:
                if (memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset) >= 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            case BGTZ_O:
                if (memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset) > 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            case BLEZ_O:
                if (memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset) <= 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            case BLTZ_O:
                if (memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset) < 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            case BNE_O:
                if (memory.words[SP] != memory.words[instruction.immed.reg + machine_types_formOffset(instruction.immed.offset)])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                SP--;
                break;
            }
            break;
        }

        case jump_instr_type: // jump instructions
        {
            opcode_type opcode = instruction.jump.op;
            switch (opcode)
            {
            // formAddress is essentially just taking the upper 4 bits of PC, and adding it to memory words
            case JMP_F:
                PC = machine_types_formAddress(PC - 1, instruction.jump.addr);
                break;

            case CALL_O:
                memory.words[SP] = PC;                                         // Save current PC (return address) on the stack
                SP--;                                                          // Decrement stack pointer
                PC = machine_types_formAddress(PC - 1, instruction.jump.addr); // Jump to function address
                break;

            case RTN_O:
                SP++;                  // Increment stack pointer to get the return address
                PC = memory.words[SP]; // Load the return address from the stack
                break;
            }
        }

        case error_instr_type: //  error instructions
        {
            fprintf(stderr, "Invalid instruction @ PC=%u", PC);
            running = false;
            break;
        }
        }

        PC++;
    }
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