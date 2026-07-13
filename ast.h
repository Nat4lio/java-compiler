#ifndef _AST_H
#define _AST_H

enum category {
    ProgramNode, FieldDeclNode, VarDeclNode, MethodDeclNode, MethodHeaderNode, 
    MethodParamsNode, ParamDeclNode, MethodBodyNode, BlockNode, IfNode, WhileNode, 
    ReturnNode, CallNode, PrintNode, ParseArgsNode, AssignNode, OrNode, AndNode, 
    EqNode, NeNode, LtNode, GtNode, LeNode, GeNode, AddNode, SubNode, MulNode, 
    DivNode, ModNode, LshiftNode, RshiftNode, XorNode, NotNode, MinusNode, PlusNode, 
    LengthNode, BoolNode, BoolLitNode, DoubleNode, DecimalNode, IdentifierNode, 
    IntNode, NaturalNode, StrLitNode, StringArrayNode, VoidNode, NullNode
};

struct node {
    enum category category;
    char *token;
    int semantic_type;
    const char *annot; // NOVO CAMPO: Para guardar assinaturas como "(int,int)"
    int line;
    int col;
    struct node_list *children;
};

struct node_list {
    struct node *node;
    struct node_list *next;
};

struct node *newnode(enum category category, char *token, int line, int col);
void addchild(struct node *parent, struct node *child);
void show(struct node *node, int depth);

#endif