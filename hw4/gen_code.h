/* $Id: gen_code.h,v 1.7 2023/11/14 04:41:00 leavens Exp $ */
#ifndef _GEN_CODE_H
#define _GEN_CODE_H
#include <stdio.h>
#include "ast.h"
#include "bof.h"
#include "instruction.h"
#include "code.h"

// Initialize the code generator
extern void gen_code_initialize();

// Requires: bf if open for writing in binary
// Generate code for the given AST
extern void gen_code_program(BOFFILE bf, program_t prog);

// Requires: bf if open for writing in binary
// Generate code for prog into bf
extern void gen_code_program(BOFFILE bf, program_t prog);

// Generate code for the var_decls_t vds to out
// There are 2 instructions generated for each identifier declared
// (one to allocate space and another to initialize that space)
extern code_seq gen_code_var_decls(var_decls_t vds);

// Generate code for a single <var-decl>, vd,
// There are 2 instructions generated for each identifier declared
// (one to allocate space and another to initialize that space)
extern code_seq gen_code_var_decl(var_decl_t vd);

// Generate code for the identififers in idents with type t
// in reverse order (so the first declared are allocated last).
// There are 2 instructions generated for each identifier declared
// (one to allocate space and another to initialize that space)
extern code_seq gen_code_idents(idents_t idents,
				type_exp_e t);

// Generate code for stmt
extern code_seq gen_code_stmt(stmt_t stmt);

// Generate code for stmt
extern code_seq gen_code_assign_stmt(assign_stmt_t stmt);

// Generate code for stmt
extern code_seq gen_code_begin_stmt(begin_stmt_t stmt);

// Generate code for the list of statments given by stmts to out
extern code_seq gen_code_stmts(stmts_t stmts);

// Generate code for the if-statment given by stmt
extern code_seq gen_code_if_stmt(if_stmt_t stmt);

// Generate code for the read statment given by stmt
extern code_seq gen_code_read_stmt(read_stmt_t stmt);

// Generate code for the write statment given by stmt.
extern code_seq gen_code_write_stmt(write_stmt_t stmt);

// Generate code for the expression exp
// putting the result on top of the stack,
// and using V0 and AT as temporary registers
// May also modify SP, HI,LO when executed
extern code_seq gen_code_expr(expr_t exp);

// Generate code for the expression exp
// putting the result on top of the stack,
// and using V0 and AT as temporary registers
// May also modify SP, HI,LO when executed
extern code_seq gen_code_binary_op_expr(binary_op_expr_t exp);

// Generate code to apply op to the
// 2nd from top and top of the stack,
// putting the result on top of the stack in their place,
// and using V0 and AT as temporary registers
// May also modify SP, HI,LO when executed
extern code_seq gen_code_op(token_t op, type_exp_e typ);

// Generate code to apply the floating-point arith_op to the
// 2nd from top and top of the stack,
// putting the result on top of the stack in their place,
// and using V0 and AT as temporary registers
// Also modifies SP when executed
extern code_seq gen_code_arith_op(token_t arith_op);

// Generate code for the rel_op
// applied to 2nd from top and top of the stack,
// putting the result on top of the stack in their place,
// and using V0 and AT as temporary registers
// Also modifies SP when executed
extern code_seq gen_code_rel_op(token_t rel_op, type_exp_e typ);

// Generate code to put the value of the given identifier
// on top of the stack
// Modifies T9, V0, and SP when executed
extern code_seq gen_code_ident(ident_t id);

// Generate code to put the given number on top of the stack
// Modifies V0 when executed
extern code_seq gen_code_number(number_t num);

// Generate code for the expression exp
// putting the result on top of the stack,
// and using V0 and AT as temporary registers
// May also modify SP, HI,LO when executed
extern code_seq gen_code_logical_not_expr(expr_t exp);

#endif