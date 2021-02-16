#include "Mycc.h"

void gen(Node *node)
{

    switch (node->kind)
    {
    case ND_NUM:
        printf("   push %d\n", node->val);
        return;
    case ND_RETURN:
        gen(node->lhs);
        printf("   pop rax\n");
        printf("   ret\n"); //return文ごとにretが一つ増える
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

void codegen(Node *node)
{
    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");

    for (Node *n = node; n; n = n->next)
    {
        gen(n);
        printf("   pop rax\n");
    }

    printf("  ret\n");
}