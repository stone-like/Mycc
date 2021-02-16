#include "Mycc.h"

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

        //複数文字に対応させる

        if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">="))
        {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2; //二つ分進めてあげる
            continue;
        }

        // Single-letter punctuator
        if (strchr("+-*/()<>;", *p))
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