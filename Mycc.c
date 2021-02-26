#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Mycc.h"

//Returns the contents of a given file.
char *read_file(char *path)
{
    //Open and read the file.
    FILE *fp = fopen(path, "r");
    if (!fp)
        error("cannot open %s: %s", path, strerror(errno));

    int filemax = 10 * 1024 * 1024;
    char *buf = malloc(filemax);
    int size = fread(buf, 1, filemax - 2, fp); //-2は\nと\0の分
    if (!feof(fp))
    {
        error("%s: file too large");
    }

    // Make sure that the string ends with "\n\0".
    if (size == 0 || buf[size - 1] != '\n')
    {
        buf[size++] = '\n';
    }
    buf[size] = '\0';
    return buf;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "argc not match\n");
        return 1;
    }

    filename = argv[1];
    user_input = read_file(filename);

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
            offset = align_to(offset, var->ty->align); //この行を追加することでローカル変数レベルのalignが可能になる
            //構造体の時と同じく、新しいローカル変数から回るので例えば
            //int x; char y;だと
            // char offset (align(0,1)=0 + sizeof char =1) = 1
            // int  offset (align(1,8)=8 + sizeof int = 8) = 16
            //これはalign_toがなければできない
            offset += size_of(var->ty, var->tok);
            vl->var->offset = offset;
        }
        fn->stack_size = align_to(offset, 8); //最後のスタックのアドレスをアラインしただけでローカル変数レベルではalignしていない,スタックにプッシュするのも8byte単位
        //ローカル変数レベルでもやってるけど、例えばcharのみだと8byte境界にならないので、stack_sizeでのalignもいるっぽい？
    }

    codegen(prog);

    return 0;
}