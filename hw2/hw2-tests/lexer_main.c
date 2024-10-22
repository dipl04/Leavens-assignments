#include "lexer.h"
#include "utilities.h"
#include "spl.tab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{

    // both of the functions here can be seen in the spl_lexer_user_code.c or in spl_lexer.l once created
    char *file_name = argv[1];
    // initializes the lexer and starts reading
    lexer_init(file_name);
    // the lexer_output, should create the entirety of the .out file
    lexer_output();
}