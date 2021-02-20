#include "Mycc.h"

// Scope For Struct Tag
typedef struct TagScope TagScope;
struct TagScope
{
    TagScope *next;
    char *name;
    Type *ty;
};

VarList *locals;
VarList *globals;
VarList *scope; //そこまでに含まれている変数、localsは関数全体だが、scopeはint main(){int x; int y;}のint xの部分までだったらxまでとなる
TagScope *tag_scope;

// Find variable by name.
Var *find_var(Token *tok)
{

    // //For Locals
    // for (VarList *vl = locals; vl; vl = vl->next)
    // {
    //     Var *var = vl->var;
    //     //memcmpは一致すれば０が返るので、!0で一致すればtrueを返す
    //     if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
    //         return var;
    // }

    // //For Globals
    // for (VarList *vl = globals; vl; vl = vl->next)
    // {
    //     Var *var = vl->var;
    //     //memcmpは一致すれば０が返るので、!0で一致すればtrueを返す
    //     if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
    //         return var;
    // }

    //scopeのみでfindすると例えば
    //'int main() { int x=2; { int x=3; } return x; }'
    //みたいなやつでreturn　xのときにfindしてもx=2しかない
    //ただ、localの方にはx=2とx=3のやつがあるし、Block内でfindすると後に追加されたx=3のやつが来る
    //単にローカル、グローバル関係なくスコープの中から探せばいい
    for (VarList *vl = scope; vl; vl = vl->next)
    {
        Var *var = vl->var;
        //memcmpは一致すれば０が返るので、!0で一致すればtrueを返す
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
            return var;
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

Node *new_num(int val, Token *tok)
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

//NodeごとにTypeを入れるときに、Varの場合はnode->ty = node->var->tyとなる
//localsに追加
Var *push_var(char *name, Type *ty, bool is_local)
{
    //新しい奴から先頭に来る
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;

    if (is_local)
    {
        vl->next = locals;
        locals = vl;
    }
    else
    {
        vl->next = globals;
        globals = vl;
    }

    //ローカル、グローバル関係なく現時点までの変数をすべて追加
    VarList *sc = calloc(1, sizeof(VarList));
    sc->var = var;
    sc->next = scope;
    scope = sc;

    return var;
}

char *new_label()
{
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

Function *function();
Type *basetype();
Type *struct_declaration();
Member *struct_member();
void global_var();
Node *declaration();
bool is_typename();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *postfix();
Node *primary();

//functionとgloval変数は似ているため見分ける関数が必要
//int x[10];とint x(...)
bool is_function()
{
    Token *tok = token;
    basetype();

    bool is_func = consume_ident() && consume("(");
    token = tok; //読んだ分戻してあげる
    return is_func;
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
            cur->next = function();
            cur = cur->next;
        }
        else
        {
            global_var();
        }
    }

    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}

// basetype = ("char" | int" | struct-declaration) "*"* //*が0個以上
Type *basetype()
{
    if (!is_typename(token))
        error_tok(token, "typename expected");

    Type *ty;

    if (consume("char"))
    {
        ty = char_type();
    }
    else if (consume("int"))
    {
        ty = int_type();
    }
    else
    {
        ty = struct_declaration();
    }

    while (consume("*"))
        ty = pointer_to(ty); //例えばint **だったらtyはTY_PTR(
                             //                       base=TY_PTR(
                             //                              base=TY_INT
                             //                                   )
                             //                              )となる

    return ty;
}

Type *read_type_suffix(Type *base)
{
    if (!consume("["))
        return base;          //そのまま返すだけ
    int sz = expect_number(); // "["の次は1とかが来る
    expect("]");
    base = read_type_suffix(base);
    return array_of(base, sz); //int[2]だとarray_od(int,2)でint[2][3]だとarray_of(array_of(int,3),2)となる
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
        offset += size_of(mem->ty);

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

// struct-member = basetype ident ( "[" num "]" )* ";"

Member *struct_member()
{
    Member *mem = calloc(1, sizeof(Member));
    mem->ty = basetype();
    mem->name = expect_ident();
    mem->ty = read_type_suffix(mem->ty);
    expect(";");
    return mem;
}

VarList *read_func_param()
{
    //fn(int x,int y)みたいになっているので"("の次はbasetypeが来ている

    Type *ty = basetype();
    //typeの次はIdentifier
    char *name = expect_ident();
    ty = read_type_suffix(ty);

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = push_var(name, ty, true); //引数もpush_varしているのでまとめてその関数のlocalsに入ることになる
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

// function = basetype ident "(" params?")" "{" stmt* "}"
// params = ident ("," ident)*
// param  = basetype ident
Function *function()
{
    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    basetype(); //ここでは関数自体の戻り値はまだ使わないので、basetypeが来てるかだけ確認
    fn->name = expect_ident();

    expect("(");
    fn->params = read_func_params();
    expect("{");

    Node head;
    head.next = NULL;
    Node *cur = &head;

    while (!consume("}"))
    {
        cur->next = stmt();
        cur = cur->next;
    }

    fn->node = head.next;
    fn->locals = locals; //functionを取得するたびにLocalsはNULLとなるのでFunctionごとのLocal
    return fn;
}

//global-var = basetype ident ( "[" num "]" )* ";"
void global_var()
{
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    expect(";");
    push_var(name, ty, false);
}

// declaration = basetype ident  ("[" num "]")*  ("=" expr) ";"
//              | basetype ";"
Node *declaration()
{
    Token *tok = token;
    Type *ty = basetype();

    if (consume(";"))
    {
        //basetype()の中にstruct\declarationも含まれているので,ここのifでは、
        // struct x {...};みたいなtag付けを処理する
        //int ;見たいのは絶対ないけどstructについてはこういう宣言もあるので変わってる
        return new_node(ND_NULL, tok);
    }

    char *name = expect_ident();
    ty = read_type_suffix(ty);           //suffixの分tyを変更、あるいはそのまま
    Var *var = push_var(name, ty, true); //ここでstructTypeもきちんと入る

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
    return peek("char") || peek("int") || peek("struct");
}

// stmt = "return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "while" "(" expr ")" stmt
//        | "for" "(" expr? ";" expr? ";" expr? ")" stmt
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

        if (!consume(";"))
        {
            node->init = read_expr_stmt(); //i=0は式文で、スタックにpushされた値は使わないので捨てる、別に捨てなくても動くとは思うけど、alignが面倒になる?
            //i < 0だとこの結果を捨てたくないので式
            expect(";");
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
        return node;
    }

    if (tok = consume("{"))
    {
        Node head;
        head.next = NULL;
        Node *cur = &head;

        //NodeBlockにscopeを追加,ここのscopeの生存期間はBlockの中でだけ
        VarList *sc1 = scope;
        TagScope *sc2 = tag_scope;

        while (!consume("}"))
        {
            cur->next = stmt();
            cur = cur->next;
        }

        scope = sc1; //stmtの中を巡ったのでscopeをBlockに入る前のscopeに戻している
        tag_scope = sc2;

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

//expr = assign
Node *expr()
{
    return assign();
}
//例えばa=1;はNode(ASSIGN,left=Lvar,right=NUM)となる

//この時点ではa+1=10みたいなのも作ってしまうがしょうがない
// assign = equality ("=" assign)?//複数代入も想定しているので"="" assignとしているっぽい？
Node *assign()
{
    Node *node = equality();

    Token *tok;
    if (tok = consume("="))
        node = new_binary(ND_ASSIGN, node, assign(), tok);
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

//mul = unary ("*" unary | "/" unary)*
Node *mul()
{
    Node *node = unary();
    Token *tok;
    for (;;)
    {
        if (tok = consume("*"))
            node = new_binary(ND_MUL, node, unary(), tok);
        else if (tok = consume("/"))
            node = new_binary(ND_DIV, node, unary(), tok);
        else
            return node;
    }
}

//unary = ("+" | "-" | "*" | "&")? unary | postfix
Node *unary()
{
    Token *tok;
    if (tok = consume("+"))
    {
        return unary();
    }
    if (tok = consume("-"))
    {
        //0-?の計算をすることで-を作る
        return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
    }
    if (tok = consume("&"))
    {
        return new_unary(ND_ADDR, unary(), tok);
    }
    if (tok = consume("*"))
    {
        return new_unary(ND_DEREF, unary(), tok);
    }

    return postfix();
}

// postfix = primary ( "[" expr "]"  |  "." ident  | "->" ident)*
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
    VarList *sc1 = scope;
    TagScope *sc2 = tag_scope;

    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    while (!consume("}"))
    {
        cur->next = stmt();
        cur = cur->next;
    }

    expect(")");

    scope = sc1;
    tag_scope = sc2;
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
        return new_unary(ND_SIZEOF, unary(), tok);
    if (tok = consume_ident())
    {
        //identのあとに"("が来ていれば関数呼び出し
        if (consume("("))
        {
            Node *node = new_node(ND_FUNCALL, tok);
            node->funcname = strndup(tok->str, tok->len); //identの名前をコピー,funcnameはもうlabelによって登録されているのでVarみたいな処理はいらない
            node->args = func_args();
            return node;
        }

        //普通のidentであってもint x;かint x = 5;みたいに事前に宣言されているはずなのでfind_varできなかったらエラー
        Var *var = find_var(tok);
        if (!var)
        {
            error_tok(tok, "undefined variable");
        }
        return new_var(var, tok);
    }

    tok = token; //primaryでnumの場合

    if (tok->kind == TK_STR)
    {

        token = token->next;

        Type *ty = array_of(char_type(), tok->count_len);
        Var *var = push_var(new_label(), ty, false);
        var->contents = tok->contents;
        var->count_len = tok->count_len;

        return new_var(var, tok);
    }

    if (tok->kind != TK_NUM)
        error_tok(tok, "expected num");

    //そうでなければ数値
    return new_num(expect_number(), tok);
}
