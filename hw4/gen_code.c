/* $Id: gen_code.c,v 1.25 2023/11/28 22:12:58 leavens Exp $ */
#include <limits.h>
#include <string.h>
#include "ast.h"
#include "code_seq.h"
#include "id_use.h"
#include "code_utils.h"
#include "literal_table.h"
#include "gen_code.h"
#include "utilities.h"
#include "regname.h"

#define STACK_SPACE 4096

// Initialize the code generator
void gen_code_initialize()
{
    literal_table_initialize();
}

// Requires: bf if open for writing in binary
// and prior to this scope checking and type checking have been done.
// Write all the instructions in cs to bf in order
static void gen_code_output_seq(BOFFILE bf, code_seq cs)
{
    while (!code_seq_is_empty(cs))
    {
        bin_instr_t inst = code_seq_first(cs)->instr;
        instruction_write_bin_instr(bf, inst);
        cs = code_seq_rest(cs);
    }
}

// Return a header appropriate for the give code
static BOFHeader gen_code_program_header(code_seq main_cs)
{
    BOFHeader ret;

    // set magic number
    bof_write_magic_to_header(&ret);
    ret.text_start_address = 0;
    ret.text_length = code_seq_size(main_cs);

    // calculate data segment start address
    int dsa = MAX(ret.text_length, 1024);
    ret.data_start_address = dsa;

    // init data_length
    ret.data_length = literal_table_size();

    // calculate stack bottom address
    int sba = ret.data_start_address + ret.data_length + STACK_SPACE;
    ret.stack_bottom_addr = sba;

    return ret;
}

static void gen_code_output_literals(BOFFILE bf)
{
    literal_table_start_iteration();
    while (literal_table_iteration_has_next())
    {
        word_type w = literal_table_iteration_next();
        // debug_print("Writing literal %f to BOF file\n", w);
        bof_write_float(bf, w);
    }
    literal_table_end_iteration(); // not necessary
}

// Requires: bf is open for writing in binary
// Write the program's BOFFILE to bf
static void gen_code_output_program(BOFFILE bf, code_seq main_cs)
{
    BOFHeader bfh = gen_code_program_header(main_cs);
    bof_write_header(bf, bfh);
    gen_code_output_seq(bf, main_cs);
    gen_code_output_literals(bf);
    bof_close(bf);
}

// Requires: bf if open for writing in binary
// Generate code for prog into bf
void gen_code_program(BOFFILE bf, block_t prog)
{
    code_seq main_cs;
    // We want to make the main program's AR look like all blocks... so:
    // allocate space and initialize any variables
    main_cs = gen_code_var_decls(prog.var_decls);
    int vars_len_in_bytes = (code_seq_size(main_cs) / 2);
    int vars_len_in_bytes = (code_seq_size(main_cs) / 2);
    // there is no static link for the program as a whole,
    // so nothing to do for saving FP into A0 as would be done for a block
    main_cs = code_utils_set_up_program();
    code_seq_concat(&main_cs, gen_code_stmts(&prog.stmts));
    code_seq_concat(&main_cs, gen_code_stmts(&prog.stmts));
    code_seq_concat(&main_cs, code_utils_restore_registers_from_AR());
    code_seq_concat(&main_cs, code_utils_deallocate_stack_space(vars_len_in_bytes));
    main_cs = code_utils_tear_down_program();
    gen_code_output_program(bf, main_cs);
}

// Generate code for the var_decls_t vds to out
// There are 2 instructions generated for each identifier declared
// (one to allocate space and another to initialize that space)
code_seq gen_code_var_decls(var_decls_t vds)
{
    code_seq ret = code_seq_empty();
    var_decl_t *vdp = vds.var_decls;
    while (vdp != NULL)
    {
        // Generate these in reverse order,
        // so the addressing offsets work properly
        code_seq s1 = gen_code_var_decl(*vdp);
        code_seq_concat(&s1, ret);
        vdp = vdp->next;
    }
    return ret;
}

// Generate code for a single <var-decl>, vd,
// There are 2 instructions generated for each identifier declared
// (one to allocate space and another to initialize that space)
code_seq gen_code_var_decl(var_decl_t vd)
{
    return gen_code_idents(vd.ident_list);
}

