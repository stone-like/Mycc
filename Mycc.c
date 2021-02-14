#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "argc not match\n");
        return 1;
    }


    char *p = argv[1];
    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");
    printf("   mov rax, %ld\n", strtol(p,&p,10));

    while(*p){
        //char[]から一つ一つ取り出しているので、stringである""を使うのではなく、charである''を使うようにする
        if(*p == '+'){
            p++;
            //+の次のポインタが指す先のものを使う
            printf("   add rax, %ld\n", strtol(p,&p,10));//strtolでもポインタは一つ進む
            continue;
        }

        if(*p == '-'){
            p++;
            printf("   sub rax, %ld\n", strtol(p,&p,10));
            continue;
        }

        fprintf(stderr,"unexpected char: '%c'\n",*p);
        return 1;
    }

    printf("   ret\n");
    return 0;
}