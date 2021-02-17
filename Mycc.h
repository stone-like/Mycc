#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// tokenize.c
//

typedef enum
{
    TK_RESERVED, // Keywords or punctuators
    TK_IDENT,    //Identifiers
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
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
bool consume(char *op);
char *strndup(char *p, int len);
Token *consume_ident();
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

//Local Variable
typedef struct Var Var;
struct Var
{
    Var *next;
    char *name;
    int offset; //Offset from RBP
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
    ND_RETURN,    //Return
    ND_IF,        // "if"
    ND_WHILE,     // "while"
    ND_FOR,       // "for"
    ND_BLOCK,     // {...}
    ND_FUNCALL,   //Function Call
    ND_EXPR_STMT, // Expression statement
    ND_VAR,       // variable
    ND_NUM,       // Integer
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

typedef struct
{
    Node *node;
    Var *locals;
    int stack_size;
} Program;

Program *program();

//
// codegen.c
//

void codegen(Program *prog);