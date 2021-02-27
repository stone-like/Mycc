#include "Mycc.h"

char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"}; //それぞれ下記の下位8bit = 1byte？
char *argreg2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
char *argreg4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"}; //下位32bit = 4byte
char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};   //関数の引数リスト、現在6引数まで

int labelseq = 1; //複数if文とかあるときにlabelがかぶらないように
int breakseq;
int continueseq;

char *funcname;

void gen(Node *node);

void gen_addr(Node *node)
{
    // *y = &xみたいなときはassignから*yは直ぐここにくる
    switch (node->kind)
    {
    case ND_VAR:
    {
        Var *var = node->var;
        if (var->is_local)
        {
            printf("   lea rax, [rbp-%d]\n", node->var->offset); //leaはrbp-%dがアドレスなので[rbp-%d]として、アドレスをそのままraxに入れる、アドレスの中の値ではないことに注意
            printf("   push rax\n");
        }
        else
        {
            //glovbalsの場合変数のラベルのアドレスをpushすればいい
            printf("   push offset %s\n", var->name);
        }

        return;
    }

    case ND_DEREF: //勘違いしてはいけないのがint *y=&xみたいな宣言の時の*yと非宣言時で呼ぶ*yのparseの挙動は違うということ
                   //int *yは宣言なのでNodeとしては単なるVarとなるparseのdeclarationでNode *lhs = new_var(var, tok);としている
                   // 非宣言の*yだとただ*yと呼ぶときはgenからDerefに行くが、*y=&xの時はassignをつかってderefは直接gen_addrに行く、genではない

        gen(node->lhs); //*の後にはVarがあるはずで、それをgenすればgen_adder -> loadで普通は値が取れるんだけど、*aのaにはアドレスが入っているのでアドレスを取得
        return;
    case ND_MEMBER:
        gen_addr(node->lhs);
        printf("   pop rax\n");
        printf("   add rax,%d\n", node->member->offset);
        printf("   push rax\n");
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

void gen_lval(Node *node)
{
    if (node->ty->kind == TY_ARRAY)
    {
        error_tok(node->tok, "not an localValue"); //現時点ではint x[20] = 20みたいにしたくないため
    }
    gen_addr(node);
}

//charがあるので、正しくスタック操作できるように
void load(Type *ty)
{
    //loadの直前でアドレスがスタックトップにあるはずなので
    printf("   pop rax\n");

    int sz = size_of(ty, NULL);

    if (sz == 1)
    {
        printf("   movsx rax,byte ptr [rax]\n");
    }
    else if (sz == 2)
    {
        printf("   movsx rax, word ptr [rax]\n"); //16bitを64bitに符合拡張して転送
    }
    else if (sz == 4)
    {
        printf("   movsxd rax, dword ptr [rax]\n"); //32bitを64bitに符合拡張して転送
    }
    else
    {
        assert(sz == 8);
        printf("   mov rax, [rax]\n");
    }

    printf("   push rax\n");
}

void store(Type *ty)
{
    //storeの直前で、スタックトップに右辺、次に左辺値(アドレス)があるはずなので
    printf("   pop rdi\n");
    printf("   pop rax\n");

    if (ty->kind == TY_BOOL)
    {
        //boolの場合は０は０、それ以外は1なので、rdi,右辺の加工を行う
        printf("   cmp rdi, 0\n");
        printf("   setne dil\n");      //0に等しくないならbyteを入れる=1を入れる
        printf("   movzb rdi, dil\n"); //1byte分以外全部0埋め
    }

    int sz = size_of(ty, NULL);

    if (sz == 1)
    {
        printf("   mov [rax], dil\n");
    }
    else if (sz == 2)
    {
        printf("   mov [rax], di\n");
    }
    else if (sz == 4)
    {
        printf("   mov [rax], edi\n");
    }
    else
    {
        assert(sz == 8);
        printf("   mov [rax], rdi\n");
    }

    printf("   push rdi\n");
}

void truncate(Type *ty)
{
    printf("   pop rax\n");

    if (ty->kind == TY_BOOL)
    {
        printf("   cmp rax,0\n");
        printf("   setne al\n");
    }

    int sz = size_of(ty, NULL);

    if (sz == 1)
    {
        printf("  movsx rax, al\n");
    }
    else if (sz == 2)
    {
        printf("  movsx rax, ax\n");
    }
    else if (sz == 4)
    {
        printf("  movsxd rax, eax\n");
    }
    printf("  push rax\n");
}

void inc(Node *node)
{
    int sz = node->ty->base ? size_of(node->ty->base, node->tok) : 1;
    printf("   pop rax\n");
    printf("   add rax, %d\n", sz);
    printf("   push rax\n");
}

void dec(Node *node)
{
    int sz = node->ty->base ? size_of(node->ty->base, node->tok) : 1;
    printf("   pop rax\n");
    printf("   sub rax, %d\n", sz);
    printf("   push rax\n");
}

void gen(Node *node)
{

    switch (node->kind)
    {
    case ND_NULL:
        return; //初期化式のダミー用？
    case ND_NUM:
        if (node->val == (int)node->val)
        {
            printf("  push %ld\n", node->val);
        }
        else
        {
            printf("  movabs rax, %ld\n", node->val);
            printf("  push rax\n");
        }
        return;

    case ND_EXPR_STMT:
        // 式文なので、結果を捨てるという意味でadd rsp,8している(genの後は結果がスタックにpushされているので)
        gen(node->lhs);
        printf("   add rsp, 8\n");
        return;
    case ND_VAR:
    case ND_MEMBER:
        gen_addr(node);
        if (node->ty->kind != TY_ARRAY)
        {
            //TY_ARRAYの場合だけど、例えばint x[3]; *x = 3とするとして、*xで*はderef、xがND_VAR、TY_ARRAYだからここにくる
            //この時xに対してgen_addrをするんだけど普通の変数とは違って今の配列xの先頭アドレスだけほしいのでloadはいらない
            //TY_PTRだったら、&a←こんな感じなのでここにそもそも来ない
            load(node->ty);
        }
        return;
    case ND_ASSIGN:
        gen_lval(node->lhs); //現時点ではarrayに対し=できないようにする
        gen(node->rhs);
        store(node->ty);
        return;
    case ND_TERNARY:
    {
        int seq = labelseq++;
        gen(node->cond);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   je .Lelse%d\n", seq);
        gen(node->then);
        printf("   jmp .Lend%d\n", seq);
        printf(".Lelse%d:\n", seq);
        gen(node->els);
        printf(".Lend%d:\n", seq);
        return;
    }

    case ND_PRE_INC:
        gen_lval(node->lhs);
        printf("   push [rsp]\n"); //この時点ではスタックに 同じ変数のアドレスを二個積んでいる
        load(node->ty);
        inc(node);
        store(node->ty); //最初に同じ変数のアドレスを二個積んだのはloadで一個目、storeで二個目を使うため
        return;
    case ND_PRE_DEC:
        gen_lval(node->lhs);
        printf("   push [rsp]\n");
        load(node->ty);
        dec(node);
        store(node->ty);
        return;
    case ND_POST_INC:
        gen_lval(node->lhs);
        printf("   push [rsp]\n");
        load(node->ty);
        inc(node);
        store(node->ty); //ここで変数的にはincされているけど
        dec(node);       //返り値(最後のスタックにpushされる)やつはdecされているので元のx
        return;
    case ND_POST_DEC:
        gen_lval(node->lhs);
        printf("   push [rsp]\n");
        load(node->ty);
        dec(node);
        store(node->ty);
        inc(node);
        return;
    case ND_A_ADD:
    case ND_A_SUB:
    case ND_A_MUL:
    case ND_A_DIV:
    case ND_A_SHL:
    case ND_A_SHR:
    {
        gen_lval(node->lhs);
        printf("   push [rsp]\n");
        load(node->lhs->ty);
        gen(node->rhs);
        printf("   pop rdi\n");
        printf("   pop rax\n");

        switch (node->kind)
        {
        case ND_A_ADD:
            if (node->ty->base)
                printf("  imul rdi, %d\n", size_of(node->ty->base, node->tok));
            printf("  add rax, rdi\n");
            break;
        case ND_A_SUB:
            if (node->ty->base)
                printf("  imul rdi, %d\n", size_of(node->ty->base, node->tok));
            printf("  sub rax, rdi\n");
            break;
        case ND_A_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_A_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case ND_A_SHL:
            printf("   mov cl,dil\n");
            printf("   shl rax,cl\n");
            break;
        case ND_A_SHR:
            printf("   mov cl,dil\n");
            printf("   sar rax, cl\n");
            break;
        }

        printf("   push rax\n");
        store(node->ty); //ここまでで変数のアドレス,計算結果の値が順にスタックに積まれているのであとはstoreする
        return;
    }

    case ND_COMMA:
        gen(node->lhs);
        gen(node->rhs);
        return;
    case ND_ADDR:
        gen_addr(node->lhs); //&aの時でアドレスが欲しいのでloadをしないでgen_addrまで
        return;
    case ND_DEREF:
        gen(node->lhs);
        //ここのgenで*aの場合aなのでND_VARに行って、aの中のアドレスが返ってくるのでそれをloadする、二回loadがいる
        //*&aの場合はgen_addr(&a)でa自体のアドレスが返る
        if (node->ty->kind != TY_ARRAY)
        {
            load(node->ty);
        }
        return;
    case ND_NOT:
        gen(node->lhs);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   sete al\n"); //0と比較して、0だったら１を入れる(NOTのなので)
        printf("   movzb rax, al\n");
        printf("   push rax\n");
        return;
    case ND_BITNOT:
        gen(node->lhs);
        printf("   pop rax\n");
        printf("   not rax\n");
        printf("   push rax\n");
        return;
    case ND_LOGAND:
    {
        int seq = labelseq++;
        gen(node->lhs);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   je .Lfalse%d\n", seq); //andなのでどっちか一個でも0だったらfalseに飛ばす
        gen(node->rhs);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   je .Lfalse%d\n", seq);

        printf("   push 1\n"); //ここまででfalseに飛んでいないならtrueなので1を返すことにする
        printf("   jmp .Lend%d\n", seq);
        printf(".Lfalse%d:\n", seq);
        printf("   push 0\n"); //falseに飛んだ場合は0を返す

        printf(".Lend%d:\n", seq);

        return;
    }

    case ND_LOGOR:
    {
        int seq = labelseq++;
        gen(node->lhs);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   jne .Ltrue%d\n", seq); //ORの場合はどっちか一個でもtrueならtrue
        gen(node->rhs);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   jne .Ltrue%d\n", seq); //ORの場合はどっちか一個でもtrueならtrue

        printf("   push 0\n");
        printf("   jmp .Lend%d\n", seq);

        printf(".Ltrue%d:\n", seq);
        printf("   push 1\n");

        printf(".Lend%d:\n", seq);
        return;
    }
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
        int brk = breakseq;
        int cont = continueseq;

        breakseq = continueseq = seq;

        printf(".L.continue.%d:\n", seq);
        gen(node->cond);
        printf("   pop rax\n");
        printf("   cmp rax, 0\n");
        printf("   je .L.break.%d\n", seq);
        gen(node->then);
        printf("   jmp .L.continue.%d\n", seq);
        printf(".L.break.%d:\n", seq);

        breakseq = brk;
        continueseq = cont;
        return;
    }

    case ND_FOR:
    {
        int seq = labelseq++;
        int brk = breakseq;
        int cont = continueseq;
        breakseq = continueseq = seq; //for内ではbreakseqはlabeseqと同じ値になっていて、
        //for(){break;}とあるとき、for内のgen(node->then);でbreakが呼ばれて、その時breakseqはlabelseqと同じになっているので、しっかり望んだbreakの場所に行ける想定

        if (node->init)
            gen(node->init); //i=0でstoreの最後にpushした値は特にいらないので(必要なのはiのアドレスに0をstoreすることだけ)ここは式文
        printf(".Lbegin%d:\n", seq);
        if (node->cond)
        {
            gen(node->cond); //ここで式文にしているとスタックトップが捨てられるのでここは式
            printf("   pop rax\n");
            printf("   cmp rax, 0\n");
            printf("   je .L.break.%d\n", seq);
        }
        gen(node->then);
        printf(".L.continue.%d:\n", seq);
        if (node->inc)
            gen(node->inc);
        printf("   jmp .Lbegin%d\n", seq);

        printf(".L.break.%d:\n", seq);

        breakseq = brk;
        continueseq = cont;
        return;
    }
    case ND_SWITCH:
    {
        int seq = labelseq++;
        int brk = breakseq;

        breakseq = seq;
        node->case_label = seq;

        gen(node->cond);
        printf("   pop rax\n");

        for (Node *n = node->case_next; n; n = n->case_next)
        {
            n->case_label = labelseq++; //case_labelはcaseごとに１ずつ増えていくけど、case_end_labelはseqのまま変わらず
            n->case_end_label = seq;
            printf("   cmp rax, %ld\n", n->val);
            printf("   je .L.case.%d\n", n->case_label);
        }

        if (node->default_case)
        {
            int i = labelseq++;
            node->default_case->case_end_label = seq;
            node->default_case->case_label = i;
            printf("   jmp .L.case.%d\n", i);
        }

        printf("   jmp .L.break.%d\n", seq);
        gen(node->then);
        printf(".L.break.%d:\n", seq);

        breakseq = brk;
        return;
    }

    case ND_CASE:
    {
        printf(".L.case.%d:\n", node->case_label);
        gen(node->lhs);
        printf("   jmp .L.break.%d\n", node->case_end_label); //case_end_labelは全case共通
        return;
    }
    case ND_BLOCK:
    case ND_STMT_EXPR:
        for (Node *n = node->body; n; n = n->next)
        {
            gen(n);
        }
        return;
    case ND_BREAK:
    {
        if (breakseq == 0)
            error_tok(node->tok, "stray break");

        printf("   jmp .L.break.%d\n", breakseq);
        return;
    }
    case ND_CONTINUE:
    {
        if (continueseq == 0)
            error_tok(node->tok, "stray break");
        printf("   jmp .L.continue.%d\n", continueseq);
        return;
    }
    case ND_GOTO: // funcname = fn->name;とfnsを回すときにやっている,なのでfuncnameは現在の関数名,なので関数が違えばラベル名が同じでも問題ない
        printf("   jmp .L.label.%s.%s\n", funcname, node->label_name);
        return;
    case ND_LABEL:
        printf(".L.label.%s.%s:\n", funcname, node->label_name);
        gen(node->lhs);
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
            printf("   pop %s\n", argreg8[i]); //argregに値を入れていく
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

        if (node->ty->kind != TY_VOID)
        {
            truncate(node->ty);
        }

        return;
    }

    case ND_RETURN:
        gen(node->lhs); //関数のretとreturnはあまり関係がない、retではraxに積むことはしないので、returnでraxに返り値を積む
        printf("   pop rax\n");
        printf("   jmp .Lreturn.%s\n", funcname);
        return;
    case ND_CAST:
        gen(node->lhs);
        truncate(node->ty);
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
        if (node->ty->base)
            printf("   imul rdi, %d\n", size_of(node->ty->base, node->tok));
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        if (node->ty->base)
            printf("   imul rdi, %d\n", size_of(node->ty->base, node->tok));
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv rdi\n");
        break;
    case ND_BITAND:
        printf("  and rax, rdi\n");
        break;
    case ND_BITOR:
        printf("  or rax, rdi\n");
        break;
    case ND_BITXOR:
        printf("  xor rax, rdi\n");
        break;
    case ND_SHL:
        printf("   mov cl, dil\n");
        printf("   shl rax, cl\n");
        break;
    case ND_SHR:
        printf("   mov cl, dil\n");
        printf("   sar rax, cl\n");
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

void emit_data(Program *prog)
{
    printf(".data\n");

    for (VarList *vl = prog->globals; vl; vl = vl->next)
    {
        Var *var = vl->var;
        printf("%s:\n", var->name);

        if (!var->contents)
        {
            //""みたいに空Stringだったら
            printf("   .zero %d\n", size_of(var->ty, var->tok));
            continue;
        }

        for (int i = 0; i < var->count_len; i++)
        {
            printf("   .byte %d\n", var->contents[i]);
        }
    }
}

void load_arg(Var *var, int idx)
{
    int sz = size_of(var->ty, var->tok);
    if (sz == 1)
    {
        printf("   mov [rbp-%d], %s\n", var->offset, argreg1[idx]);
    }
    else if (sz == 2)
    {
        printf("   mov [rbp-%d], %s\n", var->offset, argreg2[idx]);
    }
    else if (sz == 4)
    {
        printf("   mov [rbp-%d], %s\n", var->offset, argreg4[idx]);
    }
    else
    {
        assert(sz == 8);
        printf("   mov [rbp-%d], %s\n", var->offset, argreg8[idx]);
    }
}

void emit_text(Program *prog)
{
    printf(".text\n");

    for (Function *fn = prog->fns; fn; fn = fn->next)
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
            //いつargregに値が入るかは、このFnをCallした時に値を入れている
            //例えば'main() { return add2(3,4); } add2(x,y) { return x+y; }'なら
            //ここではadd2をアセンブリ化していて、argreg[0],[1]はmainでadd2を呼ぶ、つまりmain()をアセンブリ化しているときのadd2のFnCall部分でargregに値を入れている
            load_arg(vl->var, i++);
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

void codegen(Program *prog)
{
    printf(".intel_syntax noprefix\n");
    emit_data(prog);
    emit_text(prog);
}