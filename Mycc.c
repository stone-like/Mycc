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

    add_type(prog);

    //localsがprogで作られているのでoffsetを計算、実際にスタックにロードするのに備える
    for (Function *fn = prog->fns; fn; fn = fn->next)
    {
        int offset = 0;
        for (VarList *vl = fn->locals; vl; vl = vl->next)
        {
            Var *var = vl->var;
            offset += size_of(var->ty);
            vl->var->offset = offset;
        }
        fn->stack_size = offset;
    }

    codegen(prog);

    return 0;
}