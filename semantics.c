#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantics.h"

SymbolTable *global_table = NULL;
SymbolTable *tables_tail = NULL;
extern const char *category_name[];
int semantic_errors = 0;

// --- TIPOS E STRINGS ---
SymbolType get_type(enum category cat) {
    switch(cat) {
        case IntNode: return TY_INT; case BoolNode: return TY_BOOL;
        case DoubleNode: return TY_DOUBLE; case StringArrayNode: return TY_STRING_ARRAY;
        case VoidNode: return TY_VOID; default: return TY_UNDEF;
    }
}

const char* type_to_string(SymbolType type) {
    switch(type) {
        case TY_INT: return "int";
        case TY_BOOL: return "boolean";
        case TY_DOUBLE: return "double";
        case TY_STRING_ARRAY: return "String[]";
        case TY_VOID: return "void";
        case TY_STRING: return "String";
        case TY_UNDEF: return "undef";
        default: return "";
    }
}

const char* get_op_str(enum category cat) {
    switch(cat) {
        case AssignNode: return "="; case AddNode: return "+"; case SubNode: return "-";
        case MulNode: return "*"; case DivNode: return "/"; case ModNode: return "%";
        case EqNode: return "=="; case NeNode: return "!="; case LtNode: return "<";
        case GtNode: return ">"; case LeNode: return "<="; case GeNode: return ">=";
        case AndNode: return "&&"; case OrNode: return "||"; case XorNode: return "^";
        case LshiftNode: return "<<"; case RshiftNode: return ">>"; default: return "";
    }
}

// --- VERIFICAÇÃO DE LIMITES ---
int is_out_of_bounds_natural(const char *token) {
    char clean[2048]; int j = 0;
    for (int i = 0; token[i] != '\0'; i++) if (token[i] != '_') clean[j++] = token[i];
    clean[j] = '\0';
    if (strtoull(clean, NULL, 10) > 2147483647ULL) return 1;
    return 0;
}

int is_out_of_bounds_decimal(const char *token) {
    char clean[2048]; int j = 0, has_nonzero = 0, in_exponent = 0;
    for (int i = 0; token[i] != '\0'; i++) {
        if (token[i] != '_') clean[j++] = token[i];
        if (token[i] == 'e' || token[i] == 'E') in_exponent = 1;
        if (!in_exponent && token[i] >= '1' && token[i] <= '9') has_nonzero = 1;
    }
    clean[j] = '\0';
    double val = strtod(clean, NULL);
    if (val > 1.7976931348623157e+308) return 1; 
    if (val == 0.0 && has_nonzero) return 1; 
    return 0;
}

// --- TABELAS DE SÍMBOLOS ---
SymbolTable* create_table(char *name, ParamList *params, int append) {
    SymbolTable *new_table = malloc(sizeof(SymbolTable));
    new_table->name = strdup(name); new_table->method_params = params;
    new_table->first = NULL; new_table->next = NULL;
    if (append) {
        if (!global_table) { global_table = new_table; tables_tail = new_table; } 
        else { tables_tail->next = new_table; tables_tail = new_table; }
    }
    return new_table;
}

void insert_symbol(SymbolTable *table, char *name, SymbolType type, ParamList *param_types, int is_param, int is_method) {
    Symbol *new_sym = malloc(sizeof(Symbol));
    new_sym->name = strdup(name); new_sym->type = type; new_sym->param_types = param_types;
    new_sym->is_param = is_param; new_sym->is_method = is_method; 
    new_sym->method_table = NULL; new_sym->next = NULL;
    if (!table->first) table->first = new_sym;
    else { Symbol *curr = table->first; while (curr->next) curr = curr->next; curr->next = new_sym; }
}

Symbol* search_variable(SymbolTable *table, char *name) {
    if (!table) return NULL;
    Symbol *curr = table->first;
    while (curr) { if (!curr->is_method && strcmp(curr->name, name) == 0) return curr; curr = curr->next; }
    return NULL;
}