// Generate code for the identififers in idents with type ident_t
// in reverse order (so the first declared are allocated last).
// There are 2 instructions generated for each identifier declared
// (one to allocate space and another to initialize that space)
code_seq gen_code_idents(ident_list_t idents)
{
    code_seq ret = code_seq_empty();
    ident_t *idp = idents.start;
    while (idp != NULL)
    {
        // Allocate space for the variable on the stack
        code_seq alloc_and_init = code_seq_singleton(code_addi(SP, SP, -BYTES_PER_WORD));

        // Initialize the allocated space to zero
        code_seq_add_to_end(&alloc_and_init, code_swr(0, 0, SP));

        // Concatenate the generated code in reverse order for proper addressing
        code_seq_concat(&alloc_and_init, ret);
        idp = idp->next;
    }
    return ret;
}

// Generate code for stmt
code_seq gen_code_stmt(stmt_t stmt)
{
    switch (stmt.stmt_kind)
    {
    case assign_stmt:
        return gen_code_assign_stmt(stmt.data.assign_stmt);
        break;
    case call_stmt:
        return gen_code_call_stmt(stmt.data.call_stmt);
        break;
    case if_stmt:
        return gen_code_if_stmt(stmt.data.if_stmt);
        break;
    case while_stmt:
        return gen_code_while_stmt(stmt.data.while_stmt);
    case read_stmt:
        return gen_code_read_stmt(stmt.data.read_stmt);
        break;
    case print_stmt:
        return gen_code_write_stmt(stmt.data.print_stmt);
        break;
    case block_stmt:
        return gen_code_block_stmt(stmt.data.block_stmt);
        break;
    default:
        bail_with_error("Call to gen_code_stmt with an AST that is not a statement!");
        break;
    }
    // The following can never execute, but this quiets gcc's warning
    return code_seq_empty();
}

// Generate code for the list of statements given by stmts
code_seq gen_code_stmts(stmts_t *stmts)
{
    code_seq ret = code_seq_empty();

    if (stmts->stmts_kind == empty_stmts_e)
    {
        // Empty statement list, return empty code sequence
        return ret;
    }
    else if (stmts->stmts_kind == stmt_list_e)
    {
        stmt_t *current_stmt = stmts->stmt_list.start;
        while (current_stmt != NULL)
        {
            code_seq stmt_code = gen_code_stmt(*current_stmt);
            code_seq_concat(&ret, stmt_code);
            current_stmt = current_stmt->next;
        }
    }
    else
    {
        bail_with_error("Invalid stmts_t kind!");
    }

    return ret;
}

// gen code for assign stmt
code_seq gen_code_assign_stmt(assign_stmt_t stmt)
{
    code_seq ret = code_seq_empty();

    // generate code for the expression and place the result on the stack
    code_seq expr_code = gen_code_expr(*stmt.expr);
    code_seq_concat(&ret, expr_code);

    // compute the frame pointer for variable access
    code_seq fp_code = code_utils_compute_fp(4, 0); // Use $r4 as temporary register
    code_seq_concat(&ret, fp_code);

    // generate code to store the result into the variable
    code_seq store_code = code_seq_empty();
    int var_offset = id_use_get_attrs(stmt.idu)->offset_count;

    code_seq_add_to_end(&store_code, code_swr(4, var_offset, 1)); // store from $r1
    code_seq_concat(&ret, store_code);

    return ret;
}

// Generate code for stmt
code_seq gen_code_call_stmt(call_stmt_t stmt)
{
    code_seq ret = code_seq_empty();

    // Generate code to call the function
    code_seq call_code = code_seq_singleton(code_call(stmt.name));
    code_seq_concat(&ret, call_code);

    return ret;
}

