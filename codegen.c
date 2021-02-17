#include "Mycc.h"

int labelseq = 0; //複数if文とカあるときにlabelがかぶらないように

void gen_addr(Node *node)
{
    if (node->kind == ND_VAR)
    {

        printf("   lea rax, [rbp-%d]\n", node->var->offset); //leaはrbp-%dがアドレスなので[rbp-%d]として、アドレスをそのままraxに入れる、アドレスの中の値ではないことに注意
        printf("   push rax\n");
        return;
    }

    error("not an lvalue");
}

void load()
{
    //loadの直前でアドレスがスタックトップにあるはずなので
    printf("   pop rax\n");
    printf("   mov rax, [rax]\n");
    printf("   push rax\n");
}

void store()
{
    //storeの直前で、スタックトップに右辺、次に左辺値(アドレス)があるはずなので
    printf("   pop rdi\n");
    printf("   pop rax\n");
    printf("   mov [rax], rdi\n");
    printf("   push rdi\n");
}

void gen(Node *node)
{

    switch (node->kind)
    {
    case ND_NUM:
        printf("   push %d\n", node->val);
        return;
        // 式文なので、結果を捨てるという意味でadd rsp,8している(genの後は結果がスタックにpushされているので)
    case ND_EXPR_STMT:
        gen(node->lhs);
        printf("   add rsp, 8\n");
        return;
    case ND_VAR:
        gen_addr(node);
        load();
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        gen(node->rhs);
        store();
        return;
    case ND_IF:
    {
        int seq = labelseq++;
        if (node->els)
        {
            gen(node->cond);
            printf("   pop rax\n");
            printf("   cmp rax, 0\n");
            printf("   je .Lelse%d\n", seq);
            gen(node->then);
            printf("   jmp .Lend%d\n", seq);
            printf(".Lelse%d:\n", seq);
            gen(node->els);
            printf(".Lend%d:\n", seq);
        }
        else
        {
            gen(node->cond);
            printf("   pop rax\n");
            printf("   cmp rax, 0\n");
            printf("   je .Lend%d\n", seq);
            gen(node->then);
            printf(".Lend%d:\n", seq);
        }
        return;
    }

    case ND_WHILE:
    {
        int seq = labelseq++;
        printf(".Lbegin%d:\n", seq);
        gen(node->cond);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   je .Lend%d\n", seq);
        gen(node->then);
        printf("   jmp .Lbegin%d\n", seq);
        printf(".Lend%d:\n", seq);
        return;
    }

    case ND_FOR:
    {
        int seq = labelseq++;
        if (node->init)
            gen(node->init); //i=0でstoreの最後にpushした値は特にいらないので(必要なのはiのアドレスに0をstoreすることだけ)ここは式文
        printf(".Lbegin%d:\n", seq);
        if (node->cond)
        {
            gen(node->cond); //ここで式文にしているとスタックトップが捨てられるのでここは式
            printf("   pop rax\n");
            printf("   cmp rax, 0\n");
            printf("   je .Lend%d\n", seq);
        }
        gen(node->then);

        if (node->inc)
            gen(node->inc);
        printf("   jmp .Lbegin%d\n", seq);

        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
        {
            gen(n);
        }
        return;

    case ND_RETURN:
        gen(node->lhs);
        printf("   pop rax\n");
        printf("   jmp .Lreturn\n");
        return;
    }

    //なぜreturn 3; 2; 1;みたいのがうまくいくかというと
    // main:
    //    push 3
    //    pop rax
    //    ret
    //    pop rax
    //    push 2
    //    pop rax
    //    push 1
    //    pop rax
    //   ret
    //みたいになって、アセンブラ的に、
    //    push 3
    //    pop rax
    //    ret
    //までで終わりだから、なんかコード的にSQLInjectionっぽいけど

    //ここから左右の値を使う

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind)
    {
    case ND_ADD:
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv rdi\n");
        break;
    case ND_EQ:
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_NE:
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LT:
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LE:
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        break;
    }

    printf("  push rax\n");
}

void codegen(Program *prog)
{
    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");

    //Prologue
    printf("   push rbp\n");
    printf("   mov rbp, rsp\n");
    printf("   sub rsp, %d\n", prog->stack_size);

    for (Node *n = prog->node; n; n = n->next)
    {
        gen(n);
        // printf("   pop rax\n");
    }

    //Epilogue
    printf(".Lreturn:\n");
    printf("   mov rsp, rbp\n");
    printf("   pop rbp\n");
    printf("  ret\n");
}