Symbol* search_method(SymbolTable *table, char *name, ParamList *params) {
    if (!table) return NULL;
    Symbol *curr = table->first;
    while (curr) {
        if (curr->is_method && strcmp(curr->name, name) == 0) {
            int exact = 1;
            ParamList *p1 = curr->param_types, *p2 = params;
            while (p1 && p2) { if (p1->type != p2->type) { exact = 0; break; } p1 = p1->next; p2 = p2->next; }
            if (p1 || p2) exact = 0;
            if (exact) return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

// --- ANOTAÇÃO DA AST E VERIFICAÇÃO DE TIPOS ---
void annotate_ast(struct node *node, SymbolTable *local_table) {
    if (!node || node->category == NullNode) return;

    if (node->category == VarDeclNode) {
        struct node *type_node = node->children->next->node;
        struct node *id_node = node->children->next->next->node;
        SymbolType t = get_type(type_node->category);
        
        // Verificação do underscore reservado para locais
        if (strcmp(id_node->token, "_") == 0) {
            printf("Line %d, col %d: Symbol _ is reserved\n", id_node->line, id_node->col);
            semantic_errors++;
        } else if (search_variable(local_table, id_node->token)) {
            printf("Line %d, col %d: Symbol %s already defined\n", id_node->line, id_node->col, id_node->token);
            semantic_errors++;
        } else {
            insert_symbol(local_table, id_node->token, t, NULL, 0, 0);
        }
        return; 
    }

    struct node_list *child = node->children->next;
    int is_first = 1;
    while (child) {
        if (node->category == CallNode && is_first) {} 
        else annotate_ast(child->node, local_table);
        is_first = 0; child = child->next;
    }

    switch (node->category) {
        case NaturalNode: {
            if (is_out_of_bounds_natural(node->token)) {
                printf("Line %d, col %d: Number %s out of bounds\n", node->line, node->col, node->token);
                semantic_errors++;
            }
            node->semantic_type = TY_INT; break;
        }
        case DecimalNode: {
            if (is_out_of_bounds_decimal(node->token)) {
                printf("Line %d, col %d: Number %s out of bounds\n", node->line, node->col, node->token);
                semantic_errors++;
            }
            node->semantic_type = TY_DOUBLE; break;
        }
        case BoolLitNode: node->semantic_type = TY_BOOL; break;

        case StrLitNode: node->semantic_type = TY_STRING; break;
        
        case ParseArgsNode: {
            struct node *id_node = node->children->next->node;
            struct node *expr_node = node->children->next->next->node;
            if (id_node->semantic_type != TY_STRING_ARRAY || expr_node->semantic_type != TY_INT) {
                printf("Line %d, col %d: Operator Integer.parseInt cannot be applied to types %s, %s\n", node->line, node->col, type_to_string(id_node->semantic_type), type_to_string(expr_node->semantic_type));
                semantic_errors++;
            } 
            node->semantic_type = TY_INT; 
            break;
        }
        case LengthNode: {
            struct node *child_node = node->children->next->node;
            if (child_node->semantic_type != TY_STRING_ARRAY) {
                printf("Line %d, col %d: Operator .length cannot be applied to type %s\n", node->line, node->col, type_to_string(child_node->semantic_type));
                semantic_errors++;
            }
            node->semantic_type = TY_INT; 
            break;
        }
        case IdentifierNode: {
            Symbol *sym = search_variable(local_table, node->token);
            if (!sym) sym = search_variable(global_table, node->token);
            if (sym) node->semantic_type = sym->type;
            else {
                printf("Line %d, col %d: Cannot find symbol %s\n", node->line, node->col, node->token);
                semantic_errors++;
                node->semantic_type = TY_UNDEF;
            }
            break;
        }
        case CallNode: {
            struct node *id_node = node->children->next->node;
            ParamList *args = NULL, *args_tail = NULL;
            struct node_list *c = node->children->next->next;
            
            // 1. Construir a lista de tipos dos argumentos passados
            while (c) {
                ParamList *p = malloc(sizeof(ParamList)); 
                p->type = c->node->semantic_type; 
                p->next = NULL;
                if (!args) args = args_tail = p; 
                else { args_tail->next = p; args_tail = p; }
                c = c->next;
            }

            Symbol *match = NULL, *compat_match = NULL;
            int exact = 0, compat = 0;
            Symbol *curr = global_table->first;
            
            // 2. Procurar métodos compatíveis
            while (curr) {
                if (curr->is_method && strcmp(curr->name, id_node->token) == 0) {
                    int is_exact = 1, is_compat = 1;
                    ParamList *p1 = curr->param_types, *p2 = args;
                    while (p1 && p2) {
                        if (p1->type != p2->type) {
                            is_exact = 0;
                            if (!(p1->type == TY_DOUBLE && p2->type == TY_INT)) is_compat = 0;
                        }
                        p1 = p1->next; p2 = p2->next;
                    }
                    if (p1 || p2) { is_exact = 0; is_compat = 0; }
                    
                    if (is_exact) { match = curr; exact++; }
                    else if (is_compat) { compat_match = curr; compat++; }
                }
                curr = curr->next;
            }

            // 3. Decidir o resultado e formatar a mensagem de erro
            Symbol *final = (exact > 0) ? match : (compat == 1 ? compat_match : NULL);
            
            if (!final) {
                // Criar a assinatura para o erro: nome(tipo1,tipo2,...)
                char buf[1024]; 
                strcpy(buf, id_node->token); 
                strcat(buf, "(");
                ParamList *t = args;
                while(t) { 
                    strcat(buf, type_to_string(t->type)); 
                    if(t->next) strcat(buf, ","); 
                    t = t->next; 
                }
                strcat(buf, ")");

                if (compat > 1) {
                    printf("Line %d, col %d: Reference to method %s is ambiguous\n", id_node->line, id_node->col, buf);
                    semantic_errors++;
                } else {
                    printf("Line %d, col %d: Cannot find symbol %s\n", id_node->line, id_node->col, buf);
                    semantic_errors++;
                }
                node->semantic_type = TY_UNDEF; 
                id_node->semantic_type = TY_UNDEF;
            } else {
                // Caso de sucesso (anotação normal)
                node->semantic_type = final->type;
                char buf[1024]; strcpy(buf, "(");
                ParamList *t = final->param_types;
                while(t) { 
                    strcat(buf, type_to_string(t->type)); 
                    if(t->next) strcat(buf, ","); 
                    t = t->next; 
                }
                strcat(buf, ")");
                id_node->annot = strdup(buf);
                id_node->semantic_type = TY_NONE;
            }
            break;
        }
        case PrintNode: {
            struct node *child_expr = node->children->next->node;
            SymbolType ct = child_expr->semantic_type;
            if (ct == TY_UNDEF || ct == TY_VOID || ct == TY_STRING_ARRAY) {
                printf("Line %d, col %d: Incompatible type %s in System.out.print statement\n", child_expr->line, child_expr->col, type_to_string(ct));
                semantic_errors++;
            }
            break;
        }
        case IfNode: case WhileNode: {
            struct node *child_expr = node->children->next->node;
            SymbolType ct = child_expr->semantic_type;
            if (ct != TY_BOOL) { 
                const char *stmt = (node->category == IfNode) ? "if" : "while";
                printf("Line %d, col %d: Incompatible type %s in %s statement\n", child_expr->line, child_expr->col, type_to_string(ct), stmt);
                semantic_errors++;
            }
            break;
        }
        case ReturnNode: {
            Symbol *ret_sym = search_variable(local_table, "return");
            struct node *ret_expr = node->children->next ? node->children->next->node : NULL;
            if (!ret_expr) {
                if (ret_sym && ret_sym->type != TY_VOID) {
                    printf("Line %d, col %d: Incompatible type void in return statement\n", node->line, node->col);
                    semantic_errors++;
                }
            } else {
                SymbolType rt = ret_expr->semantic_type;
                if (ret_sym) {
                    if (rt == TY_UNDEF) {
                        printf("Line %d, col %d: Incompatible type undef in return statement\n", ret_expr->line, ret_expr->col);
                        semantic_errors++;
                    }
                    else if (ret_sym->type == TY_VOID || (ret_sym->type != rt && !(ret_sym->type == TY_DOUBLE && rt == TY_INT))) {
                        printf("Line %d, col %d: Incompatible type %s in return statement\n", ret_expr->line, ret_expr->col, type_to_string(rt));
                        semantic_errors++;
                    }
                }
            }
            break;
        }
        case AssignNode: {
            struct node *left = node->children->next->node;
            struct node *right = node->children->next->next->node;
            SymbolType lt = left->semantic_type, rt = right->semantic_type;
            if (lt == TY_UNDEF || rt == TY_UNDEF || lt == TY_VOID || rt == TY_VOID ||
                (lt == TY_INT && rt == TY_DOUBLE) || (lt == TY_BOOL && rt != TY_BOOL) ||
                (lt != TY_BOOL && rt == TY_BOOL) || lt == TY_STRING_ARRAY || rt == TY_STRING_ARRAY) {
                printf("Line %d, col %d: Operator = cannot be applied to types %s, %s\n", node->line, node->col, type_to_string(lt), type_to_string(rt));
                semantic_errors++;
            }
            node->semantic_type = lt; break;
        }
        case AddNode: case SubNode: case MulNode: case DivNode: case ModNode: {
            struct node *left = node->children->next->node;
            struct node *right = node->children->next->next->node;
            SymbolType lt = left->semantic_type, rt = right->semantic_type;
            if (lt == TY_BOOL || rt == TY_BOOL || lt == TY_STRING_ARRAY || rt == TY_STRING_ARRAY || lt == TY_VOID || rt == TY_VOID || lt == TY_UNDEF || rt == TY_UNDEF) {
                printf("Line %d, col %d: Operator %s cannot be applied to types %s, %s\n", node->line, node->col, get_op_str(node->category), type_to_string(lt), type_to_string(rt));
                semantic_errors++;
                node->semantic_type = TY_UNDEF;
            } else if (lt == TY_DOUBLE || rt == TY_DOUBLE) node->semantic_type = TY_DOUBLE;
            else node->semantic_type = TY_INT;
            break;
        }
        case EqNode: case NeNode: case LtNode: case GtNode: case LeNode: case GeNode: {
            struct node *left = node->children->next->node;
            struct node *right = node->children->next->next->node;
            SymbolType lt = left->semantic_type, rt = right->semantic_type;
            if (node->category == EqNode || node->category == NeNode) {
                if (lt == TY_UNDEF || rt == TY_UNDEF || lt == TY_STRING_ARRAY || rt == TY_STRING_ARRAY || lt == TY_VOID || rt == TY_VOID || (lt == TY_BOOL && rt != TY_BOOL) || (lt != TY_BOOL && rt == TY_BOOL)) {
                    printf("Line %d, col %d: Operator %s cannot be applied to types %s, %s\n", node->line, node->col, get_op_str(node->category), type_to_string(lt), type_to_string(rt));
                    semantic_errors++;
                }
            } else {
                if (lt == TY_BOOL || rt == TY_BOOL || lt == TY_STRING_ARRAY || rt == TY_STRING_ARRAY || lt == TY_VOID || rt == TY_VOID || lt == TY_UNDEF || rt == TY_UNDEF) {
                    printf("Line %d, col %d: Operator %s cannot be applied to types %s, %s\n", node->line, node->col, get_op_str(node->category), type_to_string(lt), type_to_string(rt));
                    semantic_errors++;
                }
            }
            node->semantic_type = TY_BOOL; break;
        }
        case AndNode: case OrNode: {
            struct node *left = node->children->next->node;
            struct node *right = node->children->next->next->node;
            SymbolType lt = left->semantic_type, rt = right->semantic_type;
            if (lt != TY_BOOL || rt != TY_BOOL) {
                printf("Line %d, col %d: Operator %s cannot be applied to types %s, %s\n", node->line, node->col, get_op_str(node->category), type_to_string(lt), type_to_string(rt));
                semantic_errors++;
            }
            node->semantic_type = TY_BOOL; 
            break;
        }
        case XorNode: case LshiftNode: case RshiftNode: {
            struct node *left = node->children->next->node;
            struct node *right = node->children->next->next->node;
            SymbolType lt = left->semantic_type, rt = right->semantic_type;
            if (lt != TY_INT || rt != TY_INT) {
                printf("Line %d, col %d: Operator %s cannot be applied to types %s, %s\n", node->line, node->col, get_op_str(node->category), type_to_string(lt), type_to_string(rt));
                semantic_errors++;
            }
            node->semantic_type = TY_INT; 
            break;
        }
        case NotNode: {
            struct node *child = node->children->next->node;
            if (child->semantic_type != TY_BOOL) {
                printf("Line %d, col %d: Operator ! cannot be applied to type %s\n", node->line, node->col, type_to_string(child->semantic_type));
                semantic_errors++;
            }
            node->semantic_type = TY_BOOL; 
            break;
        }
        case MinusNode: case PlusNode: {
            struct node *child = node->children->next->node;
            SymbolType ct = child->semantic_type;
            const char *op = (node->category == MinusNode) ? "-" : "+";
            if (ct == TY_BOOL || ct == TY_STRING_ARRAY || ct == TY_VOID || ct == TY_UNDEF) {
                printf("Line %d, col %d: Operator %s cannot be applied to type %s\n", node->line, node->col, op, type_to_string(ct));
                semantic_errors++;
                node->semantic_type = TY_UNDEF;
            } else node->semantic_type = ct; 
            break;
        }
        default: node->semantic_type = TY_NONE; break;
    }
}

// --- CONSTRUÇÃO DE TABELAS ---
void check_program(struct node *program) {
    if (!program || program->category != ProgramNode) return;
    struct node *id_node = program->children->next->node;
    global_table = create_table(id_node->token, NULL, 1);

    // --- 1ª PASSAGEM: Variáveis Globais e Cabeçalhos de Métodos ---
    struct node_list *curr_decl = program->children->next->next;
    while (curr_decl) {
        if (curr_decl->node->category == FieldDeclNode) {
            struct node *type_node = curr_decl->node->children->next->node;
            struct node *field_id = curr_decl->node->children->next->next->node;
            
            if (strcmp(field_id->token, "_") == 0) {
                printf("Line %d, col %d: Symbol _ is reserved\n", field_id->line, field_id->col);
                semantic_errors++;
            } else if (search_variable(global_table, field_id->token)) {
                printf("Line %d, col %d: Symbol %s already defined\n", field_id->line, field_id->col, field_id->token);
                semantic_errors++;
            } else {
                insert_symbol(global_table, field_id->token, get_type(type_node->category), NULL, 0, 0);
            }
        } else if (curr_decl->node->category == MethodDeclNode) {
            struct node *header = curr_decl->node->children->next->node;
            struct node *ret_type = header->children->next->node;
            struct node *method_id = header->children->next->next->node;
            struct node *params = header->children->next->next->next->node;
            
            ParamList *p_head = NULL, *p_tail = NULL;
            struct node_list *p = params->children->next;
            
            // Usamos uma tabela temporária só para detetar parâmetros duplicados na 1ª passagem
            SymbolTable *temp_param_table = create_table("temp", NULL, 0);

            while (p) {
                struct node *p_type = p->node->children->next->node;
                struct node *p_id = p->node->children->next->next->node;
                
                // IMPRIME OS ERROS DOS PARÂMETROS AQUI NA 1ª PASSAGEM!
                if (strcmp(p_id->token, "_") == 0) {
                    printf("Line %d, col %d: Symbol _ is reserved\n", p_id->line, p_id->col);
                    semantic_errors++;
                } else if (search_variable(temp_param_table, p_id->token)) {
                    printf("Line %d, col %d: Symbol %s already defined\n", p_id->line, p_id->col, p_id->token);
                    semantic_errors++;
                } else {
                    insert_symbol(temp_param_table, p_id->token, get_type(p_type->category), NULL, 1, 0);
                }

                ParamList *new_p = malloc(sizeof(ParamList));
                new_p->type = get_type(p_type->category);
                new_p->next = NULL;
                if (!p_head) { p_head = new_p; p_tail = new_p; } else { p_tail->next = new_p; p_tail = new_p; }
                p = p->next;
            }

            int duplicate = 0;
            Symbol *curr = global_table->first;
            while (curr) {
                if (curr->is_method && strcmp(curr->name, method_id->token) == 0) {
                    ParamList *p1 = curr->param_types, *p2 = p_head;
                    int exact = 1;
                    while (p1 && p2) { if (p1->type != p2->type) { exact = 0; break; } p1 = p1->next; p2 = p2->next; }
                    if (p1 || p2) exact = 0;
                    if (exact) { duplicate = 1; break; }
                }
                curr = curr->next;
            }

            if (strcmp(method_id->token, "_") == 0) {
                printf("Line %d, col %d: Symbol _ is reserved\n", method_id->line, method_id->col);
                semantic_errors++;
            } else if (duplicate) {
                char buf[1024]; strcpy(buf, method_id->token); strcat(buf, "(");
                ParamList *t = p_head;
                while(t) { strcat(buf, type_to_string(t->type)); if(t->next) strcat(buf, ","); t=t->next; }
                strcat(buf, ")");
                printf("Line %d, col %d: Symbol %s already defined\n", method_id->line, method_id->col, buf);
                semantic_errors++;
            } else {
                insert_symbol(global_table, method_id->token, get_type(ret_type->category), p_head, 0, 1);
                Symbol *sym = search_method(global_table, method_id->token, p_head);
                if (sym) sym->method_table = NULL; 
            }
        }
        curr_decl = curr_decl->next;
    }

    // --- 2ª PASSAGEM: Interior dos Métodos ---
    curr_decl = program->children->next->next;
    while (curr_decl) {
        if (curr_decl->node->category == MethodDeclNode) {
            struct node *header = curr_decl->node->children->next->node;
            struct node *method_id = header->children->next->next->node;
            
            ParamList *p_head = NULL, *p_tail = NULL;
            struct node_list *p = header->children->next->next->next->node->children->next;
            while (p) {
                ParamList *new_p = malloc(sizeof(ParamList));
                new_p->type = get_type(p->node->children->next->node->category);
                new_p->next = NULL;
                if (!p_head) { p_head = new_p; p_tail = new_p; } else { p_tail->next = new_p; p_tail = new_p; }
                p = p->next;
            }

            Symbol *sym = search_method(global_table, method_id->token, p_head);
            if (sym && sym->method_table == NULL) { 
                SymbolTable *local = create_table(method_id->token, p_head, 1);
                sym->method_table = local;

                insert_symbol(local, "return", get_type(header->children->next->node->category), NULL, 0, 0);

                p = header->children->next->next->next->node->children->next;
                while (p) {
                    struct node *p_type = p->node->children->next->node;
                    struct node *p_id = p->node->children->next->next->node;
                    
                    // Na 2ª passagem APENAS inserimos os válidos (os erros já foram impressos na 1ª passagem!)
                    if (strcmp(p_id->token, "_") != 0 && !search_variable(local, p_id->token)) {
                        insert_symbol(local, p_id->token, get_type(p_type->category), NULL, 1, 0);
                    }
                    p = p->next;
                }
                annotate_ast(curr_decl->node->children->next->next->node, local);
            }
        }
        curr_decl = curr_decl->next;
    }
}

// --- IMPRESSÕES FINAIS ---
void print_param_list(ParamList *params) {
    while (params) { printf("%s", type_to_string(params->type)); if (params->next) printf(","); params = params->next; }
}

void print_symbol_tables() {
    SymbolTable *curr_table = global_table;
    while (curr_table) {
        if (curr_table == global_table) printf("===== Class %s Symbol Table =====\n", curr_table->name);
        else {
            printf("\n===== Method %s(", curr_table->name); print_param_list(curr_table->method_params); printf(") Symbol Table =====\n");
        }
        Symbol *curr_sym = curr_table->first;
        while (curr_sym) {
            if (curr_sym->is_method) {
                printf("%s\t(", curr_sym->name); print_param_list(curr_sym->param_types); printf(")\t%s\n", type_to_string(curr_sym->type));
            } else {
                printf("%s\t\t%s", curr_sym->name, type_to_string(curr_sym->type));
                if (curr_sym->is_param) printf("\tparam");
                printf("\n");
            }
            curr_sym = curr_sym->next;
        }
        curr_table = curr_table->next;
    }
    printf("\n"); 
}

int should_print_type(struct node *node, struct node *parent) {
    if (node->annot) return 1; 
    if (node->semantic_type == TY_NONE) return 0;
    if (node->category == IdentifierNode && parent) {
        int pcat = parent->category;
        if (pcat == ProgramNode || pcat == MethodHeaderNode || pcat == FieldDeclNode || pcat == VarDeclNode || pcat == ParamDeclNode) return 0;
    }
    return 1;
}

void show_annotated(struct node *node, int depth, struct node *parent) {
    if (!node || node->category == NullNode) return;
    for (int i = 0; i < depth; i++) printf("..");
    if (node->token) printf("%s(%s)", category_name[node->category], node->token);
    else printf("%s", category_name[node->category]);
    
    if (should_print_type(node, parent)) {
        if (node->annot) printf(" - %s", node->annot);
        else printf(" - %s", type_to_string(node->semantic_type));
    }
    printf("\n");

    struct node_list *child = node->children->next;
    while (child) { show_annotated(child->node, depth + 1, node); child = child->next; }
}