// Generate code for the if-statement given by stmt
code_seq gen_code_if_stmt(if_stmt_t stmt)
{
    code_seq ret = code_seq_empty();
    code_seq condition_code = code_seq_empty();

    // Generate code for the left expression of the condition
    code_seq left_code = gen_code_expr(stmt.condition.data.rel_op_cond.expr1);
    // Generate code for the right expression of the condition
    code_seq right_code = gen_code_expr(stmt.condition.data.rel_op_cond.expr2);

    // Combine the code sequences for the condition
    code_seq_concat(&ret, left_code);
    code_seq_concat(&ret, right_code);

    // Pop the right expression result into $r2
    code_seq_add_to_end(&condition_code, code_pop_register(2));

    // Pop the left expression result into $r1
    code_seq_add_to_end(&condition_code, code_pop_register(1));

    // Compare $r1 and $r2 based on the operator and store result in $r3
    switch (stmt.condition.cond_kind)
    {
    case ltsym:
        // $r3 = ($r1 < $r2) ? 1 : 0
        code_seq_add_to_end(&condition_code, code_lt(3, 0, 1, 2));
        break;
    case gtsym:
        // $r3 = ($r1 > $r2) ? 1 : 0
        code_seq_add_to_end(&condition_code, code_gt(3, 0, 1, 2));
        break;
    case eqsym:
        // $r3 = ($r1 == $r2) ? 1 : 0
        code_seq_add_to_end(&condition_code, code_eq(3, 0, 1, 2));
        break;
    // Add cases for other operators as needed
    default:
        fprintf(stderr, "Unsupported operator in condition\n");
        exit(1);
    }

    code_seq_concat(&ret, condition_code);

    // Pop the result into register $r1
    code_seq_add_to_end(&ret, code_pop_register(1));

    // Load zero into register $r2
    code_seq_add_to_end(&ret, code_lit(2, 0, 0));

    // Subtract $r2 from $r1, result in $r3
    code_seq_add_to_end(&ret, code_sub(3, 0, 1, 2));

    // Now, if $r3 is zero, condition was true
    // We'll use code_bne to branch if $r3 != 0 (i.e., condition is false)

    // Generate code for 'then' statements
    code_seq then_code = gen_code_stmts(stmt.then_stmts);
    int then_code_size = code_seq_size(then_code);

    if (stmt.else_stmts != NULL)
    {
        // Generate code for 'else' statements
        code_seq else_code = gen_code_stmts(stmt.else_stmts);
        int else_code_size = code_seq_size(else_code);

        immediate_type branch_to_else_offset = then_code_size + 2; // +2 for the jump over else code

        // Add branch instruction to skip 'then' code if condition is false
        code_seq_add_to_end(&ret, code_bne(3, 0, branch_to_else_offset));

        // Add 'then' code
        code_seq_concat(&ret, then_code);

        // Add jump over 'else' code
        immediate_type jump_over_else_offset = else_code_size + 1;
        code_seq_add_to_end(&ret, code_jrel(jump_over_else_offset));

        // Add 'else' code
        code_seq_concat(&ret, else_code);
    }
    else
    {
        // No 'else' part
        immediate_type branch_over_then_offset = then_code_size + 1; // +1 to skip 'then' code

        // Add branch instruction to skip 'then' code if condition is false
        code_seq_add_to_end(&ret, code_bne(3, 0, branch_over_then_offset));

        // Add 'then' code
        code_seq_concat(&ret, then_code);
    }

    return ret;
}

// Generate code for the while-statement given by stmt
code_seq gen_code_while_stmt(while_stmt_t stmt)
{
    code_seq ret = code_seq_empty();

    // Record the starting position of the loop
    int start_of_loop = code_seq_size(ret);

    // Generate code for the left expression of the condition
    code_seq left_code = gen_code_expr(stmt.condition.data.rel_op_cond.expr1);

    // Generate code for the right expression of the condition
    code_seq right_code = gen_code_expr(stmt.condition.data.rel_op_cond.expr2);

    // Combine the code sequences for the condition
    code_seq_concat(&ret, left_code);
    code_seq_concat(&ret, right_code);

    code_seq_add_to_end(&ret, 2);
    code_seq_add_to_end(&ret, 1);

    // Compare $r1 and $r2 based on the operator and store result in $r3
    switch (stmt.condition.cond_kind)
    {
    case ltsym:
        // $r3 = ($r1 < $r2) ? 1 : 0
        code_seq_add_to_end(&ret, code_lt(3, 0, 1, 2));
        break;
    case gtsym:
        // $r3 = ($r1 > $r2) ? 1 : 0
        code_seq_add_to_end(&ret, code_gt(3, 0, 1, 2));
        break;
    case eqsym:
        // $r3 = ($r1 == $r2) ? 1 : 0
        code_seq_add_to_end(&ret, code_eq(3, 0, 1, 2));
        break;
    // Add cases for other operators as needed
    default:
        fprintf(stderr, "Unsupported operator in condition\n");
        exit(1);
    }

    // Load zero into $r0 to use in comparison
    code_seq_add_to_end(&ret, code_lit(0, 0, 0));

    // Placeholder for the branch instruction (offset to be calculated later)
    instr_type branch_instr = code_bne(3, 0, 0); // Branch if $r3 != $r0
    code_seq_add_to_end(&ret, branch_instr);

    // Record the position of the branch instruction to update offset later
    int branch_instr_pos = code_seq_size(ret) - 1;

    // Generate code for the loop body
    code_seq body_code = gen_code_stmts(stmt.body);

    // Append the body code to the loop
    code_seq_concat(&ret, body_code);

    // After the body, add an unconditional jump back to the start of the loop
    int end_of_body = code_seq_size(ret);

    // Calculate the offset to jump back to the start of the loop
    int jump_back_offset = start_of_loop - end_of_body - 1; // -1 because code_jrel is relative to next instruction

    // Add the jump back instruction
    code_seq_add_to_end(&ret, code_jrel(jump_back_offset));

    // Now, calculate the offset to branch over the loop body if the condition is false
    int after_loop = code_seq_size(ret);

    // Add unconditional jump back to the start of the loop
    code_seq_add_to_end(&ret, code_jrel(jump_back_offset));

    return ret;
}

