#include "bof.h"
#include "machine_types.h"
#include "instruction.h"
#include "execute_instructions.h"

// TODO: all the opcodes

// a size for the memory ( 2 ^ 1 6 words = 32K words )
#define MEMORY_SIZE_IN_WORDS 32768

static union mem_u {
    word_type   words[MEMORY_SIZE_IN_WORDS];
    uword_type  uwords[MEMORY_SIZE_IN_WORDS];
    bin_instr_t instrs[MEMORY_SIZE_IN_WORDS];
} memory;

void execute_instructions(BOFFILE bof)
{
    // read the bof header
    BOFHeader header = bof_read_header(bof);

    // initialize PC and SP
    word_type PC = header.text_start_address;
    word_type SP = header.stack_bottom_addr;
    word_type num_instructions = header.text_length;

    // // make list of instructions
    // char **instructions = (char **)malloc(num_instructions * sizeof(char *));

    for (word_type i = 0; i < num_instructions; i++)
    {
        // put all instructions in memory
        memory.instrs[header.text_start_address + i] = instruction_read(bof);

        // i dont know if assembly form (string) is needed, i think thats for printing only?
        /*
        
            // convert it to assembly
            const char *assembly_instruction = instruction_assembly_form(header.text_start_address + i, instruction);

            // add it to instructions
            instructions[i] = assembly_instruction;
        
        */
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
            case comp_instr_type:       // computational instructions
            {
                opcode_type opcode = instruction.comp.op;
                switch (opcode)
                {
                    case ADD_F:
                        memory.words[SP] = memory.words[SP] + memory.words[SP - 1];
                        SP--;
                        break;
                    // TODO: rest of them
                    
                    default:
                        printf("Uknown computation opcode...\n");
                        break;
                }
                break;
            }

            case other_comp_instr_type: // other computational instructions
            {
                opcode_type opcode = instruction.othc.op;
                switch (opcode) 
                {

                }
                break;
            } 

            case syscall_instr_type:    // system call instructions
            {
                opcode_type opcode = instruction.syscall.op;
                switch (opcode) 
                {
                    
                }
                break;
            }

            case immed_instr_type:      // immediate instructions
            {
                opcode_type opcode = instruction.immed.op;
                switch (opcode) 
                {
                    
                }
                break;
            }

            case jump_instr_type:       // jump instructions
            {
                opcode_type opcode = instruction.jump.op;
                switch (opcode) 
                {
                    
                }
                break;
            }

            case error_instr_type:      //  error instructions
            {
                fprintf(stderr, "Invalid instruction @ PC=%u", PC);
                running = false;
                break;
            }

        }

        PC++;
    }
}