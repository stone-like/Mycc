#include "Mycc.h"

int align_to(int n, int align)
{
    return (n + align - 1) & ~(align - 1);
}

Type *new_type(TypeKind kind, int align)
{
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    ty->align = align;
    return ty;
}

Type *void_type()
{
    return new_type(TY_VOID, 1);
}

Type *bool_type()
{
    return new_type(TY_BOOL, 1);
}

Type *char_type()
{
    return new_type(TY_CHAR, 1);
}

Type *short_type()
{
    return new_type(TY_SHORT, 2);
}

Type *int_type()
{
    return new_type(TY_INT, 4);
}

Type *long_type()
{
    return new_type(TY_LONG, 8);
}

Type *enum_type()
{
    return new_type(TY_ENUM, 4);
}

Type *struct_type()
{
    Type *ty = new_type(TY_STRUCT, 1);
    ty->is_incomplete = true;
    return ty;
}

Type *func_type(Type *return_ty)
{
    Type *ty = new_type(TY_FUNC, 1);
    ty->return_ty = return_ty;
    return ty;
}

Type *pointer_to(Type *base)
{
    Type *ty = new_type(TY_PTR, 8);
    ty->base = base;
    return ty;
}

Type *array_of(Type *base, int size)
{
    Type *ty = new_type(TY_ARRAY, base->align);
    ty->base = base;
    ty->array_size = size;
    return ty;
}

int size_of(Type *ty, Token *tok)
{
    assert(ty->kind != TY_VOID);

    if (ty->is_incomplete)
        //宣言自体はincompleteでもいいけど、parseを終え、add_typeするまでには完全系にしておかないとダメということかな
        error_tok(tok, "incomplete type");

    switch (ty->kind)
    {
    case TY_BOOL:
    case TY_CHAR:
        return 1;
    case TY_SHORT:
        return 2;
    case TY_INT:
    case TY_ENUM:
        return 4;
    case TY_LONG:
    case TY_PTR:
        return 8;
    case TY_ARRAY:
        return size_of(ty->base, tok) * ty->array_size; //再帰でsizeを獲得
    default:
        assert(ty->kind == TY_STRUCT);
        Member *mem = ty->members;
        while (mem->next)
            mem = mem->next;
        int end = mem->offset + size_of(mem->ty, tok);
        return align_to(end, ty->align); //構造体の中で最大のサイズのalignとなる
        //構造体レベルのalign
    }
}

Member *find_member(Type *ty, char *name)
{
    assert(ty->kind == TY_STRUCT);
    for (Member *mem = ty->members; mem; mem = mem->next)
    {
        if (!strcmp(mem->name, name))
            return mem; //strcmpは一致すれば0を返すので!strcmpなら一致したことになる
    }
    return NULL;
}

void visit(Node *node)
{
    if (!node)
        return;

    visit(node->lhs);
    visit(node->rhs);
    visit(node->cond);
    visit(node->then);
    visit(node->els);
    visit(node->init);
    visit(node->inc);

    for (Node *n = node->body; n; n = n->next)
        visit(n);
    for (Node *n = node->args; n; n = n->next)
        visit(n);

    switch (node->kind)
    {
    case ND_MUL:
    case ND_DIV:
    case ND_BITAND:
    case ND_BITOR:
    case ND_BITXOR:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NOT:
    case ND_BITNOT:
    case ND_LOGOR:
    case ND_LOGAND:
        node->ty = int_type();
        return;
    case ND_NUM:
        if (node->val == (int)node->val)
            node->ty = int_type();
        else
            node->ty = long_type();
        return;
    case ND_VAR:
        node->ty = node->var->ty;
        return;
    case ND_ADD:
        if (node->rhs->ty->base)
        {
            Node *tmp = node->lhs;
            node->lhs = node->rhs;
            node->rhs = tmp;
        }
        if (node->rhs->ty->base)
            error_tok(node->tok, "invalid pointer arithmetic operands");
        node->ty = node->lhs->ty;
        return;
    case ND_SUB:
        if (node->rhs->ty->base)
            error_tok(node->tok, "invalid pointer arithmetic operands");
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
    case ND_SHL:
    case ND_SHR:
    case ND_PRE_INC: //INC,DEC系はlhsに変数が来るはずなのでそのtypeに
    case ND_PRE_DEC:
    case ND_POST_INC:
    case ND_POST_DEC:
    case ND_A_ADD:
    case ND_A_SUB:
    case ND_A_MUL:
    case ND_A_DIV:
    case ND_A_SHL:
    case ND_A_SHR:
        node->ty = node->lhs->ty;
        return;
    case ND_TERNARY:
        node->ty = node->then->ty;
        return;
    case ND_COMMA:
        node->ty = node->rhs->ty;
        return;
    case ND_MEMBER:
    {
        // . からはND_MEMBERが作られる
        // x.memみたいにきて、. のlhsがvar TY_STRUCTのx <-findVarでTY_STRUCTの変数xを持ってくる
        // .のmember_nameがmem
        if (node->lhs->ty->kind != TY_STRUCT)
        {
            error_tok(node->tok, "not a struct");
        }

        node->member = find_member(node->lhs->ty, node->member_name); //ここでx.memみたいなやつで指定の構造体のメンバーが指定される,offsetはparseで計算してある
        //あくまで構造体もただの変数でしかなく、普通の変数と扱いは同じ（Var(TY_STRUCT)）という形で、オフセットの取得方法がmemberを使っているのでちょっと特殊なだけ
        if (!node->member)
        {
            error_tok(node->tok, "specified member does not exist");
        }

        //.のタイプは指定したメンバーのタイプとなる

        node->ty = node->member->ty;
        return;
    }

    case ND_ADDR: //(&x+1)の場合&xはND_ADDRなのでTY_PTRになるので&x add 1 はadd TY_PTRになる
        if (node->lhs->ty->kind == TY_ARRAY)
        {
            node->ty = pointer_to(node->lhs->ty->base); //例えば&x[2]だったらintへのポインターに変換する
        }
        else
        {
            node->ty = pointer_to(node->lhs->ty);
        }
        return;
    case ND_DEREF:
        if (!node->lhs->ty->base) // * &aの時、*のタイプはaのタイプになる、つまり*&aを計算した最終的な値の型ということでいい？
                                  //* の後には必ず何らかの形でアドレスが来なくてはいけない
            error_tok(node->tok, "invalid pointer dereference");
        node->ty = node->lhs->ty->base; //ここが重要

        if (node->ty->kind == TY_VOID)
        {
            error_tok(node->tok, "dereferenceinf a void pointer");
        }

        return;
    case ND_SIZEOF: //ノードタイプをSIZEOFからNUMへ変化させる
        node->kind = ND_NUM;
        node->ty = int_type();
        node->val = size_of(node->lhs->ty, node->tok);
        node->lhs = NULL;
        return;
    case ND_STMT_EXPR:
    {
        Node *last = node->body;
        while (last->next)
            last = last->next;
        node->ty = last->ty; // { x=2; y=2; y;}だったらラストのyのタイプ
        return;
    }
    }
}

//作成済みのNodeに対して型を付けていく、Nodeの最終的な返り値を型にするっぽい
void add_type(Program *prog)
{
    for (Function *fn = prog->fns; fn; fn = fn->next)
        for (Node *node = fn->node; node; node = node->next)
            visit(node);
}