// Generate code for the read statment given by stmt
code_seq gen_code_read_stmt(read_stmt_t stmt)
{
    code_seq ret = code_seq_empty();
    code_seq_add_to_end(&ret, code_read(1, 0));
    int var_address = id_use_2_lexical_address(stmt.idu);
    code_seq_add_to_end(&ret, code_store(1, 0, var_address));

    return ret;
}

// Generate code for the print statment given by stmt.
code_seq gen_code_print_stmt(print_stmt_t stmt)
{
    code_seq ret = code_seq_empty();
    code_seq expr_code = gen_code_expr(stmt.expr);

    code_seq_concat(&ret, expr_code);
    code_seq_add_to_end(&ret, 1);

    code *print_instr = code_pint(1, 0); // Print the value in $r1
    code_seq_add_to_end(&ret, print_instr);

    return ret;
}

// Generate code for the block statement given by stmt.
code_seq gen_code_block_stmt(block_stmt_t stmt)
{
    code_seq ret = code_seq_empty();

    // handle Variable Declarations
    code_seq var_code = gen_code_var_decls(stmt.block->var_decls);
    code_seq_concat(&ret, var_code);

    // handle Statements
    code_seq stmts_code = gen_code_stmts(&stmt.block->stmts);
    code_seq_concat(&ret, stmts_code);

    return ret;
}

// Generate code for the expression exp
// putting the result on top of the stack,
// and using V0 and AT as temporary registers
// May also modify SP, HI,LO when executed
code_seq gen_code_expr(expr_t exp)
{
    switch (exp.expr_kind)
    {
    case expr_ident:
        return gen_code_ident(exp.data.ident);
        break;
    case expr_number:
        return gen_code_number(exp.data.number);
        break;
    default:
        bail_with_error("Unexpected expr_kind_e (%d) in gen_code_expr",
                        exp.expr_kind);
        break;
    }
    // never happens, but suppresses a warning from gcc
    return code_seq_empty();
}

// Generate code for the expression exp
// putting the result on top of the stack,
// and using V0 and AT as temporary registers
// May also modify SP, HI,LO when executed
code_seq gen_code_binary_op_expr(binary_op_expr_t exp)
{
    // put the values of the two subexpressions on the stack
    code_seq ret = gen_code_expr(*(exp.expr1));
    ret = code_seq_concat(ret, gen_code_expr(*(exp.expr2)));
    // check the types match
    type_exp_e t1 = ast_expr_type(*(exp.expr1));
    assert(ast_expr_type(*(exp.expr2)) == t1);
    // do the operation, putting the result on the stack
    ret = code_seq_concat(ret, gen_code_op(exp.op, t1));
    return ret;
}

