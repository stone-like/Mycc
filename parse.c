#include "Mycc.h"

// Scope for local var,global var or typedefs

typedef struct VarScope VarScope;

struct VarScope
{
    VarScope *next;
    char *name;
    int depth;
    Var *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
};

// Scope For Struct Tag
typedef struct TagScope TagScope;
struct TagScope
{
    TagScope *next;
    char *name;
    int depth;
    Type *ty;
};

typedef struct
{
    VarScope *var_scope;
    TagScope *tag_scope;
} Scope;

VarList *locals;
VarList *globals;
VarScope *var_scope; //そこまでに含まれている変数、localsは関数全体だが、scopeはint main(){int x; int y;}のint xの部分までだったらxまでとなる
//scopeにはtypedefMemberがある

TagScope *tag_scope;
int scope_depth;

Node *current_switch;

Scope *enter_scope()
{
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    ++scope_depth;
    return sc;
}

void leave_scope(Scope *sc)
{
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
    --scope_depth;
}
// Find variable or a typedef by name.
VarScope *find_var(Token *tok)
{

    //scopeのみでfindすると例えば
    //'int main() { int x=2; { int x=3; } return x; }'
    //みたいなやつでreturn　xのときにfindしてもx=2しかない
    //ただ、localの方にはx=2とx=3のやつがあるし、Block内でfindすると後に追加されたx=3のやつが来る
    //単にローカル、グローバル関係なくスコープの中から探せばいい

    for (VarScope *sc = var_scope; sc; sc = sc->next)
    {
        //memcmpは一致すれば０が返るので、!0で一致すればtrueを返す
        if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len))
            return sc;
    }

    return NULL;
}

TagScope *find_tag(Token *tok)
{
    for (TagScope *sc = tag_scope; sc; sc = sc->next)
    {
        if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len))
        {
            return sc;
        }
    }

    return NULL;
}

