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

    Node *node = program();

    codegen(node);

    return 0;
}