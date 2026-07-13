%{
/*
 * João Natálio
 * Numero de Estudante: 2023205576
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "semantics.h"

int yylex(void);
void yyerror(const char *s);
extern int syn_line, syn_column;
extern char *error_yytext; 

int flag_l = 0, flag_t = 0, flag_e1 = 0, flag_e2 = 0, flag_s = 0;
int syntax_errors = 0;
struct node *ast = NULL;
%}

%union {
    struct {
        char *str;
        int line;
        int col;
    } token;
    struct node *node;
}

%token <token> IDENTIFIER NATURAL DECIMAL BOOLLIT STRLIT
%token <token> CLASS PUBLIC STATIC VOID STRING INT DOUBLE BOOL
%token <token> IF ELSE WHILE RETURN PRINT PARSEINT DOTLENGTH
%token <token> LBRACE RBRACE LPAR RPAR LSQ RSQ SEMICOLON COMMA
%token <token> ASSIGN OR AND EQ NE LT GT LE GE LSHIFT RSHIFT XOR PLUS MINUS STAR DIV MOD NOT ARROW RESERVED

%right ASSIGN
%left OR
%left AND
%left XOR
%left EQ NE
%left LT LE GT GE
%left LSHIFT RSHIFT
%left PLUS MINUS
%left STAR DIV MOD
%right NOT UNARY_PLUS UNARY_MINUS

%nonassoc THEN
%nonassoc ELSE

%type <node> Program DeclList MethodDecl FieldDecl FieldList Type MethodHeader FormalParams FormalParamsList
%type <node> MethodBody MethodBodyDecls BlockStatements Statement MethodInvocation ExprList
%type <node> ParseArgs Assignment Expr BinaryExpr VarDecl

%%
Program: CLASS IDENTIFIER LBRACE DeclList RBRACE 
{ 
    $$ = newnode(ProgramNode, NULL, $1.line, $1.col); 
    addchild($$, newnode(IdentifierNode, $2.str, $2.line, $2.col));
    if ($4 != NULL) {
        struct node_list *curr = $4->children->next;
        while(curr != NULL) { addchild($$, curr->node); curr = curr->next; }
    }
    ast = $$;
} ;

DeclList: /* vazio */ { $$ = newnode(NullNode, NULL, 0, 0); }
  | DeclList MethodDecl { $$ = $1; addchild($$, $2); }
  | DeclList FieldDecl { $$ = $1; if ($2 != NULL) { struct node_list *curr = $2->children->next; while(curr != NULL) { addchild($$, curr->node); curr = curr->next; } } }
  | DeclList SEMICOLON { $$ = $1; }
  ;

MethodDecl: PUBLIC STATIC MethodHeader MethodBody { $$ = newnode(MethodDeclNode, NULL, $1.line, $1.col); addchild($$, $3); addchild($$, $4); } ;

FieldDecl: PUBLIC STATIC Type FieldList SEMICOLON { 
    $$ = newnode(NullNode, NULL, 0, 0); struct node_list *curr = $4->children->next;
    while(curr != NULL) { 
        struct node *fdecl = newnode(FieldDeclNode, NULL, $2.line, $2.col);
        addchild(fdecl, newnode($3->category, NULL, $3->line, $3->col)); 
        addchild(fdecl, curr->node); addchild($$, fdecl); curr = curr->next; 
    } 
} | error SEMICOLON { $$ = NULL; } ;

FieldList: IDENTIFIER { $$ = newnode(NullNode, NULL, 0, 0); addchild($$, newnode(IdentifierNode, $1.str, $1.line, $1.col)); }
  | FieldList COMMA IDENTIFIER { $$ = $1; addchild($$, newnode(IdentifierNode, $3.str, $3.line, $3.col)); }
  ;

Type: BOOL { $$ = newnode(BoolNode, NULL, $1.line, $1.col); } | INT { $$ = newnode(IntNode, NULL, $1.line, $1.col); } | DOUBLE { $$ = newnode(DoubleNode, NULL, $1.line, $1.col); } ;

MethodHeader: Type IDENTIFIER LPAR RPAR { $$ = newnode(MethodHeaderNode, NULL, $1->line, $1->col); addchild($$, $1); addchild($$, newnode(IdentifierNode, $2.str, $2.line, $2.col)); addchild($$, newnode(MethodParamsNode, NULL, $3.line, $3.col)); }
  | Type IDENTIFIER LPAR FormalParams RPAR { $$ = newnode(MethodHeaderNode, NULL, $1->line, $1->col); addchild($$, $1); addchild($$, newnode(IdentifierNode, $2.str, $2.line, $2.col)); addchild($$, $4); }
  | VOID IDENTIFIER LPAR RPAR { $$ = newnode(MethodHeaderNode, NULL, $1.line, $1.col); addchild($$, newnode(VoidNode, NULL, $1.line, $1.col)); addchild($$, newnode(IdentifierNode, $2.str, $2.line, $2.col)); addchild($$, newnode(MethodParamsNode, NULL, $3.line, $3.col)); }
  | VOID IDENTIFIER LPAR FormalParams RPAR { $$ = newnode(MethodHeaderNode, NULL, $1.line, $1.col); addchild($$, newnode(VoidNode, NULL, $1.line, $1.col)); addchild($$, newnode(IdentifierNode, $2.str, $2.line, $2.col)); addchild($$, $4); }
  ;

FormalParams: FormalParamsList { $$ = $1; }
  | STRING LSQ RSQ IDENTIFIER { 
    $$ = newnode(MethodParamsNode, NULL, $1.line, $1.col); 
    struct node *p = newnode(ParamDeclNode, NULL, $1.line, $1.col); 
    addchild(p, newnode(StringArrayNode, NULL, $1.line, $1.col)); 
    addchild(p, newnode(IdentifierNode, $4.str, $4.line, $4.col)); 
    addchild($$, p); 
} ;

FormalParamsList: Type IDENTIFIER { 
    $$ = newnode(MethodParamsNode, NULL, $1->line, $1->col); 
    struct node *p = newnode(ParamDeclNode, NULL, $1->line, $1->col); 
    addchild(p, $1); addchild(p, newnode(IdentifierNode, $2.str, $2.line, $2.col)); 
    addchild($$, p); 
}
  | FormalParamsList COMMA Type IDENTIFIER { 
    $$ = $1; struct node *p = newnode(ParamDeclNode, NULL, $3->line, $3->col); 
    addchild(p, $3); addchild(p, newnode(IdentifierNode, $4.str, $4.line, $4.col)); 
    addchild($$, p); 
} ;

MethodBody: LBRACE MethodBodyDecls RBRACE { $$ = $2; $$->category = MethodBodyNode; $$->line = $1.line; $$->col = $1.col; } ;

MethodBodyDecls: /* vazio */ { $$ = newnode(MethodBodyNode, NULL, 0, 0); }
  | MethodBodyDecls Statement { $$ = $1; if ($2 != NULL && $2->category != NullNode) addchild($$, $2); }
  | MethodBodyDecls VarDecl { $$ = $1; if ($2 != NULL) { struct node_list *curr = $2->children->next; while(curr != NULL) { addchild($$, curr->node); curr = curr->next; } } }
  ;

VarDecl: Type FieldList SEMICOLON { $$ = newnode(NullNode, NULL, 0, 0); struct node_list *curr = $2->children->next;
    while(curr != NULL) { struct node *vdecl = newnode(VarDeclNode, NULL, $1->line, $1->col); addchild(vdecl, newnode($1->category, NULL, $1->line, $1->col)); addchild(vdecl, curr->node); addchild($$, vdecl); curr = curr->next; } } ;

BlockStatements: /* vazio */ { $$ = newnode(NullNode, NULL, 0, 0); }
  | BlockStatements Statement { 
        if ($1->category == NullNode) {
            if ($2 != NULL && $2->category != NullNode) { $$ = newnode(BlockNode, NULL, $2->line, $2->col); addchild($$, $2); }
            else $$ = newnode(NullNode, NULL, 0, 0);
        } else { $$ = $1; if ($2 != NULL && $2->category != NullNode) addchild($$, $2); }
    } ;

Statement: 
    LBRACE BlockStatements RBRACE {
        int count = 0; struct node_list *curr = $2->children->next;
        while(curr != NULL) { count++; curr = curr->next; }
        if (count == 0) $$ = newnode(NullNode, NULL, 0, 0);
        else if (count == 1) $$ = $2->children->next->node;
        else $$ = $2;
    }
  | IF LPAR Expr RPAR Statement %prec THEN { $$ = newnode(IfNode, NULL, $1.line, $1.col); addchild($$, $3); addchild($$, ($5 == NULL || $5->category == NullNode) ? newnode(BlockNode, NULL, 0, 0) : $5); addchild($$, newnode(BlockNode, NULL, 0, 0)); }
  | IF LPAR Expr RPAR Statement ELSE Statement { $$ = newnode(IfNode, NULL, $1.line, $1.col); addchild($$, $3); addchild($$, ($5 == NULL || $5->category == NullNode) ? newnode(BlockNode, NULL, 0, 0) : $5); addchild($$, ($7 == NULL || $7->category == NullNode) ? newnode(BlockNode, NULL, 0, 0) : $7); }
  | WHILE LPAR Expr RPAR Statement { $$ = newnode(WhileNode, NULL, $1.line, $1.col); addchild($$, $3); addchild($$, ($5 == NULL || $5->category == NullNode) ? newnode(BlockNode, NULL, 0, 0) : $5); }
  | RETURN SEMICOLON { $$ = newnode(ReturnNode, NULL, $1.line, $1.col); }
  | RETURN Expr SEMICOLON { $$ = newnode(ReturnNode, NULL, $1.line, $1.col); addchild($$, $2); }
  | MethodInvocation SEMICOLON { $$ = $1; }
  | Assignment SEMICOLON { $$ = $1; }
  | ParseArgs SEMICOLON { $$ = $1; }
  | SEMICOLON { $$ = newnode(NullNode, NULL, 0, 0); }
  | PRINT LPAR Expr RPAR SEMICOLON { $$ = newnode(PrintNode, NULL, $1.line, $1.col); addchild($$, $3); }
  | PRINT LPAR STRLIT RPAR SEMICOLON { $$ = newnode(PrintNode, NULL, $1.line, $1.col); addchild($$, newnode(StrLitNode, $3.str, $3.line, $3.col)); }
  | error SEMICOLON { $$ = NULL; }
  ;

MethodInvocation: IDENTIFIER LPAR RPAR { $$ = newnode(CallNode, NULL, $1.line, $1.col); addchild($$, newnode(IdentifierNode, $1.str, $1.line, $1.col)); }
  | IDENTIFIER LPAR ExprList RPAR { $$ = newnode(CallNode, NULL, $1.line, $1.col); addchild($$, newnode(IdentifierNode, $1.str, $1.line, $1.col)); struct node_list *curr = $3->children->next; while(curr != NULL) { addchild($$, curr->node); curr = curr->next; } }
  | IDENTIFIER LPAR error RPAR { $$ = NULL; }
  ;

ExprList: Expr { $$ = newnode(NullNode, NULL, 0, 0); addchild($$, $1); } | ExprList COMMA Expr { $$ = $1; addchild($$, $3); } ;

Assignment: IDENTIFIER ASSIGN Expr { $$ = newnode(AssignNode, NULL, $2.line, $2.col); addchild($$, newnode(IdentifierNode, $1.str, $1.line, $1.col)); addchild($$, $3); } ;

ParseArgs: PARSEINT LPAR IDENTIFIER LSQ Expr RSQ RPAR { $$ = newnode(ParseArgsNode, NULL, $1.line, $1.col); addchild($$, newnode(IdentifierNode, $3.str, $3.line, $3.col)); addchild($$, $5); } | PARSEINT LPAR error RPAR { $$ = NULL; } ;

Expr: Assignment { $$ = $1; } | BinaryExpr { $$ = $1; } ;

BinaryExpr: MethodInvocation { $$ = $1; } | ParseArgs { $$ = $1; }
  | BinaryExpr PLUS BinaryExpr { $$ = newnode(AddNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr MINUS BinaryExpr { $$ = newnode(SubNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr STAR BinaryExpr { $$ = newnode(MulNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr DIV BinaryExpr { $$ = newnode(DivNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr MOD BinaryExpr { $$ = newnode(ModNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr AND BinaryExpr { $$ = newnode(AndNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr OR BinaryExpr { $$ = newnode(OrNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr XOR BinaryExpr { $$ = newnode(XorNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr LSHIFT BinaryExpr { $$ = newnode(LshiftNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr RSHIFT BinaryExpr { $$ = newnode(RshiftNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr EQ BinaryExpr { $$ = newnode(EqNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr NE BinaryExpr { $$ = newnode(NeNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr LT BinaryExpr { $$ = newnode(LtNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr GT BinaryExpr { $$ = newnode(GtNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr LE BinaryExpr { $$ = newnode(LeNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | BinaryExpr GE BinaryExpr { $$ = newnode(GeNode, NULL, $2.line, $2.col); addchild($$, $1); addchild($$, $3); }
  | MINUS BinaryExpr %prec UNARY_MINUS { $$ = newnode(MinusNode, NULL, $1.line, $1.col); addchild($$, $2); }
  | PLUS BinaryExpr %prec UNARY_PLUS { $$ = newnode(PlusNode, NULL, $1.line, $1.col); addchild($$, $2); }
  | NOT BinaryExpr { $$ = newnode(NotNode, NULL, $1.line, $1.col); addchild($$, $2); }
  | IDENTIFIER { $$ = newnode(IdentifierNode, $1.str, $1.line, $1.col); }
  | IDENTIFIER DOTLENGTH { $$ = newnode(LengthNode, NULL, $2.line, $2.col); addchild($$, newnode(IdentifierNode, $1.str, $1.line, $1.col)); }
  | LPAR Expr RPAR { $$ = $2; }
  | LPAR error RPAR { $$ = NULL; }
  | NATURAL { $$ = newnode(NaturalNode, $1.str, $1.line, $1.col); }
  | DECIMAL { $$ = newnode(DecimalNode, $1.str, $1.line, $1.col); }
  | BOOLLIT { $$ = newnode(BoolLitNode, $1.str, $1.line, $1.col); }
  ;
%%

void yyerror(const char *s) {
    syntax_errors = 1;
    if (flag_l || flag_e1) return;
    printf("Line %d, col %d: %s: %s\n", syn_line, syn_column, s, error_yytext);
}

extern int semantic_errors;
void generate_code(struct node *program);

int main(int argc, char **argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "-l") == 0) flag_l = 1;
        else if (strcmp(argv[1], "-t") == 0) flag_t = 1;
        else if (strcmp(argv[1], "-e1") == 0) flag_e1 = 1;
        else if (strcmp(argv[1], "-e2") == 0) flag_e2 = 1;
        else if (strcmp(argv[1], "-s") == 0) flag_s = 1; 
    }
    if (flag_l || flag_e1) { while (yylex() != 0); return 0; }
    
    yyparse();
    
    if (syntax_errors == 0 && ast != NULL) {
        if (flag_t) {
            show(ast, 0); 
        } else {
            check_program(ast);
            if (flag_s) {
                print_symbol_tables();
                show_annotated(ast, 0, NULL);
            } else if (argc == 1 && semantic_errors == 0) {
                generate_code(ast);
            }
        }
    }
    return 0;
}