Node *new_node(NodeKind kind, Token *tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

//unaryは左辺値＝アドレスのみ
Node *new_unary(NodeKind kind, Node *expr, Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

Node *new_num(long val, Token *tok)
{
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

//Node用
Node *new_var(Var *var, Token *tok)
{
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

//typedef int xならxをintとして扱う
VarScope *push_scope(char *name)
{
    VarScope *sc = calloc(1, sizeof(VarScope));
    sc->name = name;
    sc->next = var_scope;
    sc->depth = scope_depth;
    var_scope = sc;
    return sc;
}

//NodeごとにTypeを入れるときに、Varの場合はnode->ty = node->var->tyとなる
//localsに追加
Var *push_var(char *name, Type *ty, bool is_local, Token *tok)
{
    //新しい奴から先頭に来る
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;
    var->tok = tok;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;

    if (is_local)
    {
        vl->next = locals;
        locals = vl;
    }
    else if (ty->kind != TY_FUNC)
    { //TY_FUNCの変数はscopeに追加するだけ
        vl->next = globals;
        globals = vl;
    }

    //ローカル、グローバル関係なく現時点までの変数をすべて追加
    // push_scope(name)->var = var; //push_varでやるのではなく、push_varした後、別にまた呼び出して分離してscopeに入れることにした

    return var;
}

Type *find_typedef(Token *tok)
{
    if (tok->kind == TK_IDENT)
    {
        VarScope *sc = find_var(token);
        if (sc)
        {
            return sc->type_def;
        }
    }

    return NULL;
}

char *new_label()
{
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

Function *function();
// Type *basetype();
Type *type_specifier();
Type *declarator(Type *ty, char **name);
Type *abstract_declarator(Type *ty);
Type *type_suffix(Type *ty);
Type *type_name();

Type *struct_declaration();
Type *enum_specifier();
Member *struct_member();
void global_var();
Node *declaration();
bool is_typename();
Node *stmt();
Node *expr();
long const_expr();
Node *assign();
Node *conditional();
Node *logor();
Node *logand();
Node *bitand();
Node * bitor ();
Node *bitxor();
Node *equality();
Node *relational();
Node *shift();
Node *add();
Node *mul();
Node *cast();
Node *unary();
Node *postfix();
Node *primary();

//functionとgloval変数は似ているため見分ける関数が必要
//int x[10];とint x(...)
bool is_function()
{
    Token *tok = token;

    Type *ty = type_specifier();
    char *name = NULL;
    declarator(ty, &name);
    bool isfunc = name && consume("(");

    token = tok; //読んだ分戻してあげる
    return isfunc;
}

// program = (gloval-var | function)*

Program *program()
{

    Function head;
    head.next = NULL;
    Function *cur = &head;
    globals = NULL; //globalsはfunctionレベルで初期化しないでよい

    while (!at_eof())
    {
        if (is_function())
        {
            Function *fn = function();
            if (!fn)
                continue;
            cur->next = fn;
            cur = cur->next;
            continue;
        }

        global_var();
    }

    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}

// typespecifier = builtin-type | struct-declaration | typedef-name | enum-specifier

// builtin-type   = "void"
//                | "_Bool"
//                | "char"
//                | "short" | "short" "int" | "int" "short"
//                | "int"
//                | "long" | "long" "int" | "int" "long"
//
//basetypeではtypedefで宣言された typedef int xのxも処理する
//このときのxはただの変数ではあるが、VarScopeのtypedefにType構造体でtyがINTのやつ(stmt()のtypedefの処理の時にbasetype()から作ったやつ)が入っている

//staticもここで処理

Type *type_specifier()
{
    if (!is_typename(token))
        error_tok(token, "typename expected");

    Type *ty = NULL;
    enum
    {
        VOID = 1 << 1,
        BOOL = 1 << 3,
        CHAR = 1 << 5,
        SHORT = 1 << 7,
        INT = 1 << 9,
        LONG = 1 << 11,
    };

    int base_type = 0;
    Type *user_type = NULL;

    bool is_typedef = false;
    bool is_static = false;

    for (;;)
    {
        //Read one Token at a time.
        Token *tok = token;
        if (consume("typedef"))
        { //typedefで作られた変数もここで扱うが、typedefの作成自体もここでする
            //ただここでは変数を作るだけ、変数をScopeに入れていない
            is_typedef = true;
        }
        else if (consume("static"))
        {
            is_static = true;
        }
        else if (consume("void"))
        {
            base_type += VOID;
        }
        else if (consume("_Bool"))
        {
            base_type += BOOL;
        }
        else if (consume("char"))
        {
            base_type += CHAR;
        }
        else if (consume("short"))
        {
            base_type += SHORT;
        }
        else if (consume("int"))
        {
            base_type += INT;
        }
        else if (consume("long"))
        {
            base_type += LONG;
        } //user_typeはbase_type以外のやつ,breakしているのはbase_type+user_type,user_type+user_typeの組み合わせをなくすため
        else if (peek("struct"))
        {
            if (base_type || user_type)
                break;
            user_type = struct_declaration();
        }
        else if (peek("enum"))
        {
            if (base_type || user_type)
                break;
            user_type = enum_specifier();
        }
        else
        {
            if (base_type || user_type)
                break;
            Type *ty = find_typedef(token);
            if (!ty)
                break;
            token = token->next;
            user_type = ty;
        }

        switch (base_type)
        {
        //enumを使って型の足し合わせをbitで表現している
        case VOID:
            ty = void_type();
            break;
        case BOOL:
            ty = bool_type();
            break;
        case CHAR:
            ty = char_type();
            break;
        case SHORT:
        case SHORT + INT:
            ty = short_type();
            break;
        case INT:
            ty = int_type();
            break;
        case LONG:
        case LONG + INT:
            ty = long_type();
            break;
        case 0:
            // If there's no type specifier, it becomes int.
            // For example, `typedef x` defines x as an alias for int.
            ty = user_type ? user_type : int_type();
            break;
        default:
            error_tok(tok, "invalid type");
        }
    }

    ty->is_typedef = is_typedef;
    ty->is_static = is_static;
    return ty;
}

// declarator = "*"* ( "(" declaration ")"  | ident ) type-suffix
Type *declarator(Type *ty, char **name)
{

    while (consume("*"))
    {
        ty = pointer_to(ty); //例えばint **だったらtyはTY_PTR(
                             //                       base=TY_PTR(
                             //                              base=TY_INT
                             //                                   )
                             //                              )となる
    }

    if (consume("("))
    {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = declarator(placeholder, name);

        expect(")");
        *placeholder = *type_suffix(ty);

        return new_ty;
    }

    *name = expect_ident();
    return type_suffix(ty);
}

// abstract-declarator = "*"* ( "(" abstract-declarator ")" )? type-suffix
Type *abstract_declarator(Type *ty)
{
    while (consume("*"))
    {
        ty = pointer_to(ty);
    }

    if (consume("("))
    {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = abstract_declarator(placeholder);
        expect(")");
        *placeholder = *type_suffix(ty);
        return new_ty;
    }

    return type_suffix(ty);
}

// type-suffix = ( "[" const-expr? "]" type-suffix)?
//const-exprを使うことによって、[1]だけでなく、[1+1+2]みたいなのもできる
Type *type_suffix(Type *ty)
{
    if (!consume("["))
        return ty; //そのまま返すだけ

    //Arrayの場合
    int sz = 0;
    bool is_incomplete = true;
    if (!consume("]"))
    {

        sz = const_expr(); //例えば1+1ならconst_exprで計算されて２が入る
        is_incomplete = false;
        expect("]");
    }

    ty = type_suffix(ty);

    ty = array_of(ty, sz); //int[2]だとarray_od(int,2)でint[2][3]だとarray_of(array_of(int,3),2)となる,
    //int[]みたいな不完全系ならty->array_size = 0;となり、is_incompleteもtrueのまま
    ty->is_incomplete = is_incomplete;
    return ty;
}

// type-name = type-specifier abstract-declarator type-suffix
Type *type_name()
{
    Type *ty = type_specifier();
    ty = abstract_declarator(ty);
    return type_suffix(ty);
}

void push_tag_scope(Token *tok, Type *ty)
{
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = strndup(tok->str, tok->len);
    sc->depth = scope_depth;
    sc->ty = ty;
    tag_scope = sc;
}

// struct-declaration = "struct" ident? ( "{" struct-member "}" )?
// struct {}の{}までintみたいなtypeのうち　なのでここまでbasetypeでやって、その結果がVarに収容される
Type *struct_declaration()
{ //struct declarationはTY_STRUCTを返す

    //Read struct members.
    expect("struct");

    // Read a struct tag.
    Token *tag = consume_ident();
    if (tag && !peek("{"))
    {
        //もしstruct identなら登録済みのを利用する
        TagScope *sc = find_tag(tag);

        if (!sc)
        {
            //struct xとしたけど、tagがないとき
            //struct identでtagが未登録なら、不完全構造体としてTagを新たに登録
            Type *ty = struct_type();
            push_tag_scope(tag, ty);
            return ty;
        }

        if (sc->ty->kind != TY_STRUCT)
            error_tok(tag, "not a struct tag");

        return sc->ty;
    }

    if (!consume("{"))
    { //struct *fooみたいなtag付けもしないやつのとき、つまりstructのみがtypeのとき
        //普通はstruct x とか struct {}、struct x {}みたいな感じなんだけどね
        return struct_type();
    }

    //例えば 1- struct T *foo; 2- struct T { int x;};としたとき
    //1ではstruct TのTがtagとして登録されるだけで不完全
    //2で{int x;}まで宣言することで、↓以降に来ることができて、そこで初めて、findTagでTを持ってきて、Tが完全系となる

    TagScope *sc = find_tag(tag);
    Type *ty;

    if (sc && sc->depth == scope_depth)
    {

        if (sc->ty->kind != TY_STRUCT)
        {
            error_tok(tag, "not a struct tag");
        }
        ty = sc->ty;
    }
    else
    {
        ty = struct_type();
        if (tag)
        {
            push_tag_scope(tag, ty);
        }
    }

    // Read struct members;
    Member head;
    head.next = NULL;
    Member *cur = &head;

    while (!consume("}"))
    {
        cur->next = struct_member();
        cur = cur->next;
    }

    ty->members = head.next;

    //Assign offsets within the struct to members.
    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next)
    {
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += size_of(mem->ty, mem->tok);

        if (ty->align < mem->ty->align)
        {
            ty->align = mem->ty->align; //memというか大体Myccで使っているリンクドリストは新しく追加された奴から回していくので、例えばstruct { char x; int y;}だったらyからここにくることになる
            //で、char->intでもint->charでも同じように構造体全体のsizeとしては16byteになってほしい
            //このif説の中でalignが構造体の中で最大のtypeのalignとなる
            //type.cのsize_ofの return align_to(end,ty->align);で効いてくる
        }
    }

    ty->is_incomplete = false;

    return ty;
}

//enum-specifier = "enum" ident
//               | "enum" ident? "{" enum-list? "}"
//
//enum-list = enum-elem  ( "," enum-elem)* ","?
// enum-elem = ident ( "=" const-expr )
// const-exprを使うことによって、enum { ten=1+2+3+4 }みたいなのができる
Type *enum_specifier()
{
    //他のやつと同様、生成と取得を同じところで行う
    expect("enum");
    Type *ty = enum_type();

    //取得
    Token *tag = consume_ident();
    if (tag && !peek("{"))
    {                                 //"enum" identの時
        TagScope *sc = find_tag(tag); //ここに来るときはすでに登録済でなくてはいけない
        if (!sc)
            error_tok(tag, "unknown enum type");
        if (sc->ty->kind != TY_ENUM)
            error_tok(tag, "not an enum tag");
        return sc->ty;
    }

    //生成
    expect("{");

    int cnt = 0;
    for (;;)
    {
        char *name = expect_ident();
        if (consume("="))
            cnt = const_expr();

        VarScope *sc = push_scope(name); //localとかglobalには入れずにscopeにだけ入れる
        sc->enum_ty = ty;
        sc->enum_val = cnt++;

        if (consume(","))
        {
            if (consume("}"))
                break;
            continue;
        }

        expect("}");
        break;
    }

    //Tagがある(identがあるなら)ならtagを追加
    if (tag)
    {
        push_tag_scope(tag, ty);
    }

    return ty;
}

// struct-member = type-specifier declarator type-suffix ";"

Member *struct_member()
{
    Type *ty = type_specifier();
    Token *tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name = name;
    mem->ty = ty;
    mem->tok = tok;
    return mem;
}

VarList *read_func_param()
{
    //fn(int x,int y)みたいになっているので"("の次はbasetypeが来ている

    Type *ty = type_specifier();
    //typeの次はIdentifier
    char *name = NULL;
    Token *tok = token;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    //もしパラメーターでarray[int]があったらpointer to intに変換する,
    // *int[] だったら　**intとなる

    if (ty->kind == TY_ARRAY)
    {
        ty = pointer_to(ty->base);
    }

    Var *var = push_var(name, ty, true, tok); //引数もpush_varしているのでまとめてその関数のlocalsに入ることになる
    push_scope(name)->var = var;              //scopeにも入れる
    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    return vl;
}

VarList *read_func_params()
{
    if (consume(")"))
        return NULL;

    VarList *head = read_func_param();
    VarList *cur = head;

    //二つ目以降
    while (!consume(")"))
    {
        expect(",");
        cur->next = read_func_param();
        cur = cur->next;
    }

    return head;
}

// function = type-specifier declarator "(" params?")" ( "{" stmt* "}" | ";" )
// params = ident ("," ident)*
// param  = type-specifier declarator type-suffix
Function *function()
{
    locals = NULL;

    Type *ty = type_specifier();
    char *name = NULL;
    Token *tok = token;
    ty = declarator(ty, &name);

    //Add a fuction type to the scope
    Var *var = push_var(name, func_type(ty), false, tok);
    push_scope(name)->var = var;

    //Construct a function object
    Function *fn = calloc(1, sizeof(Function));

    fn->name = name;

    expect("(");
    fn->params = read_func_params();

    if (consume(";"))
        return NULL; //もし関数宣言だったらprogram->fnsには追加しない、関数を変数として追加するだけ

    //Read function Body
    Node head;
    head.next = NULL;
    Node *cur = &head;

    expect("{");

    while (!consume("}"))
    {
        cur->next = stmt();
        cur = cur->next;
    }

    fn->node = head.next;
    fn->locals = locals; //functionを取得するたびにLocalsはNULLとなるのでFunctionごとのLocal
    return fn;
}

bool peek_end()
{
    Token *tok = token;
    bool ret = consume("}") || (consume(",") && consume("}"));
    token = tok; //チェックするだけなので元に戻す
    return ret;
}

void expect_end()
{
    Token *tok = token;
    if (consume(",") && consume("}"))
        return;
    token = tok;
    expect("}"); //初期化は}か,}で終わるのでここでチェック
}

//global-var = type-specifier declarator type-suffix ";"
void global_var()
{
    Type *ty = type_specifier();
    char *name = NULL;
    Token *tok = token;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    expect(";");
    Var *var = push_var(name, ty, false, tok);
    push_scope(name)->var = var;
}

typedef struct Designator Designator;

struct Designator
{
    Designator *next;
    int index;
    Member *mem; //struct
};

Node *create_array_access(Var *var, Designator *desg)
{
    Token *tok = var->tok;
    if (!desg)
    {
        return new_var(var, tok);
    }

    //例えばx[1]で、desg={NULL,1}とすると,
    //下のcreate_array_accessで。desg->next=NULLとなり、new_varが返る
    Node *node = create_array_access(var, desg->next);

    //構造体の場合
    if (desg->mem)
    {
        node = new_unary(ND_MEMBER, node, desg->mem->tok);
        node->member_name = desg->mem->name;
        return node;
    }

    node = new_binary(ND_ADD, node, new_num(desg->index, tok), tok); //index分アドレスに足す,codegenではADDでvarとnumを足すことになる、varはgenでgenAddrに送られるので結局変数のアドレス+indexとなる

    //ここのND_ADDは*(*(x+0)+1)なんだけど、
    //x+0ではimul*4して、 ~ +1ではimul+12している

    return new_unary(ND_DEREF, node, tok); //DEREFでアドレスの中身へアクセス
}

Node *assign_array_to_value(Var *var, Designator *desg, Node *rhs)
{
    Node *lhs = create_array_access(var, desg);
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, rhs->tok); //create~で作ったアドレスにASSIGNで値を入れる
    return new_unary(ND_EXPR_STMT, node, rhs->tok);         //値を入れることだけ目的で、その値を他の何かで使わないのでEXPR_STMR(condとかだったらcmpで使うのでExprにしなきゃいけないけど)
}

Node *lvar_init_zero(Node *cur, Var *var, Type *ty, Designator *desg)
{
    if (ty->kind == TY_ARRAY)
    { //例えばint x[2][3] = {{1,2,3} , }で[2]部分のレベルで配列が丸ごと一個分足りないときがここ
        // そのときは[1][?]は全部０にする
        for (int i = 0; i < ty->array_size; i++)
        {
            Designator desg2 = {desg, i++, NULL};
            cur = lvar_init_zero(cur, var, ty->base, &desg2);
        }

        return cur;
    }

    //ここは配列の一番下のレベルx[2][3]だったら[3]部分

    cur->next = assign_array_to_value(var, desg, new_num(0, token)); //0埋め
    return cur->next;
}

// lvar-initilaize = assign ただのassignの場合もある
//                   | "{" lvar-initializer ( "," lvar-initializer)* ","? "}"
// char x[4] = "foo"みたいなcharの配列をstringで初期化する場合、
// char x[4] = { 'f','o','o','\0'}とこちらで変えてあげればいい

//もし左辺が不完全系配列だったら、右辺のサイズを読み取って完全系にしてあげる
//int x[] = {1,2,3}なら int x[3]としてあげる

//構造体の場合は、
//struct { int a; int b; } x = { 1,2 }だったら　x.a = 1 x.b = 2としてあげればいい

Node *lvar_initializer(Node *cur, Var *var, Type *ty, Designator *desg)
{
    if (ty->kind == TY_ARRAY && ty->base->kind == TY_CHAR && token->kind == TK_STR)
    {
        Token *tok = token;
        token = token->next;

        //不完全系だった場合
        if (ty->is_incomplete)
        {
            ty->array_size = tok->count_len;
            ty->is_incomplete = false;
        }

        int len = (ty->array_size < tok->count_len) ? ty->array_size : tok->count_len;
        //lenはどちらか小さい方に合わせる,char x[4] = "fooooooo\0" だったら4で,char x[4] = "f"だったらfとして、残りは0埋め
        int i;

        for (i = 0; i < len; i++)
        {
            Designator desg2 = {desg, i, NULL};
            Node *rhs = new_num(tok->contents[i], tok);          //charはnum扱いなので
            cur->next = assign_array_to_value(var, &desg2, rhs); //*(x+0) = 'f'みたいになる
            cur = cur->next;
        }

        //ここからは0埋め
        for (; i < ty->array_size; i++)
        {
            Designator desg2 = {desg, i, NULL};
            cur = lvar_init_zero(cur, var, ty->base, &desg2);
        }

        return cur;
    }

    Token *tok = consume("{");
    if (!tok)
    {
        cur->next = assign_array_to_value(var, desg, assign()); //ここで*(*(x+0)+1) = 1みたいなのをを作る
        return cur->next;
    }

    if (ty->kind == TY_ARRAY)
    {
        int i = 0;

        do
        {
            Designator desg2 = {desg, i++, NULL}; //desgはarrayAccess用に使う
            cur = lvar_initializer(cur, var, ty->base, &desg2);
        } while (!peek_end() && consume(","));

        expect_end();

        //もし初期化の時に初期化要素数が型より不足していたら
        //例として、int x[2][3]={{1,2}}の時は,まず{1,2}のレベルだと,上のdoWhileの終了時はi=2となっているはずで、tyはarrayof(int,3)
        //なので、あと1配列が足りないのでそこを0埋め
        // { {1,2} , ... , }のレベルでは、終了時 i = 1となり、tyはarrayof(arrayof(int,3),2)
        //であと一個足りないのでそこを0埋め
        while (i < ty->array_size)
        {

            Designator desg2 = {desg, i++, NULL};
            cur = lvar_init_zero(cur, var, ty->base, &desg2);
        }

        if (ty->is_incomplete)
        {
            ty->array_size = i; //こうすることで配列のサイズ計算の時でもうまく出来るようになり、offset,sizeofもできるようになる
            ty->is_incomplete = false;
        }

        return cur;
    }

    if (ty->kind == TY_STRUCT)
    {
        Member *mem = ty->members;

        do
        {
            Designator desg2 = {desg, 0, mem};
            //構造体のmemberに対して値を割り当てていく
            cur = lvar_initializer(cur, var, mem->ty, &desg2);
            mem = mem->next;
        } while (!peek_end() && consume(","));

        expect_end();

        //0埋め
        for (; mem; mem = mem->next)
        {
            Designator desg2 = {desg, 0, mem};
            cur = lvar_init_zero(cur, var, mem->ty, &desg2);
        }

        return cur;
    }

    //このcurのnextには *(*(x+0)+1) = 1みたいな初期化が配列の分だけ入る(0初期化も含めて)

    error_tok(tok, "invalid array initializer");
}

// declaration = type-specifier declarator type-suffix ("=" lvar-initializer)? ";"
//             | type-specifier ";"
Node *declaration()
{
    Token *tok;
    Type *ty = type_specifier();

    if (tok = consume(";"))
    {
        //basetype()の中にstruct_declarationも含まれているので,ここのifでは、
        // struct x {...};みたいなtag付けを処理する
        //int ;見たいのは絶対ないけどstructについてはこういう宣言もあるので変わってる
        return new_node(ND_NULL, tok);
    }

    tok = token;

    char *name = NULL;

    ty = declarator(ty, &name);
    ty = type_suffix(ty); //suffixの分tyを変更、あるいはそのまま

    if (ty->is_typedef)
    { //typedefの時は今まではキーワードで扱っていたがtypedefも型と同じ場所で処理することにした(以前はstmtで処理していた)
        expect(";");
        ty->is_typedef = false;          //ここでis_typedefをfalseにしてただの変数に戻す,なぜ戻す？
        push_scope(name)->type_def = ty; //type_sepecifierでtypedefの宣言かどうか確認して、ここで登録

        return new_node(ND_NULL, tok);
    }

    if (ty->kind == TY_VOID)
    {
        error_tok(tok, "variable declared void");
    }

    Var *var;
    if (ty->is_static)
    {
        var = push_var(new_label(), ty, false, tok);
    }
    else
    {
        var = push_var(name, ty, true, tok); //ここでstructTypeもきちんと入る
    }

    push_scope(name)->var = var;

    if (consume(";"))
    {
        return new_node(ND_NULL, tok); //もしint x;みたいな形だったらND_NULLを返す・・・？//多分初期化式はまだ実装しないからダミーかな
    }

    expect("=");
    //ここからlvar-initializer
    Node head;
    head.next = NULL;
    lvar_initializer(&head, var, var->ty, NULL); //ここのvarは int x = {}のxのこと
    expect(";");

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    return node; //初期化する際は変数を使って新たにND_BLOCKを作成する、でBLOCKの中身を実際の初期化にする,なので、変数の宣言と、初期化を分離している
    //例として int x = { 1 };とすると(まぁこれならint x = 1でいいんだけど例として)
    // int x;
    // { x = 1;}としていることになる
    //結局新しく作るND_BLOCKのbodyは
    //{
    //  *(*(x+0)+0) = 1;
    //  *(*(x+0)+1) = 2;
    //  *(*(x+1)+0) = 3;みたいになっている
    //    *(*(x+0)+1)と*(*(x+1)+0)は,二重配列なので、x+0に入っているアドレスに+1足したそれの中身とx+1に入っているアドレス+0の中身ということ
    //
    //}
}

Node *read_expr_stmt()
{
    Token *tok = token; //現在のtokenはexprStmtなはずなので
    return new_unary(ND_EXPR_STMT, expr(), tok);
}

bool is_typename()
{
    return peek("void") || peek("_Bool") || peek("char") || peek("short") || peek("int") || peek("long") || peek("enum") || peek("struct") || peek("typedef") || peek("static") || find_typedef(token);
}

// stmt = "return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "switch" "(" expr ")" stmt
//        | "case" const-expr ":" stmt
//        | "default" ":" stmt
//        | "while" "(" expr ")" stmt
//        | "for" "(" ( expr? ";" | declaration ) expr? ";" expr? ")" stmt
//        | "{" stmt* "}"
//        | "break" ";"
//        | "continue" ";"
//        | "goto" ident ";"
//        | ident ":" stmt //例えばlabel1: {}とか
//        | declaration
//        | expr ";"

//{...}でfor(){}をして、{}の内部に複数statementを配置できる
//その時はNodeForのthenにNodeBlockが入る、NodeBlockのbodyにstmt*が入る

Node *stmt()
{
    Token *tok;
    if (tok = consume("return"))
    {
        Node *node = new_unary(ND_RETURN, expr(), tok);
        expect(";");
        return node;
    }

    //tokenがTK_RESERVEDで、tok->strが指すアドレスから二文字分memcmpしてifと合致するなら
    if (tok = consume("if"))
    {
        Node *node = new_node(ND_IF, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else"))
            node->els = stmt();
        return node;
    }

    if (tok = consume("switch"))
    {
        Node *node = new_node(ND_SWITCH, tok);
        expect("(");
        node->cond = expr();
        expect(")");

        Node *sw = current_switch;
        current_switch = node; //stmt中ではnodeがcurrent_switchとなる
        node->then = stmt();
        current_switch = sw;
        return node;
    }

    //case,defaultは上記のstmt中のこと、stmt中でcurrent_switchにcase,defaultをどんどんつなげていく
    if (tok = consume("case"))
    {
        if (!current_switch)
        {
            error_tok(tok, "stray case");
        }

        int val = const_expr();
        expect(":");

        Node *node = new_unary(ND_CASE, stmt(), tok);
        node->val = val;
        node->case_next = current_switch->case_next; //ここでswitchのnode=current_switchのcase_nextにcase式をどんどんつないでいく
        current_switch->case_next = node;
        return node;
    }

    if (tok = consume("default"))
    {
        if (!current_switch)
        {
            error_tok(tok, "stray default");
        }

        expect(":");

        Node *node = new_unary(ND_CASE, stmt(), tok);
        current_switch->default_case = node;
        return node;
    }

    if (tok = consume("while"))
    {
        Node *node = new_node(ND_WHILE, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if (tok = consume("for"))
    {
        Node *node = new_node(ND_FOR, tok);
        expect("(");

        Scope *sc = enter_scope();

        if (!consume(";"))
        {
            if (is_typename())
            {
                node->init = declaration();
            }
            else
            {
                node->init = read_expr_stmt(); //i=0は式文で、スタックにpushされた値は使わないので捨てる、別に捨てなくても動くとは思うけど、alignが面倒になる?
                //i < 0だとこの結果を捨てたくないので式
                expect(";");
            }
        }

        if (!consume(";"))
        {
            node->cond = expr();
            expect(";");
        }

        if (!consume(")"))
        {
            node->inc = read_expr_stmt();
            expect(")");
        }
        node->then = stmt();

        //for内のint i=0;みたいな宣言したローカル変数もfor{}のBlockのみでだけいるので、scopeをもどしてあげる

        leave_scope(sc);
        return node;
    }

    if (tok = consume("{"))
    {
        Node head;
        head.next = NULL;
        Node *cur = &head;

        //NodeBlockにscopeを追加,ここのscopeの生存期間はBlockの中でだけ
        Scope *sc = enter_scope();

        while (!consume("}"))
        {
            cur->next = stmt();
            cur = cur->next;
        }

        leave_scope(sc);

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }

    if (tok = consume("break"))
    {
        expect(";");
        return new_node(ND_BREAK, tok);
    }

    if (tok = consume("continue"))
    {
        expect(";");
        return new_node(ND_CONTINUE, tok);
    }

    if (tok = consume("goto"))
    {
        Node *node = new_node(ND_GOTO, tok);
        node->label_name = expect_ident();
        expect(";");
        return node;
    }

    if (tok = consume_ident())
    {
        if (consume(":"))
        {
            Node *node = new_unary(ND_LABEL, stmt(), tok);
            node->label_name = strndup(tok->str, tok->len);
            return node;
        }

        token = tok; // identのあとに":"が来ないなら処理しなくていいので元に戻す
    }

    if (is_typename())
        return declaration();

    Node *node = read_expr_stmt();
    expect(";");
    return node;
}

//expr = assign ("," assign)*
Node *expr()
{
    Node *node = assign();
    Token *tok;
    while (tok = consume(","))
    {
        node = new_unary(ND_EXPR_STMT, node, node->tok);
        node = new_binary(ND_COMMA, node, assign(), tok); //lhsをEXPR＿STMTにすることで常に最後の一つだけがスタックに積まれた状態となる
    }

    return node;
}

long eval(Node *node)
{
    //const_exprからevalで評価できるのは計算系に限る、例えばND_IFとかを渡してもダメ
    // case if(){} :みたいなのはダメ、ただ三項演算子はOK case x ? 1 : 0 :{}
    switch (node->kind)
    {
    case ND_ADD:
        return eval(node->lhs) + eval(node->rhs);
    case ND_SUB:
        return eval(node->lhs) - eval(node->rhs);
    case ND_MUL:
        return eval(node->lhs) * eval(node->rhs);
    case ND_DIV:
        return eval(node->lhs) / eval(node->rhs);
    case ND_BITAND:
        return eval(node->lhs) & eval(node->rhs);
    case ND_BITOR:
        return eval(node->lhs) | eval(node->rhs);
    case ND_BITXOR:
        return eval(node->lhs) | eval(node->rhs);
    case ND_SHL:
        return eval(node->lhs) << eval(node->rhs);
    case ND_SHR:
        return eval(node->lhs) >> eval(node->rhs);
    case ND_EQ:
        return eval(node->lhs) == eval(node->rhs);
    case ND_NE:
        return eval(node->lhs) != eval(node->rhs);
    case ND_LT:
        return eval(node->lhs) < eval(node->rhs);
    case ND_LE:
        return eval(node->lhs) <= eval(node->rhs);
    case ND_TERNARY:
        return eval(node->cond) ? eval(node->then) : eval(node->els);
    case ND_COMMA:
        return eval(node->rhs);
    case ND_NOT:
        return !eval(node->lhs);
    case ND_BITNOT:
        return ~eval(node->lhs);
    case ND_LOGAND:
        return eval(node->lhs) && eval(node->rhs);
    case ND_LOGOR:
        return eval(node->lhs) || eval(node->rhs);
    case ND_NUM:
        return node->val;
    }

    error_tok(node->tok, "not a constant expression");
}

long const_expr()
{
    return eval(conditional());
}
//例えばa=1;はNode(ASSIGN,left=Lvar,right=NUM)となる

//この時点ではa+1=10みたいなのも作ってしまうがしょうがない
// assign = equality ("=" assign)?//複数代入も想定しているので"="" assignとしているっぽい？

// assign = conditional (assign-op assign)?
//assign-op = "=" | "+=" | "-=" | "*=" | "/=" | "<<=" | ">>="
Node *assign()
{
    Node *node = conditional();

    Token *tok;
    if (tok = consume("="))
        node = new_binary(ND_ASSIGN, node, assign(), tok);
    if (tok = consume("+="))
        node = new_binary(ND_A_ADD, node, assign(), tok);
    if (tok = consume("-="))
        node = new_binary(ND_A_SUB, node, assign(), tok);
    if (tok = consume("*="))
        node = new_binary(ND_A_MUL, node, assign(), tok);
    if (tok = consume("/="))
        node = new_binary(ND_A_DIV, node, assign(), tok);
    if (tok = consume("<<="))
        node = new_binary(ND_A_SHL, node, assign(), tok);
    if (tok = consume(">>="))
        node = new_binary(ND_A_SHR, node, assign(), tok);
    return node;
}

// conditional = logor ( "?" expr ":" condtional )?
Node *conditional()
{
    Node *node = logor();
    Token *tok = consume("?");
    if (!tok)
    {
        return node;
    }

    Node *ternary = new_node(ND_TERNARY, tok);
    ternary->cond = node;
    ternary->then = expr();
    expect(":");
    ternary->els = conditional();
    return ternary;
}
// logor = logand ( "||" logand)*
Node *logor()
{
    Node *node = logand();
    Token *tok;
    while (tok = consume("||"))
    {
        node = new_binary(ND_LOGOR, node, logand(), tok);
    }

    return node;
}

// logand = bitor ( "&&" bitor )*
Node *logand()
{
    Node *node = bitor ();
    Token *tok;

    while (tok = consume("&&"))
    {
        node = new_binary(ND_LOGAND, node, bitor (), tok);
    }

    return node;
}

//bitor = bitxor ( "|" bitxor)*
Node * bitor ()
{
    Node *node = bitxor();
    Token *tok;
    while (tok = consume("|"))
    {
        node = new_binary(ND_BITOR, node, bitxor(), tok);
    }

    return node;
}

//bitxor = bitand ( "^" bitand)*
Node *bitxor()
{
    Node *node = bitand();
    Token *tok;
    while (tok = consume("^"))
    {
        node = new_binary(ND_BITXOR, node, bitand(), tok);
    }

    return node;
}

//bitand = equality ( "&" equality)*
Node *bitand()
{
    Node *node = equality();
    Token *tok;
    while (tok = consume("&"))
    {
        node = new_binary(ND_BITAND, node, equality(), tok);
    }
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality()
{
    Node *node = relational();
    Token *tok;
    for (;;)
    {
        if (tok = consume("=="))
            node = new_binary(ND_EQ, node, relational(), tok);
        else if (tok = consume("!="))
            node = new_binary(ND_NE, node, relational(), tok);
        else
            return node;
    }
}

// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*

Node *relational()
{
    Node *node = shift();
    Token *tok;

    for (;;)
    {
        if (tok = consume("<"))
            node = new_binary(ND_LT, node, shift(), tok);
        else if (tok = consume("<="))
            node = new_binary(ND_LE, node, shift(), tok);
        else if (tok = consume(">"))
            node = new_binary(ND_LT, shift(), node, tok); //逆にして対応する
        else if (tok = consume(">="))
            node = new_binary(ND_LE, shift(), node, tok);
        else
            return node;
    }
}

// shift = add ( "<<" add | ">>" add)*
Node *shift()
{
    Node *node = add();
    Token *tok;

    for (;;)
    {
        if (tok = consume("<<"))
        {
            node = new_binary(ND_SHL, node, add(), tok);
        }
        else if (tok = consume(">>"))
        {
            node = new_binary(ND_SHR, node, add(), tok);
        }
        else
        {
            return node;
        }
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add()
{
    Node *node = mul();
    Token *tok;
    for (;;)
    {
        if (tok = consume("+"))
            node = new_binary(ND_ADD, node, mul(), tok);

        else if (tok = consume("-"))
            node = new_binary(ND_SUB, node, mul(), tok);
        else
            return node;
    }
}

//mul = cast ("*" cast | "/" cast)*
Node *mul()
{
    Node *node = cast();
    Token *tok;
    for (;;)
    {
        if (tok = consume("*"))
            node = new_binary(ND_MUL, node, cast(), tok);
        else if (tok = consume("/"))
            node = new_binary(ND_DIV, node, cast(), tok);
        else
            return node;
    }
}

// cast = "(" type-name ")" cast | unary
Node *cast()
{
    Token *tok = token;

    if (consume("("))
    {
        //typeCastの場合
        if (is_typename())
        {
            Type *ty = type_name();
            expect(")");
            Node *node = new_unary(ND_CAST, cast(), tok);
            node->ty = ty; //ND_CASTのタイプはcastするtype
            return node;
        }
        token = tok; //typenameで無い場合tokenを戻す
    }

    return unary();
}

//unary = ("+" | "-" | "*" | "&" | "!" | "~" )? cast
//        | ("++" | "--") unary
//        | postfix
Node *unary()
{
    Token *tok;
    if (tok = consume("+"))
    {
        return cast();
    }
    if (tok = consume("-"))
    {
        //0-?の計算をすることで-を作る
        return new_binary(ND_SUB, new_num(0, tok), cast(), tok);
    }
    if (tok = consume("&"))
    {
        return new_unary(ND_ADDR, cast(), tok);
    }
    if (tok = consume("*"))
    {
        return new_unary(ND_DEREF, cast(), tok);
    }
    if (tok = consume("!"))
    {
        return new_unary(ND_NOT, cast(), tok);
    }
    if (tok = consume("~"))
    {
        return new_unary(ND_BITNOT, cast(), tok);
    }
    if (tok = consume("++"))
    {
        return new_unary(ND_PRE_INC, unary(), tok);
    }
    if (tok = consume("--"))
    {
        return new_unary(ND_PRE_DEC, unary(), tok);
    }

    return postfix();
}

// postfix = primary ( "[" expr "]"  |  "." ident  | "->" ident | "++" | "--" )*
Node *postfix()
{
    Node *node = primary();
    Token *tok;

    for (;;)
    {
        if (tok = consume("["))
        {
            //x[y] is short for *(x+y)
            //x[2][3]があったとして、最初のx[2]を読み取るときND_ADDのtypeは変数のxとなり、
            //ここで変数のxのtypeは宣言時にtype_suffixでarrayof(arrayof(int,3),2)となっていたので、(x+0)のtypeはarrayof(arrayof(int,3),2)でaddの時にはそのbase array_of(int,3)が使われる
            //*(x+0)+1の時は,addのtypeは一回目のやつで作ったNDDEREFのtypeになる、ND_DEREFのtypeはnode->lhs->baseなのでarrayof(arrayof(int,3),2)->baseのarrayof(int,3)となる
            //実際のaddの時はarray_of(int,3)のbase、intが使われる
            Node *exp = new_binary(ND_ADD, node, expr(), tok);
            expect("]");
            node = new_unary(ND_DEREF, exp, tok);

            continue;
        }

        if (tok = consume("."))
        {
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident(); //流れとしてはparseで構造体の宣言、memberのoffsetを計算、そのあとtype.cで . で実際に呼ぶmember_nameからmember_offsetを入手

            continue;
        }

        if (tok = consume("->"))
        {
            // x->y is short for (*x).y
            // . と　->　は用法が違って, ~.aは直接構造体から、 ~ -> aは構造体の先頭アドレスを格納したポインタからのアクセス,なので->はポインタを介したアクセス
            //なので struct x {int a;}; *y  = &xで y->aとしたとき
            //yは上でprimary()として計算され、それがDEREFに入る　
            node = new_unary(ND_DEREF, node, tok);
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident();
            continue;
        }

        if (tok = consume("++"))
        {
            node = new_unary(ND_POST_INC, node, tok);
            continue;
        }

        if (tok = consume("--"))
        {
            node = new_unary(ND_POST_DEC, node, tok);
            continue;
        }

        return node;
    }
}

//func-args = "(" (assign ("," assign)* )? ")"
Node *func_args()
{
    if (consume(")"))
        return NULL;

    Node *head = assign();
    Node *cur = head;
    while (consume(","))
    {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");

    return head;
}

// stmt-expr = "(" "{" stmt stmt* "}" ")"
//StatementExpressionは({...})みたいに使う、BlockStatementと区別したい
//で、返すのはExpressionStatement
//Statement expression is a GNU C extensionらしい
Node *stmt_expr(Token *tok)
{
    Scope *sc = enter_scope();

    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    while (!consume("}"))
    {
        cur->next = stmt();
        cur = cur->next;
    }

    expect(")");

    leave_scope(sc);
    //stmtの中を巡ったのでscopeをBlockに入る前のscopeに戻している

    if (cur->kind != ND_EXPR_STMT)
    {
        error_tok(cur->tok, "stmt expr returning void is not supported");
    }

    *cur = *cur->lhs; //return ({ 0; 1; 2; });とあるとしたら0;1;2;は全部ExprStatementだけど、最後だけlhsをとってnumにする、これは型を付けるときに最後のstaementをみてTypeを付けるため
    //ここまで来た時のcurは最後、上記の例なら2;のExprStatement(curはどんどんnextに行くので最初node->bodyの時のcurではない,node->bodyは相変わらずcurを指したままだけど)

    // Node *read_expr_stmt()でnew_unary(ND_EXPR_STMT, expr(), tok);を返すので、ND_EXPR_STMTのlhsということ

    return node;
}

//primary = "(" "{" stmt-expr-tail "}" ")"
//           |  "(" expr ")" | "sizeof" unary | ident func-args? | str | num
//           |  "sizeof" "(" type-name ")"

Node *primary()
{
    Token *tok;

    if (consume("("))
    {
        if (consume("{"))
        { //"(" "{" の並びはStatementExpression
            return stmt_expr(tok);
        }

        //次のトークンが"("なら、"("　expr　")"となっているはず
        Node *node = expr();
        expect(")");
        return node;
    }

    if (tok = consume("sizeof"))
    {
        //sizeof exprでなくsizeof typeの場合
        if (consume("("))
        {
            if (is_typename())
            {
                Type *ty = type_name();
                expect(")");
                return new_num(size_of(ty, tok), tok);
            }

            token = tok->next;
        }
        return new_unary(ND_SIZEOF, unary(), tok);
    }

    if (tok = consume_ident())
    {
        //identのあとに"("が来ていれば関数呼び出し
        if (consume("("))
        {
            Node *node = new_node(ND_FUNCALL, tok);
            node->funcname = strndup(tok->str, tok->len); //identの名前をコピー,funcnameはもうlabelによって登録されているのでVarみたいな処理はいらない
            node->args = func_args();

            //すでに関数宣言で関数が変数として登録されているはずなので
            VarScope *sc = find_var(tok);
            if (sc)
            {
                if (!sc->var || sc->var->ty->kind != TY_FUNC)
                {
                    error_tok(tok, "not a function");
                }

                node->ty = sc->var->ty->return_ty; //ND_FUNCALLのTYPEは返り値の値になる
            }
            else
            {
                node->ty = int_type();
            }

            return node;
        }

        //普通のidentであってもint x;かint x = 5;みたいに事前に宣言されているはずなのでfind_varできなかったらエラー
        VarScope *sc = find_var(tok);

        if (sc)
        {
            if (sc->var)
                return new_var(sc->var, tok);
            if (sc->enum_ty) //enum_specifierで登録済みだったら
                return new_num(sc->enum_val, tok);
        }
        error_tok(tok, "undefined variable");
    }

    tok = token;

    if (tok->kind == TK_STR)
    {

        token = token->next; //これはただ次のtokenに行くだけ

        Type *ty = array_of(char_type(), tok->count_len);
        Var *var = push_var(new_label(), ty, false, NULL);
        var->contents = tok->contents;
        var->count_len = tok->count_len;

        return new_var(var, tok);
    }

    if (tok->kind != TK_NUM)
        error_tok(tok, "expected num");

    //そうでなければ数値
    return new_num(expect_number(), tok);
}
