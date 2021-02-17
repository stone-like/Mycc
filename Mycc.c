#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Mycc.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "argc not match\n");
        return 1;
    }

    user_input = argv[1];

    token = tokenize(); //TokList作成

    Program *prog = program();

    //localsがprogで作られているのでoffsetを計算、実際にスタックにロードするのに備える
    int offset = 0;
    for (Var *var = prog->locals; var; var = var->next)
    {
        offset += 8;
        var->offset = offset;
    }

    prog->stack_size = offset;

    codegen(prog);

    return 0;
}