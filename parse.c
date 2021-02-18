#include "Mycc.h"

VarList *locals;

Var *find_var(Token *tok)
{
    for (VarList *vl = locals; vl; vl = vl->next)
    {
        Var *var = vl->var;
        //memcmpは一致すれば０が返るので、!0で一致すればtrueを返す
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
            return var;
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
Var *push_var(char *name, Type *ty)
{
    //新しい奴から先頭に来る
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

Function *function();
Node *declaration();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// program = function*

Function *program()
{

    Function head;
    head.next = NULL;
    Function *cur = &head;

    while (!at_eof())
    {
        cur->next = function();
        cur = cur->next;
    }

    return head.next;
}

// basetype = "int" "*"* //*が0個以上
Type *basetype()
{
    expect("int");
    Type *ty = int_type();
    while (consume("*"))
        ty = pointer_to(ty); //例えばint **だったらtyはTY_PTR(
                             //                       base=TY_PTR(
                             //                              base=TY_INT
                             //                                   )
                             //                              )となる

    return ty;
}

VarList *read_func_param()
{
    //一つ目の引数だけここで取得
    VarList *vl = calloc(1, sizeof(VarList));
    //fn(int x,int y)みたいになっているので"("の次はbasetypeが来ている

    Type *ty = basetype();
    //typeの次はIdentifier
    vl->var = push_var(expect_ident(), ty); //引数もpush_varしているのでまとめてその関数のlocalsに入ることになる
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

// declaration = basetype ident ("=" expr) ";"
Node *declaration()
{
    Token *tok = token;
    Type *ty = basetype();
    Var *var = push_var(expect_ident(), ty);

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

        while (!consume("}"))
        {
            cur->next = stmt();
            cur = cur->next;
        }

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }

    if (tok = peek("int"))
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

//unary = ("+" | "-" | "*" | "&")? unary | primary
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

    return primary();
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

//primary = "(" expr ")" | ident func-args?| num
Node *primary()
{
    if (consume("("))
    {
        //次のトークンが"("なら、"("　expr　")"となっているはず
        Node *node = expr();
        expect(")");
        return node;
    }

    Token *tok;
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
    if (tok->kind != TK_NUM)
        error_tok(tok, "expected num");

    //そうでなければ数値
    return new_num(expect_number(), tok);
}