// Generate code to apply op to the
// 2nd from top and top of the stack,
// putting the result on top of the stack in their place,
// and using V0 and AT as temporary registers
// Modifies SP when executed
code_seq gen_code_op(token_t op, type_exp_e typ)
{
    code_seq ret = code_seq_empty();
    code_seq_concat(&ret, code_pop_stack_into_reg(AT, typ));
    code_seq_concat(&ret, code_pop_stack_into_reg(V0, typ));
    code_seq_concat(&ret, code_pop_stack_into_reg(AT, typ));
    code_seq_concat(&ret, code_pop_stack_into_reg(V0, typ));

    code_seq operation = code_seq_empty();

    switch (op.code)
    {
    case eqsym:
        code_seq_singleton(code_cpr(V0, AT));
        code_seq_add_to_end(&operation, code_beq(V0, 0, 1)); // Conditional branch
        break;
    case neqsym:
        code_seq_singleton(code_cpr(V0, AT));
        code_seq_add_to_end(&operation, code_bne(V0, 0, 1));
        code_seq_singleton(code_cpr(V0, AT));
        code_seq_add_to_end(&operation, code_bne(V0, 0, 1));
        break;
    case ltsym:
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_bltz(V0, 1));
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_bltz(V0, 1));
        break;
    case leqsym:
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_blez(V0, 1));
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_blez(V0, 1));
        break;
    case gtsym:
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_bgtz(V0, 1));
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_bgtz(V0, 1));
        break;
    case geqsym:
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_bgez(V0, 1));
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_add_to_end(&operation, code_bgez(V0, 1));
        break;
    case plussym:
        code_seq_singleton(code_add(V0, 0, AT, 0));
        code_seq_singleton(code_add(V0, 0, AT, 0));
        break;
    case minussym:
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        code_seq_singleton(code_sub(V0, 0, AT, 0));
        break;
    case multsym:
        code_seq_singleton(code_mul(V0, 0));
        code_seq_singleton(code_mul(V0, 0));
        break;
    case divsym:
        code_seq_singleton(code_div(V0, 0));
        code_seq_singleton(code_div(V0, 0));
        break;
    default:
        bail_with_error("Unknown token code (%d) in gen_code_op",
                        op.code);
        break;
    }

    // push whatever result is back onto stack
    code_seq_concat(&operation, code_push_reg_on_stack(V0, typ));
    code_seq_concat(&ret, operation);

    return ret;
    code_seq_concat(&operation, code_push_reg_on_stack(V0, typ));
    code_seq_concat(&ret, operation);

    return ret;
}

// Generate code to apply the floating-point arith_op to the
// 2nd from top and top of the stack,
// putting the result on top of the stack in their place,
// and using V0 and AT as temporary registers
// Also modifies SP when executed
code_seq gen_code_arith_op(token_t arith_op)
{
    // load top of the stack (the second operand) into AT
    code_seq ret = code_pop_stack_into_reg(AT, float_te);
    // load next element of the stack into V0
    ret = code_seq_concat(ret, code_pop_stack_into_reg(V0, float_te));

    code_seq do_op = code_seq_empty();
    switch (arith_op.code)
    {
    case plussym:
        do_op = code_seq_add_to_end(do_op, code_fadd(V0, AT, V0));
        break;
    case minussym:
        do_op = code_seq_add_to_end(do_op, code_fsub(V0, AT, V0));
        break;
    case multsym:
        do_op = code_seq_add_to_end(do_op, code_fmul(V0, AT, V0));
        break;
    case divsym:
        do_op = code_seq_add_to_end(do_op, code_fdiv(V0, AT, V0));
        break;
    default:
        bail_with_error("Unexpected arithOp (%d) in gen_code_arith_op",
                        arith_op.code);
        break;
    }
    do_op = code_seq_concat(do_op, code_push_reg_on_stack(V0, float_te));
    return code_seq_concat(ret, do_op);
}

