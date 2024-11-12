/* $Id: scope_check.c,v 1.20 2023/11/13 14:10:00 leavens Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scope_check.h"
#include "id_attrs.h"
#include "file_location.h"
#include "ast.h"
#include "utilities.h"
#include "symtab.h"
#include "scope_check.h"

// Build the symbol table for prog
// and check for duplicate declarations
// or uses of undeclared identifiers
// Return the modified AST with id_use pointers
block_t scope_check_program(block_t prog)
{
    symtab_enter_scope();
    scope_check_varDecls(prog.var_decls);
    // need to update stmt's AST with id_use structs
    prog.stmts = scope_check_stmt(prog.stmts);
    symtab_leave_scope();
    return prog;
}

// build the symbol table and check the declarations in vds
void scope_check_varDecls(var_decls_t vds)
{
    var_decl_t *vdp = vds.var_decls;
    while (vdp != NULL) {
	scope_check_varDecl(*vdp);
	vdp = vdp->next;
    }
}

// Add declarations for the names in vd,
// reporting duplicate declarations
void scope_check_varDecl(var_decl_t vd)
{
    scope_check_idents(vd.ident_list, vd.type_tag);
}

// Add declarations for the names in ids
// to current scope as type t
// reporting any duplicate declarations
void scope_check_idents(ident_list_t ids, AST_type t)
{
    ident_t *idp = ids.type_tag;
    while (idp != NULL) {
	scope_check_declare_ident(*idp, t);
	idp = idp->next;
    }
}

// Add declaration for id
// to current scope as type t
// reporting if it's a duplicate declaration
void scope_check_declare_ident(ident_t id,
			       AST_type t)
{
    if (symtab_declared_in_current_scope(id.name)) {
        // only variables in FLOAT
	bail_with_prog_error(*(id.file_loc),
			     "Variable \"%s\" has already been declared!",
			     id.name);
    } else {
	int ofst_cnt = symtab_scope_loc_count();
	id_attrs *attrs = id_attrs_loc_create(*(id.file_loc),
					      t, ofst_cnt);
	symtab_insert(id.name, attrs);
    }
}

// check the statement to make sure that
// all idenfifiers used have been declared
// (if not, then produce an error)
// Return the modified AST with id_use pointers
stmt_t scope_check_stmt(stmts_t stmt)
{
    switch (stmt.stmts_kind) {
    case assign_stmt:
	//stmt.data.assign_stmt
    stmt.stmts_kind
	    = scope_check_assignStmt(stmt.data.assign_stmt);
	break;
    case begin_stmt:
	stmt.data.begin_stmt
	    = scope_check_beginStmt(stmt.data.begin_stmt);
	break;
    case if_stmt:
	stmt.data.if_stmt
	    = scope_check_ifStmt(stmt.data.if_stmt);
	break;
    case read_stmt:
	stmt.data.read_stmt
	    = scope_check_readStmt(stmt.data.read_stmt);
	break;
    case write_stmt:
	stmt.data.write_stmt
	    = scope_check_writeStmt(stmt.data.write_stmt);
	break;
    default:
	bail_with_error("Call to scope_check_stmt with an AST that is not a statement!");
	break;
    }
    return stmt;
}

// check the statement for
// undeclared identifiers
// Return the modified AST with id_use pointers
assign_stmt_t scope_check_assignStmt(
                       assign_stmt_t stmt)
{
    const char *name = stmt.name;
    stmt.idu
	= scope_check_ident_declared(*(stmt.file_loc),
				     name);
    assert(stmt.idu != NULL);  // since would bail if not declared
    *stmt.expr = scope_check_expr(*(stmt.expr));
    return stmt;
}

// check the statement for
// duplicate declarations and for
// undeclared identifiers
// Return the modified AST with id_use pointers
begin_stmt_t scope_check_beginStmt(begin_stmt_t stmt)
{
    symtab_enter_scope();
    scope_check_varDecls(stmt.var_decls);
    stmt.stmts
	= scope_check_stmts(stmt.stmts);
    symtab_leave_scope();
    return stmt;
}

// check the statements to make sure that
// all idenfifiers referenced in them have been declared
// (if not, then produce an error)
// Return the modified AST with id_use pointers
stmts_t scope_check_stmts(stmts_t stmts)
{
    stmt_t *sp = stmts.stmts;
    while (sp != NULL) {
	*sp = scope_check_stmt(*sp);
	sp = sp->next;
    }
    return stmts;
}

// check the statement to make sure that
// all idenfifiers referenced in it have been declared
// (if not, then produce an error)
// Return the modified AST with id_use pointers
if_stmt_t scope_check_ifStmt(if_stmt_t stmt)
{
    stmt.expr = scope_check_expr(stmt.expr);
    *(stmt.body) = scope_check_stmt(*(stmt.body));
    return stmt;
}

// check the statement to make sure that
// all idenfifiers referenced in it have been declared
// (if not, then produce an error)
// Return the modified AST with id_use pointers
read_stmt_t scope_check_readStmt(read_stmt_t stmt)
{
    stmt.idu
	= scope_check_ident_declared(*(stmt.file_loc),
				     stmt.name);
    return stmt;
}

// check the statement to make sure that
// all idenfifiers referenced in it have been declared
// (if not, then produce an error)
// Return the modified AST with id_use pointers
write_stmt_t scope_check_writeStmt(write_stmt_t stmt)
{
    stmt.expr = scope_check_expr(stmt.expr);
    return stmt;
}

// check the expresion to make sure that
// all idenfifiers used have been declared
// (if not, then produce an error)
// Return the modified AST with id_use pointers
expr_t scope_check_expr(expr_t exp)
{
    switch (exp.expr_kind) {
    case expr_bin_op:
	exp.data.binary
	    = scope_check_binary_op_expr(exp.data.binary);
	break;
    case expr_ident:
	exp.data.ident
	    = scope_check_ident_expr(exp.data.ident);
	break;
    case expr_number:
	// no identifiers are possible in this case, so just return
	break;
    case expr_logical_not:
	*(exp.data.logical_not)
	    = scope_check_expr(*(exp.data.logical_not));
	break;
    default:
	bail_with_error("Unexpected expr_kind_e (%d) in scope_check_expr",
			exp.expr_kind);
	break;
    }
    return exp;
}

// check that all identifiers used in exp
// have been declared
// (if not, then produce an error)
// Return the modified AST with id_use pointers
binary_op_expr_t scope_check_binary_op_expr(binary_op_expr_t exp)
{
    *(exp.expr1) = scope_check_expr(*(exp.expr1));
    // (note: no identifiers can occur in the operator)
    *(exp.expr2) = scope_check_expr(*(exp.expr2));
    return exp;
}

// check the identifier (id) to make sure that
// all it has been declared (if not, then produce an error)
// Return the modified AST with id_use pointers
ident_t scope_check_ident_expr(ident_t id)
{
    id.idu
	= scope_check_ident_declared(*(id.file_loc),
				     id.name);
    return id;
}

// check that name has been declared,
// if so, then return an id_use for it
// otherwise, produce an error 
id_use *scope_check_ident_declared(
         file_location floc,
         const char *name)
{
    id_use *ret = symtab_lookup(name);
    if (ret == NULL) {
	bail_with_prog_error(floc,
				"identifier \"%s\" is not declared!",
				name);
    }
    assert(id_use_get_attrs(ret) != NULL);
    return ret;
}