#include "Mycc.h"

char *filename;
//現在着目しているトークン
Token *token;

char *user_input;

// Reports an error message in the following format and exit
//
// foo.c:10: x = y+1;
//               ^ <error message here>

void verror_at(char *loc, char *fmt, va_list ap)
{

    //Find a line containing "loc".
    char *line = loc;
    while (user_input < line && line[-1] != '\n')
        line--;

    char *end = loc;
    while (*end != '\n')
        end++;

    //Get a line number.
    int line_num = 1;
    for (char *p = user_input; p < line; p++)
    {
        if (*p == '\n')
            line_num++;
    }

    //Print out the line.
    int indent = fprintf(stderr, "%s:%d", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    //Show the error message.
    int pos = loc - line + indent;
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

long expect_number()
{
    if (token->kind != TK_NUM)
        error_tok(token, "数ではありません");
    long val = token->val;
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
    static char *kw[] = {"return", "if", "else", "while", "for", "int", "sizeof", "char", "struct", "typedef", "short", "long", "void", "_Bool"};

    //sizeof(kw)/sizeof(*kw)は配列のlen
    for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    {
        int len = strlen(kw[i]);
        if (startswith(p, kw[i]) && !is_alnum(p[len]))
            return kw[i];
    }

    //MultiLetter pinctuator
    static char *ops[] = {"==", "!=", "<=", ">=", "->"};

    for (int i = 0; i < sizeof(ops) / sizeof(*ops); i++)
    {
        if (startswith(p, ops[i]))
            return ops[i];
    }

    return NULL;
}

char get_escape_char(char c)
{
    switch (c)
    {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'r':
        return '\r';
    case 'e':
        return 27;
    case '0':
        return 0;
    default:
        return c;
    }
}

//charLiteralは数字扱いっぽい
Token *read_char_literal(Token *cur, char *start)
{
    char *p = start + 1;
    if (*p == '\0')
        error_at(start, "unclosed char literal");

    char c;
    if (*p == '\\')
    {
        p++;
        c = get_escape_char(*p++);
    }
    else
    {
        c = *p++;
    }

    if (*p != '\'')
        error_at(start, "char literal too long");
    p++;

    Token *tok = new_token(TK_NUM, cur, start, p - start);
    tok->val = c;
    return tok;
}

Token *read_string_literal(Token *cur, char *start)
{
    char *p = start + 1;
    char buf[1024];
    int len = 0;

    for (;;)
    {
        if (len == sizeof(buf))
            error_at(start, "string literal too large");
        if (*p == '\0')
            error_at(start, "unclod string literal");
        if (*p == '"')
            break;

        if (*p == '\\')
        {
            p++;
            buf[len++] = get_escape_char(*p++);
        }
        else
        {
            buf[len++] = *p++;
        }
    }

    Token *tok = new_token(TK_STR, cur, start, p - start + 1);
    tok->contents = malloc(len + 1);
    memcpy(tok->contents, buf, len);
    tok->contents[len] = '\0';
    tok->count_len = len + 1; //\0の分+1
    return tok;
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

        //Skip line comments.
        if (startswith(p, "//"))
        {
            p += 2;
            while (*p != '\n')
            {
                p++;
            }

            continue;
        }

        //Skip block comments.
        if (startswith(p, "/*"))
        {
            char *q = strstr(p + 2, "*/");
            if (!q)
            {
                error_at(p, "unclosed block comment");
            }
            p = q + 2; //*/分飛ばす
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
        if (strchr("+-*/()<>;={},&[].", *p))
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

        //String Literal
        if (*p == '"')
        {
            cur = read_string_literal(cur, p);
            p += cur->len;
            continue;
        }

        // Character literal
        if (*p == '\'')
        {
            cur = read_char_literal(cur, p);
            p += cur->len;
            continue;
        }

        //Integer Literal
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