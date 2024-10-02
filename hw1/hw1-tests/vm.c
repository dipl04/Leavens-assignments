#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bof.h"
#include "instruction.h"
#include "utilities.h"

#define MEMORY_SIZE_IN_WORDS 32768

static uword_type HI;
static uword_type LO;
static word_type GPR[8]; // general Purpose Registers
static word_type PC;     // program Counter
bool tracing_enabled = true;

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

    word_type data_start_addr = header.data_start_address;
    word_type data_length = header.data_length;

    if (data_length > 0)
    {
        word_type addr = data_start_addr;
        word_type data_word;

        for (word_type i = 0; i < data_length; i++)
        {
            data_word = bof_read_word(bof);

            if (i == 0)
            {
                printf("%u: %d", addr, data_word);
            }
            else if (i < 4)
            {
                printf("\t%u: %d", addr, data_word);
            }
            addr++;

            if (i == 3)
            {
                if (data_length > 4)
                {
                    printf("\t...\n");
                }
                else
                {
                    printf("\n");
                }
            }
        }

        // If data_length <= 4, ensure a newline is printed
        if (data_length <= 4)
        {
            printf("\n");
        }
    }
}

void execute_instructions(BOFFILE bof)
{
    // read the bof header
    BOFHeader header = bof_read_header(bof);

    // // initialize PC and SP
    // word_type GP = header.data_start_address;
    // word_type PC = header.text_start_address;
    // word_type SP = header.stack_bottom_addr;
    // word_type RA;
    word_type num_instructions = header.text_length;

    GPR[0] = header.data_start_address;    // $gp
    GPR[1] = header.stack_bottom_addr;     // $sp
    GPR[2] = header.stack_bottom_addr;     // $fp
    GPR[3] = GPR[4] = GPR[5] = GPR[6] = 0; // $r3 to $r6
    GPR[7] = 0;                            // $ra
    PC = header.text_start_address;

    word_type data_start_addr = header.data_start_address;
    word_type data_length = header.data_length;

   

    for (word_type i = 0; i < num_instructions; i++)
    {
        // put all instructions in memory
        memory.instrs[header.text_start_address + i] = instruction_read(bof);
        // const char *assembly_instruction = instruction_assembly_form(header.text_start_address + i, memory.instrs[header.text_start_address + i]);
        // printf("i just read in:::: %s\n", assembly_instruction);

    }

    for (word_type i = 0; i < data_length; i++) {
        memory.words[data_start_addr + i] = bof_read_word(bof);

    }

    bool running = true;
    int count = 0;
    while (running)
    {
        // fetch instruction from memory
        bin_instr_t instruction = memory.instrs[PC];
        // printf("%d", instruction.syscall.op);

         // trace current state
        if (tracing_enabled)
        {
            // print output like his .out files

            // registers
            printf("      PC: %d\n", PC);
            printf("GPR[$gp]: %d \tGPR[$sp]: %d \tGPR[$fp]: %d \tGPR[$r3]: %d \tGPR[$r4]: %d\n",
                   GPR[0], GPR[1], GPR[2], GPR[3], GPR[4]);

            printf("GPR[$r5]: %d \tGPR[$r6]: %d \tGPR[$ra]: %d\n",
                   GPR[5], GPR[6], GPR[7]);

            // data segment
            word_type addr = GPR[0]; // $gp
            int entries_printed = 0;

            while (addr < MEMORY_SIZE_IN_WORDS && entries_printed < 5)
            {
                word_type value = memory.words[addr];
                printf("    %d: %d\t", addr, value);
                addr++;
                entries_printed++;
            }
            printf("\n        ...\n");

            // print stack
            word_type start_addr = GPR[1]; // $sp
            word_type end_addr = GPR[2];   // $fp

            if (start_addr > end_addr)
            {
                word_type temp = start_addr;
                start_addr = end_addr;
                end_addr = temp;
            }

            addr = start_addr;
            while (addr <= end_addr && addr < MEMORY_SIZE_IN_WORDS)
            {
                word_type value = memory.words[addr];
                printf("    %d: %d\t", addr, value);
                addr++;
            }
            printf("\n");
            printf("\n");

            // assembly form at the end
            const char *instr_str = instruction_assembly_form(PC, instruction);
            printf("==>      %d: %s\n", PC, instr_str);
        }



        // first determine the type of instruction
        instr_type type = instruction_type(instruction);

        PC++;
        // now the logic depends on the type of instruction
        switch (type)
        {
        // these are enums provided in instruction.h
        case comp_instr_type: // computational instructions
        {
            func_type func = instruction.comp.func;
            switch (func)
            {
            case NOP_F:
                break;
            // once we know the instruction type, we can use information from the instruction's struct
            // see instruction.h.  Then, use the format on the ssm-vm.pdf file to run the operations.
            case ADD_F:
                memory.words[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[GPR[1]] + memory.words[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                //  // $sp
                break;

            case SUB_F:
                memory.words[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[GPR[1]] - memory.words[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                // 
                break;

            case CPW_F:
                memory.words[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                break;

            case AND_F:
                memory.uwords[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    memory.uwords[GPR[1]] & memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                // 
                break;

            case BOR_F:
                memory.uwords[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    memory.uwords[GPR[1]] | memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                // 
                break;

            case NOR_F:
                memory.uwords[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    ~(memory.uwords[GPR[1]] | memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)]);
                // 
                break;

            case XOR_F:
                memory.uwords[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    memory.uwords[GPR[1]] ^ memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                // 
                break;

            case LWR_F:
                GPR[instruction.comp.rt] =
                    memory.words[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                break;

            case SWR_F:
                memory.words[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    GPR[instruction.comp.rs];
                break;

            case SCA_F: // store computed address
                memory.words[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os);
                break;

            case LWI_F:
                memory.words[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    memory.words[memory.words[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)]];
                break;

            case NEG_F:
                memory.words[GPR[instruction.comp.rt] + machine_types_formOffset(instruction.comp.ot)] =
                    -memory.words[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)];
                break;

            default:
                printf("Uknown computation opcode...\n");
                break;
            }
            break;
        }

        case other_comp_instr_type: // other computational instructions
        {
            // printf("%d", other_comp_instr_type);
            // printf("in other comp");
            func_type func = instruction.othc.func;
            switch (func)
            {
            case LIT_F:
                memory.words[GPR[instruction.othc.reg] + machine_types_formOffset(instruction.othc.offset)] =
                    machine_types_sgnExt(instruction.othc.arg);
                break;

            case ARI_F:
                GPR[instruction.othc.reg] = GPR[instruction.othc.reg] + machine_types_sgnExt(instruction.othc.arg);
                break;

            case SRI_F:
                GPR[instruction.othc.reg] = GPR[instruction.othc.reg] - machine_types_sgnExt(instruction.othc.arg);
                break;

            case MUL_F:

                // store the most significant 32 bits of the result in HI
                HI = (uword_type)(memory.uwords[GPR[1]] * (uint64_t)memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)]) >> 32;

                // store the least significant 32 bits of the result in LO
                LO = (uword_type)(memory.uwords[GPR[1]] * (uint64_t)memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)]) & 0xFFFFFFFF;

                // decrement the stack pointer
                
                break;

            case DIV_F:
                // check for division by zero
                if (memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)] == 0)
                {
                    fprintf(stderr, "Division by zero error @ PC=%u\n", PC);
                    running = false; // stop execution
                    break;
                }

                // perform the division
                LO = memory.uwords[GPR[1]] / memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)]; // store quotient in LO
                HI = memory.uwords[GPR[1]] % memory.uwords[GPR[instruction.comp.rs] + machine_types_formOffset(instruction.comp.os)]; // store remainder in HI

                // decrement the stack pointer
                
                break;

            case CFHI_F:
                memory.words[GPR[instruction.othc.reg] + machine_types_formOffset(instruction.othc.offset)] = HI;
                break;

            case CFLO_F:
                memory.words[GPR[instruction.othc.reg] + machine_types_formOffset(instruction.othc.offset)] = LO;
                break;

            case SLL_F:
                memory.uwords[GPR[instruction.othc.reg] + machine_types_formOffset(instruction.othc.offset)] =
                    memory.uwords[GPR[1]] << instruction.othc.arg;
                
                break;

            case SRL_F:
                memory.uwords[GPR[instruction.othc.reg] + machine_types_formOffset(instruction.othc.offset)] =
                    memory.uwords[GPR[1]] >> instruction.othc.arg;
                
                break;

            case JMP_F:
                PC = memory.uwords[GPR[instruction.othc.reg] + machine_types_formOffset(instruction.othc.offset)];
                break;

            case CSI_F:
                GPR[7] = PC; // Save the current program counter (PC) into the return address (RA)

                PC = memory.words[GPR[instruction.othc.reg] + machine_types_formOffset(instruction.othc.offset)]; // update PC to the new address
                break;

            case JREL_F:
                PC = (PC - 1) + machine_types_formOffset(instruction.othc.offset);
                break;
                
            }

            break;
        }

        case syscall_instr_type: // system call instructions
        {
            // printf("%d", syscall_instr_type);
            // printf("in syscall\n");
            syscall_type code = instruction.syscall.code;
            switch (code)
            {


            case print_str_sc:
                memory.words[GPR[1]] = printf("%s", (char *)&memory.words[GPR[instruction.syscall.reg] + machine_types_formOffset(instruction.syscall.offset)]);
                break;

            case print_char_sc:
                memory.words[GPR[1]] = fputc(memory.words[GPR[instruction.syscall.reg] + machine_types_formOffset(instruction.syscall.offset)], stdout);
                break;

            case read_char_sc:
                memory.words[GPR[instruction.syscall.reg] + machine_types_formOffset(instruction.syscall.offset)] = getc(stdin);
                break;

            case start_tracing_sc:
                // printf("start tracing");
                tracing_enabled = true;
                break;

            case stop_tracing_sc:
                // printf("stopped tracing");
                tracing_enabled = false;
                break;
            
            case exit_sc:
                // printf("exit_sc");
                running = false;
                break;
            }
            
            break;
        }

        case immed_instr_type: // immediate instructions
        {
            opcode_type opcode = instruction.immed.op;
            switch (opcode)
            {
            case ADDI_O:
                memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] =
                    memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] + machine_types_sgnExt(instruction.immed.immed);
                break;

            case ANDI_O:
                memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] =
                    memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] & machine_types_zeroExt(instruction.immed.immed);
                break;

            case BORI_O:
                memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] =
                    memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] | machine_types_zeroExt(instruction.immed.immed);
                break;

            case NORI_O:
                memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] =
                    !(memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] | machine_types_zeroExt(instruction.immed.immed));
                break;

            case XORI_O:
                memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] =
                    memory.uwords[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)] ^ machine_types_zeroExt(instruction.immed.immed);
                break;

            // GPR[1] is the stack pointer
            case BEQ_O:
                if (memory.words[GPR[1]] == memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                
                break;

            case BGEZ_O:
                if (memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset) >= 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            case BGTZ_O:
                if (memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset) > 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            case BLEZ_O:
                if (memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset) <= 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            case BLTZ_O:
                if (memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset) < 0])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                break;

            // GPR[1] is our stack pointer
            case BNE_O:
                if (memory.words[GPR[1]] != memory.words[GPR[instruction.immed.reg] + machine_types_formOffset(instruction.immed.offset)])
                {
                    PC = (PC - 1) + machine_types_formOffset(instruction.immed.immed);
                }
                
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
                GPR[7] = PC;
                PC = machine_types_formAddress(PC - 1, instruction.jump.addr);
                break;

            case RTN_O:
                PC = GPR[7];
                break;
            }
            break;
        }

        case error_instr_type: //  error instructions
        {
            printf("error");
            fprintf(stderr, "Invalid instruction @ PC=%u", PC);
            running = false;
            break;
        }
        }

        // printf("incrementing PC, %d, %d", tracing_enabled, running);

        // printf("PC: %u\n", PC);
        // printf("tracing: %d\n", tracing_enabled);
        // printf("running: %d", running);
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