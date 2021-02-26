#include "Mycc.h"

// Scope for local var,global var or typedefs

typedef struct VarScope VarScope;

struct VarScope
{
    VarScope *next;
    char *name;
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

Scope *enter_scope()
{
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    return sc;
}

void leave_scope(Scope *sc)
{
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
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
Node *assign();
Node *logor();
Node *logand();
Node *bitand();
Node * bitor ();
Node *bitxor();
Node *equality();
Node *relational();
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

// type-suffix = ( "[" num? "]" type-suffix)?
Type *type_suffix(Type *ty)
{
    if (!consume("["))
        return ty; //そのまま返すだけ

    //Arrayの場合
    int sz = 0;
    bool is_incomplete = true;
    if (!consume("]"))
    {
        //数字が書いてあって完全系なら
        sz = expect_number();
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
    sc->ty = ty;
    tag_scope = sc;
}

// struct-declaration = "struct" ident | "struct"  ident? "{" struct-member "}"
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
            error_tok(tag, "unknown struct type"); //構造体宣言までに、tagを使うのであればTagを登録しておくこと
        if (sc->ty->kind != TY_STRUCT)
            error_tok(tag, "not a struct tag");
        return sc->ty;
    }

    //ここからはident {...}ならTag付けしつつ利用、普通にstruct {...}なら普通に利用

    expect("{");

    // Read struct members;
    Member head;
    head.next = NULL;
    Member *cur = &head;

    while (!consume("}"))
    {
        cur->next = struct_member();
        cur = cur->next;
    }

    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
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

    // Register the struct type if a name was given.
    if (tag)
        push_tag_scope(tag, ty);

    return ty;
}

//enum-specifier = "enum" ident
//               | "enum" ident? "{" enum-list? "}"
//
//enum-list = ident ( "=" num)? ( "," ident ( "=" num )? )* ","?
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
            cnt = expect_number();

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

// declaration = type-specifier declarator type-suffix ("=" expr)? ";"
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
    Node *lhs = new_var(var, tok);
    Node *rhs = expr();
    expect(";");
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
    return new_unary(ND_EXPR_STMT, node, tok);
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
//        | "while" "(" expr ")" stmt
//        | "for" "(" ( expr? ";" | declaration ) expr? ";" expr? ")" stmt
//        | "{" stmt* "}"
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
//例えばa=1;はNode(ASSIGN,left=Lvar,right=NUM)となる

//この時点ではa+1=10みたいなのも作ってしまうがしょうがない
// assign = equality ("=" assign)?//複数代入も想定しているので"="" assignとしているっぽい？

// assign = logor (assign-op assign)?
//assign-op = "=" | "+=" | "-=" | "*=" | "/="
Node *assign()
{
    Node *node = logor();

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
    return node;
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

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*

Node *relational()
{
    Node *node = add();
    Token *tok;

    for (;;)
    {
        if (tok = consume("<"))
            node = new_binary(ND_LT, node, add(), tok);
        else if (tok = consume("<="))
            node = new_binary(ND_LE, node, add(), tok);
        else if (tok = consume(">"))
            node = new_binary(ND_LT, add(), node, tok); //逆にして対応する
        else if (tok = consume(">="))
            node = new_binary(ND_LE, add(), node, tok);
        else
            return node;
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
