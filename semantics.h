#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"

typedef enum {
    TY_INT = 0, TY_BOOL = 1, TY_DOUBLE = 2, TY_STRING_ARRAY = 3, 
    TY_VOID = 4, TY_UNDEF = 5, TY_NONE = 6, TY_STRING = 7
} SymbolType;

typedef struct ParamList {
    SymbolType type;
    struct ParamList *next;
} ParamList;

struct SymbolTable; 

typedef struct Symbol {
    char *name;
    SymbolType type;
    ParamList *param_types;
    int is_param;
    int is_method;
    struct SymbolTable *method_table; 
    struct Symbol *next;
} Symbol;

typedef struct SymbolTable {
    char *name;
    ParamList *method_params;
    Symbol *first;
    struct SymbolTable *next;
} SymbolTable;

void check_program(struct node *program);
void print_symbol_tables();
void annotate_ast(struct node *node, SymbolTable *local_table);
void show_annotated(struct node *node, int depth, struct node *parent);

// --- AS 3 FUNÇÕES QUE FALTAVAM EXPOR ---
SymbolType get_type(enum category cat);
Symbol* search_variable(SymbolTable *table, char *name);
Symbol* search_method(SymbolTable *table, char *name, ParamList *params);

#endif