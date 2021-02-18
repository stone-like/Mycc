#include "Mycc.h"

char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"}; //関数の引数リスト、現在6引数まで

int labelseq = 0; //複数if文とカあるときにlabelがかぶらないように
char *funcname;

void gen(Node *node);

void gen_addr(Node *node)
{
    switch (node->kind)
    {
    case ND_VAR:
        printf("   lea rax, [rbp-%d]\n", node->var->offset); //leaはrbp-%dがアドレスなので[rbp-%d]として、アドレスをそのままraxに入れる、アドレスの中の値ではないことに注意
        printf("   push rax\n");
        return;
    case ND_DEREF:
        gen(node->lhs); //*の後にはVarがあるはずで、それをgenすればgen_adder -> loadで普通は値が取れるんだけど、*aのaにはアドレスが入っているのでアドレスを取得
        return;
    }

    error_tok(node->tok, "not an lvalue");
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
    case ND_NULL:
        return; //初期化式のダミー用？
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
    case ND_ADDR:
        gen_addr(node->lhs); //&aの時でアドレスが欲しいのでloadをしないでgen_addrまで
        return;
    case ND_DEREF:
        gen(node->lhs);
        //ここのgenで*aの場合aなのでND_VARに行って、aの中のアドレスが返ってくるのでそれをloadする、二回loadがいる
        //*&aの場合はgen_addr(&a)でa自体のアドレスが返る
        load();
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
    case ND_FUNCALL:
    {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next)
        {
            gen(arg); //argをスタックに積んでいく
            nargs++;
        }

        for (int i = nargs - 1; i >= 0; i--)
        {
            printf("   pop %s\n", argreg[i]); //argregに値を入れていく
        }

        //RSPを16byteAlignする、可変長引数のためにraxを0にセットしておく
        int seq = labelseq++;
        printf("  mov rax, rsp\n");
        printf("  and rax, 15\n");
        printf("  jnz .Lcall%d\n", seq);
        printf("  mov rax, 0\n");
        printf("  call %s\n", node->funcname);
        printf("  jmp .Lend%d\n", seq);
        printf(".Lcall%d:\n", seq);
        printf("  sub rsp, 8\n"); //ここまでのスタック操作は8Byte単位だったので、16byte単位でないということは8byte単位になっているので8byte引いてあげればいい
        printf("  mov rax, 0\n");
        printf("   call %s\n", node->funcname); //%sはchar *を最後まで表示？
        printf("  add rsp, 8\n");               //一応戻しておく、また関数呼び出しでずれていたらその都度8byte引くことになる
        printf(".Lend%d:\n", seq);
        printf("   push rax\n"); //返り値がraxに入っているので
        return;
    }

    case ND_RETURN:
        gen(node->lhs);
        printf("   pop rax\n");
        printf("   jmp .Lreturn.%s\n", funcname);
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
        if (node->ty->kind == TY_PTR) //add_typeでポインタ型の計算ならnode->tyをTY_PTRにしている
            printf("   imul rdi, 8\n");
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        if (node->ty->kind == TY_PTR)
            printf("   imul rdi, 8\n");
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

void codegen(Function *prog)
{
    printf(".intel_syntax noprefix\n");

    for (Function *fn = prog; fn; fn = fn->next)
    {
        printf(".global %s\n", fn->name);
        printf("%s:\n", fn->name);
        funcname = fn->name;

        //Prologue
        printf("   push rbp\n");
        printf("   mov rbp, rsp\n");
        printf("   sub rsp,%d\n", fn->stack_size);

        //Push arguements to the stack
        int i = 0; //fnごと
        for (VarList *vl = fn->params; vl; vl = vl->next)
        {
            Var *var = vl->var; //いつargregに値が入るかは、このFnをCallした時に値を入れている
            //例えば'main() { return add2(3,4); } add2(x,y) { return x+y; }'なら
            //ここではadd2をアセンブリ化していて、argreg[0],[1]はmainでadd2を呼ぶ、つまりmain()をアセンブリ化しているときのadd2のFnCall部分でargregに値を入れている
            printf("   mov [rbp-%d], %s\n", var->offset, argreg[i++]);
        }
        //Emit Code
        for (Node *node = fn->node; node; node = node->next)
        {
            gen(node);
        } //ここまでで返り値はraxに入っているはず

        //Epilogue
        printf(".Lreturn.%s:\n", funcname);
        printf("   mov rsp, rbp\n");
        printf("   pop rbp\n");
        printf("   ret\n");
    }
}