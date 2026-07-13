#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

struct node *newnode(enum category category, char *token, int line, int col) {
    struct node *new = malloc(sizeof(struct node));
    new->category = category;
    new->token = token;
    new->semantic_type = 6;
    new->annot = NULL;
    new->line = line;
    new->col = col;
    new->children = malloc(sizeof(struct node_list));
    new->children->node = NULL;
    new->children->next = NULL;
    return new;
}

void addchild(struct node *parent, struct node *child) {
    if(child == NULL) return;
    struct node_list *new = malloc(sizeof(struct node_list));
    new->node = child;
    new->next = NULL;
    struct node_list *children = parent->children;
    while(children->next != NULL) children = children->next;
    children->next = new;
}

const char *category_name[] = {
    "Program", "FieldDecl", "VarDecl", "MethodDecl", "MethodHeader", 
    "MethodParams", "ParamDecl", "MethodBody", "Block", "If", "While", 
    "Return", "Call", "Print", "ParseArgs", "Assign", "Or", "And", 
    "Eq", "Ne", "Lt", "Gt", "Le", "Ge", "Add", "Sub", "Mul", 
    "Div", "Mod", "Lshift", "Rshift", "Xor", "Not", "Minus", "Plus", 
    "Length", "Bool", "BoolLit", "Double", "Decimal", "Identifier", 
    "Int", "Natural", "StrLit", "StringArray", "Void", "Null"
};

void show(struct node *node, int depth) {
    if (node == NULL || node->category == NullNode) return;
    for (int i = 0; i < depth; i++) printf("..");
    if (node->token != NULL) printf("%s(%s)\n", category_name[node->category], node->token);
    else printf("%s\n", category_name[node->category]);

    struct node_list *child = node->children->next;
    while (child != NULL) {
        show(child->node, depth + 1);
        child = child->next;
    }
}