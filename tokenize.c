#include "Mycc.h"

//現在着目しているトークン
Token *token;

char *user_input;

void verror_at(char *loc, char *fmt, va_list ap)
{
    // va_list ap;
    // va_start(ap, fmt);

    int pos = loc - user_input; //アドレスの場所の差をとる,例えばアドレス400がLocで390がuser_inputだったら10が出てくる
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " "); // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

// Reports an error location and exit.
void error_tok(Token *tok, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (tok)
        verror_at(tok->str, fmt, ap);

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

char *strndup(char *p, int len)
{
    char *buf = malloc(len + 1); // \0の分+1
    strncpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

Token *peek(char *s)
{
    //二文字以上の演算子を取れるように改良
    if (token->kind != TK_RESERVED || strlen(s) != token->len || memcmp(token->str, s, token->len))
        return NULL;
    return token;
}
Token *consume(char *op)
{

    if (!peek(op))
        return NULL;
    Token *t = token;
    token = token->next;
    return t;
}

Token *consume_ident()
{
    if (token->kind != TK_IDENT)
        return NULL;
    Token *t = token;
    token = token->next; //次のTokenへ進む
    return t;            //TK_IDENTを返す
}

void expect(char *op)
{
    // 次のトークンが期待している記号のときには、トークンを1つ読み進める。
    // それ以外の場合にはエラーを報告する。
    if (!peek(op))
        error_tok(token, "'%c'ではありません", op);
    token = token->next;
}

int expect_number()
{
    if (token->kind != TK_NUM)
        error_tok(token, "数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

char *expect_ident()
{
    if (token->kind != TK_IDENT)
        error_tok(token, "expected an identifier");
    char *s = strndup(token->str, token->len);
    token = token->next;
    return s;
}

bool at_eof()
{
    return token->kind == TK_EOF;
}

Token *new_token(TokenKind kind, Token *cur, char *str, int len)
{
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind,
    tok->str = str; //*tokはtokのアドレス内には構造体の先頭アドレスが入っていて、tok->strはtokのstrの保有するアドレス、*strには入力された文字の(先頭)アドレスが入っているのでtok->strが指すアドレスを*strが指すアドレスと同じにしてあげる,lenで先頭アドレスからの文字数がわかるので例えばreturnの場合だとstrにはrを指すアドレス、lenは6でこの二つで初めてreturnを表せる
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool startswith(char *p, char *q)
{
    return memcmp(p, q, strlen(q)) == 0;
}

bool is_alpha(char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

//returnxのようなトークンを防ぐため、is_alnumがいる
bool is_alnum(char c)
{
    return is_alpha(c) || ('0' <= c && c <= '9');
}

char *starts_with_reserved(char *p)
{
    //Keyword
    static char *kw[] = {"return", "if", "else", "while", "for", "int", "sizeof"};

    //sizeof(kw)/sizeof(*kw)は配列のlen
    for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    {
        int len = strlen(kw[i]);
        if (startswith(p, kw[i]) && !is_alnum(p[len]))
            return kw[i];
    }

    //MultiLetter pinctuator
    static char *ops[] = {"==", "!=", "<=", ">="};

    for (int i = 0; i < sizeof(ops) / sizeof(*ops); i++)
    {
        if (startswith(p, ops[i]))
            return ops[i];
    }

    return NULL;
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

        //Keyword
        if (startswith(p, "return") && !is_alnum(p[6]))
        {
            cur = new_token(TK_RESERVED, cur, p, 6);
            p += 6;
            continue;
        }

        //Keyword,複数文字に対応させる
        char *kw = starts_with_reserved(p);
        if (kw)
        {
            int len = strlen(kw);
            cur = new_token(TK_RESERVED, cur, p, len);
            p += len;
            continue;
        }

        // Single-letter punctuator
        if (strchr("+-*/()<>;={},&[]", *p))
        {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        //Identifier
        if (is_alpha(*p))
        {
            char *q = p++; //Identifierの最初のアドレスを保存
            while (is_alnum(*p))
            {
                p++;
            }
            cur = new_token(TK_IDENT, cur, q, p - q);
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