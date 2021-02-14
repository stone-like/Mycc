#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    TK_RESERVED, //記号
    TK_NUM,      //整数
    TK_EOF,
} TokenKind;

typedef struct Token Token;

struct Token
{
    TokenKind kind; // トークンの型
    Token *next;    // 次の入力トークン
    int val;        // kindがTK_NUMの場合、その数値
    char *str;      // トークン文字列
    int len;        // トークンの長さ
};
//現在着目しているトークン
Token *token;

char *user_input;

void error_at(char *loc, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input; //アドレスの場所の差をとる,例えばアドレス400がLocで390がuser_inputだったら10が出てくる
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " "); // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

//error報告用関数
void error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

bool consume(char *op)
{
    //二文字以上の演算子を取れるように改良
    if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
        return false;
    token = token->next;
    return true;
}

void expect(char *op)
{
    // 次のトークンが期待している記号のときには、トークンを1つ読み進める。
    // それ以外の場合にはエラーを報告する。
    if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
        error_at(token->str, "'%c'ではありません", op);
    token = token->next;
}

int expect_number()
{
    if (token->kind != TK_NUM)
        error_at(token->str, "数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof()
{
    return token->kind == TK_EOF;
}

Token *new_token(TokenKind kind, Token *cur, char *str, int len)
{
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind,
    tok->str = str; //*tokはtokのアドレス内には構造体の先頭アドレスが入っていて、tok->strはtokのstrの保有するアドレス、*strには入力された文字のアドレスが入っているのでtok->strが指すアドレスを*strが指すアドレスと同じにしてあげる
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool startswith(char *p, char *q)
{
    return memcmp(p, q, strlen(q)) == 0;
}

Token *tokenize()
{

    char *p = user_input;

    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p)
    {
        if (isspace(*p))
        {
            p++;
            continue;
        }
        //複数文字に対応させる

        if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">="))
        {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2; //二つ分進めてあげる
            continue;
        }

        // Single-letter punctuator
        if (strchr("+-*/()<>", *p))
        {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        if (isdigit(*p))
        {
            cur = new_token(TK_NUM, cur, p, 0);
            //ここでtoken生成のときに整数を入れないのは、例えば11みたいなのだと1がふた二つ連続したものではなく続く数字を一つのtokenとして作りたい

            char *q = p;

            cur->val = strtol(p, &p, 10); //strtoiのあとpは整数が終わったところまで進んでいる
            cur->len = p - q;             //strtolで続く数字の分だけ進むので、整数のlenはポインタが進んだ分のアドレスの差分でとる
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0); //eofはlen0
    return head.next;
}

typedef enum
{
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_EQ, // ==
    ND_NE, // !=
    ND_LT, // <
    ND_LE, // <=    >,>=は引数を逆にすればいい
    ND_NUM,

} NodeKind;

//Node＝非終端記号でいい？
typedef struct Node Node;

struct Node
{
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int val; //KindがND_NUMの場合のみ使用
};

Node *new_node(NodeKind kind)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs)
{
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_num(int val)
{
    Node *node = new_node(ND_NUM);
    node->val = val;
    return node;
}

Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

//expr = equality
Node *expr()
{
    return equality();
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality()
{
    Node *node = relational();

    for (;;)
    {
        if (consume("=="))
            node = new_binary(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_binary(ND_NE, node, relational());
        else
            return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*

Node *relational()
{
    Node *node = add();

    for (;;)
    {
        if (consume("<"))
            node = new_binary(ND_LT, node, add());
        else if (consume("<="))
            node = new_binary(ND_LE, node, add());
        else if (consume(">"))
            node = new_binary(ND_LT, add(), node); //逆にして対応する
        else if (consume(">="))
            node = new_binary(ND_LE, add(), node);
        else
            return node;
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add()
{
    Node *node = mul();

    for (;;)
    {
        if (consume("+"))
            node = new_binary(ND_ADD, node, mul());

        else if (consume("-"))
            node = new_binary(ND_SUB, node, mul());
        else
            return node;
    }
}

//mul = unary ("*" unary | "/" unary)*
Node *mul()
{
    Node *node = unary();

    for (;;)
    {
        if (consume("*"))
            node = new_binary(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_binary(ND_DIV, node, unary());
        else
            return node;
    }
}

//unary = ("+" | "-")? primary
Node *unary()
{
    if (consume("+"))
    {
        return unary();
    }
    if (consume("-"))
    {
        //0-?の計算をすることで-を作る
        return new_binary(ND_SUB, new_num(0), unary());
    }

    return primary();
}

//primary = "(" expr ")" | num
Node *primary()
{
    if (consume("("))
    {
        //次のトークンが"("なら、"("　expr　")"となっているはず
        Node *node = expr();
        expect(")");
        return node;
    }

    //そうでなければ数値
    return new_num(expect_number());
}

void gen(Node *node)
{
    if (node->kind == ND_NUM)
    {
        printf("   push %d\n", node->val);
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("   pop rdi\n");
    printf("   pop rax\n");

    switch (node->kind)
    {
    case ND_ADD:
        printf("   add rax, rdi\n");
        break;
    case ND_SUB:
        printf("   sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("   imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("   cqo\n");
        printf("   idiv rdi\n");
        break;
    case ND_EQ:
        printf("   cmp rax, rdi\n");
        printf("   sete al\n");
        printf("   movzb rax, al\n");
        break;
    case ND_NE:
        printf("   cmp rax, rdi\n");
        printf("   setne al\n");
        printf("   movzb rax, al\n");
        break;
    case ND_LT:
        printf("   cmp rax, rdi\n");
        printf("   setl al\n");
        printf("   movzb rax,al\n");
        break;
    case ND_LE:
        printf("   cmp rax, rdi\n");
        printf("   setle al\n");
        printf("   movzb rax,al\n");
        break;
    }

    printf("   push rax\n");
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "argc not match\n");
        return 1;
    }

    user_input = argv[1];

    token = tokenize(); //TokList作成

    Node *node = expr();

    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");

    gen(node);

    //genを終えるとスタックトップに最終計算結果が残っているはずなので、それをraxにロードしてretの返り値とする
    printf("   pop rax\n");

    printf("   ret\n");
    return 0;
}