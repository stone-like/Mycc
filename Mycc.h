#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

//
// tokenize.c
//

typedef enum
{
    TK_RESERVED, // Keywords or punctuators
    TK_IDENT,    //Identifiers
    TK_STR,      //String literals
    TK_NUM,      // Integer literals
    TK_EOF,      // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;
struct Token
{
    TokenKind kind; // Token kind
    Token *next;    // Next token
    int val;        // If kind is TK_NUM, its value
    char *str;      // Token string
    int len;        // Token length

    char *contents; //String literal contents including \0
    char count_len; //string literal length
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *peek(char *s);
Token *consume(char *op);
char *strndup(char *p, int len);
Token *consume_ident();
char *expect_ident();
void expect(char *op);
int expect_number();
bool at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
Token *tokenize();

extern char *user_input;
extern Token *token;

//
// parse.c
//

//Variable
typedef struct Var Var;
struct Var
{
    char *name;
    Type *ty;
    bool is_local; //local or global

    //Used For Local Variable
    int offset; //Offset from RBP

    //Used For Global Variable
    char *contents;
    int count_len;
};

typedef struct VarList VarList;
struct VarList
{
    VarList *next;
    Var *var;
};

//AST Node
typedef enum
{
    ND_ADD,       // +
    ND_SUB,       // -
    ND_MUL,       // *
    ND_DIV,       // /
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ASSIGN,    // =
    ND_ADDR,      // unary &
    ND_DEREF,     // unary *
    ND_RETURN,    //Return
    ND_IF,        // "if"
    ND_WHILE,     // "while"
    ND_FOR,       // "for"
    ND_SIZEOF,    // "sizeof"
    ND_BLOCK,     // {...}
    ND_FUNCALL,   //Function Call
    ND_EXPR_STMT, // Expression statement
    ND_VAR,       // variable
    ND_NUM,       // Integer
    ND_NULL,      //EmptyStatement
} NodeKind;

// AST node type
typedef struct Node Node;
struct Node
{
    NodeKind kind; // Node kind
    Node *next;    //stmt単位、;で区切られて次のstmtへ、chibiccは一つの構造体で、複数のことをやってる
                   //Node ->  Node  ->とStatement単位に連なっていくNodeと
                   //  |
                   //LeftNode RightNodeとNestするNodeになっている

    Type *ty;   //Type, e.g. int or pointer to int
    Token *tok; //Representitive token

    Node *lhs; // Left-hand side
    Node *rhs; // Right-hand side

    // "if" or "while","for" statement
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    // Block
    Node *body;

    //Function Call
    char *funcname;
    Node *args;

    Var *var; //Used if kind == ND_VAR
    int val;  // Used if kind == ND_NUM
};

typedef struct Function Function;

struct Function
{
    Function *next;
    char *name;
    VarList *params;
    Node *node;
    VarList *locals;
    int stack_size;
};

typedef struct
{
    VarList *globals;
    Function *fns;
} Program;

Program *program();

// type.c

typedef enum
{
    TY_CHAR,
    TY_INT,
    TY_PTR,
    TY_ARRAY
} TypeKind;

struct Type
{
    TypeKind kind;
    Type *base;
    int array_size;
};

Type *char_type();
Type *int_type();
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
int size_of(Type *ty);

void add_type(Program *prog);

//
// codegen.c
//

void codegen(Program *prog);