// Generate code for the rel_op
// applied to 2nd from top and top of the stack,
// putting the result on top of the stack in their place,
// and using V0 and AT as temporary registers
// Also modifies SP when executed
code_seq gen_code_rel_op(token_t rel_op, type_exp_e typ)
{
    // load top of the stack (the second operand) into AT
    code_seq ret = code_pop_stack_into_reg(AT, typ);
    // load next element of the stack into V0
    ret = code_seq_concat(ret, code_pop_stack_into_reg(V0, typ));

    // start out by doing the comparison
    // and skipping the next 2 instructions if it's true
    code_seq do_op = code_seq_empty();
    switch (rel_op.code)
    {
    case eqsym:
        if (typ == float_te)
        {
            do_op = code_seq_singleton(code_bfeq(V0, AT, 2));
        }
        else
        {
            do_op = code_seq_singleton(code_beq(V0, AT, 2));
        }
        break;
    case neqsym:
        if (typ == float_te)
        {
            do_op = code_seq_singleton(code_bfne(V0, AT, 2));
        }
        else
        {
            do_op = code_seq_singleton(code_bne(V0, AT, 2));
        }
        break;
    case ltsym:
        if (typ == float_te)
        {
            do_op = code_seq_singleton(code_fsub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_bfltz(V0, 2));
        }
        else
        {
            do_op = code_seq_singleton(code_sub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_bltz(V0, 2));
        }
        break;
    case leqsym:
        if (typ == float_te)
        {
            do_op = code_seq_singleton(code_fsub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_bflez(V0, 2));
        }
        else
        {
            do_op = code_seq_singleton(code_sub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_blez(V0, 2));
        }
        break;
    case gtsym:
        if (typ == float_te)
        {
            do_op = code_seq_singleton(code_fsub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_bfgtz(V0, 2));
        }
        else
        {
            do_op = code_seq_singleton(code_sub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_bgtz(V0, 2));
        }
        break;
    case geqsym:
        if (typ == float_te)
        {
            do_op = code_seq_singleton(code_fsub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_bfgez(V0, 2));
        }
        else
        {
            do_op = code_seq_singleton(code_sub(V0, AT, V0));
            do_op = code_seq_add_to_end(do_op, code_bgez(V0, 2));
        }
        break;
    default:
        bail_with_error("Unknown token code (%d) in gen_code_rel_op",
                        rel_op.code);
        break;
    }
    ret = code_seq_concat(ret, do_op);
    // rest of the code for the comparisons
    ret = code_seq_add_to_end(ret, code_add(0, 0, AT));  // put false in AT
    ret = code_seq_add_to_end(ret, code_beq(0, 0, 1));   // skip next instr
    ret = code_seq_add_to_end(ret, code_addi(0, AT, 1)); // put true in AT
    ret = code_seq_concat(ret, code_push_reg_on_stack(AT, bool_te));
    return ret;
}

// Generate code to put the value of the given identifier
// on top of the stack
// Modifies T9, V0, and SP when executed
code_seq gen_code_ident(ident_t id)
{
    assert(id.idu != NULL);
    code_seq ret = code_compute_fp(T9, id.idu->levelsOutward);
    assert(id_use_get_attrs(id.idu) != NULL);
    unsigned int offset_count = id_use_get_attrs(id.idu)->offset_count;
    assert(offset_count <= USHRT_MAX); // it has to fit!
    type_exp_e typ = id_use_get_attrs(id.idu)->type;
    if (typ == float_te)
    {
        ret = code_seq_add_to_end(ret,
                                  code_flw(T9, V0, offset_count));
    }
    else
    {
        ret = code_seq_add_to_end(ret,
                                  code_lw(T9, V0, offset_count));
    }
    return code_seq_concat(ret, code_push_reg_on_stack(V0, typ));
}

// Generate code to put the given number on top of the stack
code_seq gen_code_number(number_t num)
{
    code_seq ret = code_seq_empty();
    code_seq ret = code_seq_empty();
    unsigned int global_offset = literal_table_lookup(num.text, num.value);

    // Allocate space for the variable on the stack
    code_seq alloc_and_init = code_seq_singleton(code_addi(SP, SP, -BYTES_PER_WORD));

    // Initialize the allocated space to zero
    code_seq_add_to_end(&alloc_and_init, code_swr(0, global_offset, SP));

    code_seq_concat(&alloc_and_init, ret);
    return ret;
}

// Generate code for the expression exp
// putting the result on top of the stack,
// and using V0 and AT as temporary registers
// May also modify SP, HI,LO when executed
code_seq gen_code_logical_not_expr(expr_t exp)
{
    code_seq ret = gen_code_expr(exp);
    code_seq_concat(&ret, );
    code_seq_concat(&ret, );
    // if 0 skip next 2 instructions
    code_seq_add_to_end(&ret, code_beq(0, AT, 2));
    // it was 1, so put 0 in AT
    code_seq_add_to_end(&ret, code_add(0, 0, AT));
    // and skip the next instruction
    code_seq_add_to_end(&ret, code_beq(0, 0, 1));
    // put 1 in AT
    code_seq_add_to_end(&ret, code_addi(0, AT, 1));

    code_seq_add_to_end(&ret, code_addi(0, AT, 1));

    // push the result on the stack
    code_seq_concat(&ret, );
    return ret;
}