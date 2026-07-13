#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "semantics.h"

extern SymbolTable *global_table;
int tmp_reg = 1;
int label_cnt = 1;

int str_lens[1000];
int str_cnt = 0;

static int is_empty_block(struct node *n) {
    return n && n->category == BlockNode && n->children->next == NULL;
}

// O SEGREDO DO OVERLOADING: Mudar o nome do método no LLVM com base nos argumentos!
void get_mangled_name(char *buf, const char *name, ParamList *params) {
    strcpy(buf, name);
    ParamList *p = params;
    while (p) {
        if (p->type == TY_INT) strcat(buf, "_i");
        else if (p->type == TY_DOUBLE) strcat(buf, "_d");
        else if (p->type == TY_BOOL) strcat(buf, "_b");
        else if (p->type == TY_STRING_ARRAY) strcat(buf, "_sa");
        p = p->next;
    }
}

void print_llvm_string(const char *token, int id) {
    char llvm_str[4096];
    int j = 0;
    int len = 0;
    for (int i = 1; token[i] != '\0'; i++) {
        if (token[i] == '"' && token[i+1] == '\0') break;
        if (token[i] == '\\') {
            i++;
            if      (token[i] == 'n') { strcpy(&llvm_str[j], "\\0A"); j+=3; len++; }
            else if (token[i] == 't') { strcpy(&llvm_str[j], "\\09"); j+=3; len++; }
            else if (token[i] == 'r') { strcpy(&llvm_str[j], "\\0D"); j+=3; len++; }
            else if (token[i] == 'f') { strcpy(&llvm_str[j], "\\0C"); j+=3; len++; }
            else if (token[i] == '"') { strcpy(&llvm_str[j], "\\22"); j+=3; len++; }
            else if (token[i] == '\\') { strcpy(&llvm_str[j], "\\5C"); j+=3; len++; }
        } else {
            llvm_str[j++] = token[i];
            len++;
        }
    }
    llvm_str[j] = '\0';
    str_lens[id] = len + 1;
    printf("@.str.%d = private unnamed_addr constant [%d x i8] c\"%s\\00\"\n", id, len + 1, llvm_str);
}

void find_strings(struct node *n) {
    if (!n || n->category == NullNode) return;
    if (n->category == StrLitNode) {
        n->semantic_type = str_cnt;
        print_llvm_string(n->token, str_cnt);
        str_cnt++;
    }
    struct node_list *c = n->children->next;
    while (c) { find_strings(c->node); c = c->next; }
}

const char *get_llvm_type(SymbolType type) {
    if (type == TY_INT)          return "i32";
    if (type == TY_DOUBLE)       return "double";
    if (type == TY_BOOL)         return "i1";
    if (type == TY_VOID)         return "void";
    if (type == TY_STRING_ARRAY) return "i8**";
    return "i32";
}

double parse_decimal(const char *token) {
    char clean[2048]; int j = 0;
    for (int i = 0; token[i] != '\0'; i++) if (token[i] != '_') clean[j++] = token[i];
    clean[j] = '\0';
    return strtod(clean, NULL);
}

unsigned long long parse_natural(const char *token) {
    char clean[2048]; int j = 0;
    for (int i = 0; token[i] != '\0'; i++) if (token[i] != '_') clean[j++] = token[i];
    clean[j] = '\0';
    return strtoull(clean, NULL, 10);
}

static Symbol *find_method_for_call(const char *name, struct node_list *arg_start) {
    Symbol *exact_match  = NULL;
    Symbol *compat_match = NULL;
    int     compat_count = 0;

    Symbol *curr = global_table->first;
    while (curr) {
        if (curr->is_method && strcmp(curr->name, name) == 0) {
            int is_exact = 1, is_compat = 1;
            ParamList *p1 = curr->param_types;
            struct node_list *p2 = arg_start;
            while (p1 && p2) {
                SymbolType arg_t = p2->node->semantic_type;
                if (p1->type != arg_t) {
                    is_exact = 0;
                    if (!(p1->type == TY_DOUBLE && arg_t == TY_INT)) is_compat = 0;
                }
                p1 = p1->next; p2 = p2->next;
            }
            if (p1 || p2) { is_exact = 0; is_compat = 0; }
            if (is_exact)        { exact_match = curr; break; }
            else if (is_compat)  { compat_match = curr; compat_count++; }
        }
        curr = curr->next;
    }
    if (exact_match)       return exact_match;
    if (compat_count == 1) return compat_match;

    curr = global_table->first;
    while (curr) {
        if (curr->is_method && strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

int gen_expr(struct node *n, SymbolTable *local_table);

int gen_expr_cast(struct node *child, SymbolType target_type, SymbolTable *local_table) {
    int reg = gen_expr(child, local_table);
    if (child->semantic_type == TY_INT && target_type == TY_DOUBLE) {
        int cast = tmp_reg++;
        printf("  %%t%d = sitofp i32 %%t%d to double\n", cast, reg);
        return cast;
    }
    return reg;
}

int gen_expr(struct node *n, SymbolTable *local_table) {
    if (!n) return -1;
    switch (n->category) {
        case NaturalNode: {
            int reg = tmp_reg++;
            printf("  %%t%d = add i32 0, %llu\n", reg, parse_natural(n->token));
            return reg;
        }
        case DecimalNode: {
            int reg = tmp_reg++;
            union { double d; unsigned long long u; } v;
            v.d = parse_decimal(n->token);
            printf("  %%t%d = fadd double 0.0, 0x%016llX\n", reg, v.u);
            return reg;
        }
        case BoolLitNode: {
            int reg = tmp_reg++;
            if (strcmp(n->token, "true") == 0) printf("  %%t%d = add i1 0, 1\n", reg);
            else printf("  %%t%d = add i1 0, 0\n", reg);
            return reg;
        }
        case IdentifierNode: {
            int reg = tmp_reg++;
            Symbol *sym = search_variable(local_table, n->token);
            if (sym) {
                printf("  %%t%d = load %s, %s* %%%s\n", reg, get_llvm_type(sym->type), get_llvm_type(sym->type), n->token);
            } else {
                sym = search_variable(global_table, n->token);
                printf("  %%t%d = load %s, %s* @%s\n", reg, get_llvm_type(sym->type), get_llvm_type(sym->type), n->token);
            }
            return reg;
        }
        case AssignNode: {
            struct node *id_node   = n->children->next->node;
            struct node *expr_node = n->children->next->next->node;
            Symbol *sym = search_variable(local_table, id_node->token);
            if (!sym) sym = search_variable(global_table, id_node->token);

            int val_reg = gen_expr_cast(expr_node, sym->type, local_table);

            if (search_variable(local_table, id_node->token)) {
                printf("  store %s %%t%d, %s* %%%s\n", get_llvm_type(sym->type), val_reg, get_llvm_type(sym->type), id_node->token);
            } else {
                printf("  store %s %%t%d, %s* @%s\n", get_llvm_type(sym->type), val_reg, get_llvm_type(sym->type), id_node->token);
            }
            return val_reg;
        }
        case PlusNode:
            return gen_expr(n->children->next->node, local_table);
        case MinusNode: {
            int r   = gen_expr(n->children->next->node, local_table);
            int reg = tmp_reg++;
            if (n->semantic_type == TY_DOUBLE) printf("  %%t%d = fsub double -0.0, %%t%d\n", reg, r);
            else printf("  %%t%d = sub i32 0, %%t%d\n", reg, r);
            return reg;
        }
        case NotNode: {
            int r   = gen_expr(n->children->next->node, local_table);
            int reg = tmp_reg++;
            printf("  %%t%d = xor i1 %%t%d, 1\n", reg, r);
            return reg;
        }
        case AddNode: {
            int r1  = gen_expr_cast(n->children->next->node, n->semantic_type, local_table);
            int r2  = gen_expr_cast(n->children->next->next->node, n->semantic_type, local_table);
            int reg = tmp_reg++;
            if (n->semantic_type == TY_DOUBLE) printf("  %%t%d = fadd double %%t%d, %%t%d\n", reg, r1, r2);
            else printf("  %%t%d = add i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case SubNode: {
            int r1  = gen_expr_cast(n->children->next->node, n->semantic_type, local_table);
            int r2  = gen_expr_cast(n->children->next->next->node, n->semantic_type, local_table);
            int reg = tmp_reg++;
            if (n->semantic_type == TY_DOUBLE) printf("  %%t%d = fsub double %%t%d, %%t%d\n", reg, r1, r2);
            else printf("  %%t%d = sub i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case MulNode: {
            int r1  = gen_expr_cast(n->children->next->node, n->semantic_type, local_table);
            int r2  = gen_expr_cast(n->children->next->next->node, n->semantic_type, local_table);
            int reg = tmp_reg++;
            if (n->semantic_type == TY_DOUBLE) printf("  %%t%d = fmul double %%t%d, %%t%d\n", reg, r1, r2);
            else printf("  %%t%d = mul i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case DivNode: {
            int r1  = gen_expr_cast(n->children->next->node, n->semantic_type, local_table);
            int r2  = gen_expr_cast(n->children->next->next->node, n->semantic_type, local_table);
            int reg = tmp_reg++;
            if (n->semantic_type == TY_DOUBLE) printf("  %%t%d = fdiv double %%t%d, %%t%d\n", reg, r1, r2);
            else printf("  %%t%d = sdiv i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case ModNode: {
            int r1  = gen_expr_cast(n->children->next->node, n->semantic_type, local_table);
            int r2  = gen_expr_cast(n->children->next->next->node, n->semantic_type, local_table);
            int reg = tmp_reg++;
            if (n->semantic_type == TY_DOUBLE) printf("  %%t%d = frem double %%t%d, %%t%d\n", reg, r1, r2);
            else printf("  %%t%d = srem i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case EqNode: case NeNode: case LtNode: case GtNode: case LeNode: case GeNode: {
            SymbolType c1 = n->children->next->node->semantic_type;
            SymbolType c2 = n->children->next->next->node->semantic_type;
            SymbolType target = (c1 == TY_DOUBLE || c2 == TY_DOUBLE) ? TY_DOUBLE : TY_INT;
            if (c1 == TY_BOOL && c2 == TY_BOOL) target = TY_BOOL;

            int r1  = gen_expr_cast(n->children->next->node, target, local_table);
            int r2  = gen_expr_cast(n->children->next->next->node, target, local_table);
            int reg = tmp_reg++;

            const char *i_op = "", *f_op = "";
            if (n->category == EqNode) { i_op = "eq";  f_op = "oeq"; }
            if (n->category == NeNode) { i_op = "ne";  f_op = "one"; }
            if (n->category == LtNode) { i_op = "slt"; f_op = "olt"; }
            if (n->category == GtNode) { i_op = "sgt"; f_op = "ogt"; }
            if (n->category == LeNode) { i_op = "sle"; f_op = "ole"; }
            if (n->category == GeNode) { i_op = "sge"; f_op = "oge"; }

            if      (target == TY_DOUBLE) printf("  %%t%d = fcmp %s double %%t%d, %%t%d\n", reg, f_op, r1, r2);
            else if (target == TY_BOOL)   printf("  %%t%d = icmp %s i1 %%t%d, %%t%d\n", reg, i_op, r1, r2);
            else                          printf("  %%t%d = icmp %s i32 %%t%d, %%t%d\n", reg, i_op, r1, r2);
            return reg;
        }
        case AndNode: {
            int r1 = gen_expr(n->children->next->node, local_table);
            printf("  store i1 %%t%d, i1* %%logic_res\n", r1);
            int l_true = label_cnt++; int l_end  = label_cnt++;
            printf("  br i1 %%t%d, label %%L%d, label %%L%d\n", r1, l_true, l_end);
            printf("L%d:\n", l_true);
            int r2 = gen_expr(n->children->next->next->node, local_table);
            printf("  store i1 %%t%d, i1* %%logic_res\n", r2);
            printf("  br label %%L%d\n", l_end);
            printf("L%d:\n", l_end);
            int res = tmp_reg++;
            printf("  %%t%d = load i1, i1* %%logic_res\n", res);
            return res;
        }
        case OrNode: {
            int r1 = gen_expr(n->children->next->node, local_table);
            printf("  store i1 %%t%d, i1* %%logic_res\n", r1);
            int l_false = label_cnt++; int l_end   = label_cnt++;
            printf("  br i1 %%t%d, label %%L%d, label %%L%d\n", r1, l_end, l_false);
            printf("L%d:\n", l_false);
            int r2 = gen_expr(n->children->next->next->node, local_table);
            printf("  store i1 %%t%d, i1* %%logic_res\n", r2);
            printf("  br label %%L%d\n", l_end);
            printf("L%d:\n", l_end);
            int res = tmp_reg++;
            printf("  %%t%d = load i1, i1* %%logic_res\n", res);
            return res;
        }
        case XorNode: {
            int r1  = gen_expr(n->children->next->node, local_table);
            int r2  = gen_expr(n->children->next->next->node, local_table);
            int reg = tmp_reg++;
            if (n->semantic_type == TY_BOOL) printf("  %%t%d = xor i1 %%t%d, %%t%d\n", reg, r1, r2);
            else printf("  %%t%d = xor i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case LshiftNode: {
            int r1  = gen_expr(n->children->next->node, local_table);
            int r2  = gen_expr(n->children->next->next->node, local_table);
            int reg = tmp_reg++;
            printf("  %%t%d = shl i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case RshiftNode: {
            int r1  = gen_expr(n->children->next->node, local_table);
            int r2  = gen_expr(n->children->next->next->node, local_table);
            int reg = tmp_reg++;
            printf("  %%t%d = ashr i32 %%t%d, %%t%d\n", reg, r1, r2);
            return reg;
        }
        case ParseArgsNode: {
            struct node *id_node = n->children->next->node;
            int idx_reg = gen_expr(n->children->next->next->node, local_table);
            int arg_idx = tmp_reg++;
            printf("  %%t%d = add i32 %%t%d, 1\n", arg_idx, idx_reg);
            
            int array_reg = tmp_reg++;
            Symbol *sym = search_variable(local_table, id_node->token);
            if (sym) {
                printf("  %%t%d = load i8**, i8*** %%%s\n", array_reg, id_node->token);
            } else {
                printf("  %%t%d = load i8**, i8*** @global_argv\n", array_reg);
            }
            
            int ptr_reg = tmp_reg++;
            printf("  %%t%d = getelementptr inbounds i8*, i8** %%t%d, i32 %%t%d\n", ptr_reg, array_reg, arg_idx);
            int str_reg = tmp_reg++;
            printf("  %%t%d = load i8*, i8** %%t%d\n", str_reg, ptr_reg);
            int res_reg = tmp_reg++;
            printf("  %%t%d = call i32 @atoi(i8* %%t%d)\n", res_reg, str_reg);
            return res_reg;
        }
        case LengthNode: {
            int argc_reg = tmp_reg++;
            printf("  %%t%d = load i32, i32* @global_argc\n", argc_reg);
            int reg = tmp_reg++;
            printf("  %%t%d = sub i32 %%t%d, 1\n", reg, argc_reg);
            return reg;
        }
        case CallNode: {
            struct node *id_node   = n->children->next->node;
            struct node_list *arg_start = n->children->next->next;
            int arg_regs[100];
            int arg_cnt = 0;

            Symbol *meth_sym = find_method_for_call(id_node->token, arg_start);
            if (!meth_sym) return -1;

            ParamList *pt = meth_sym->param_types;
            struct node_list *c  = arg_start;
            while (c) {
                if (pt) {
                    arg_regs[arg_cnt++] = gen_expr_cast(c->node, pt->type, local_table);
                    pt = pt->next;
                } else {
                    arg_regs[arg_cnt++] = gen_expr(c->node, local_table);
                }
                c = c->next;
            }

            printf("  ");
            int reg = -1;
            if (meth_sym->type != TY_VOID) {
                reg = tmp_reg++;
                printf("%%t%d = ", reg);
            }

            char mangled_name[512];
            get_mangled_name(mangled_name, id_node->token, meth_sym->param_types);

            const char *func_name = mangled_name;
            if (strcmp(id_node->token, "main") == 0 && meth_sym->param_types && meth_sym->param_types->type == TY_STRING_ARRAY && meth_sym->param_types->next == NULL) {
                func_name = "juc_main";
            }

            printf("call %s @%s(", get_llvm_type(meth_sym->type), func_name);

            pt = meth_sym->param_types;
            for (int i = 0; i < arg_cnt; i++) {
                if (pt) {
                    printf("%s %%t%d", get_llvm_type(pt->type), arg_regs[i]);
                    pt = pt->next;
                } else {
                    printf("i32 %%t%d", arg_regs[i]);
                }
                if (i < arg_cnt - 1) printf(", ");
            }
            printf(")\n");
            return reg;
        }
        default: break;
    }
    return -1;
}

int gen_stmt(struct node *n, SymbolTable *local_table) {
    if (!n || n->category == NullNode) return 0;

    if (n->category == AssignNode) {
        gen_expr(n, local_table);
        return 0;
    } else if (n->category == ParseArgsNode) {
        gen_expr(n, local_table);
        return 0;
    } else if (n->category == PrintNode) {
        struct node *expr = n->children->next->node;
        if (expr->category == StrLitNode) {
            int id  = expr->semantic_type;
            int len = str_lens[id];
            printf("  %%t%d = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.str, i32 0, i32 0), i8* getelementptr inbounds ([%d x i8], [%d x i8]* @.str.%d, i32 0, i32 0))\n", tmp_reg++, len, len, id);
        } else {
            int val_reg = gen_expr(expr, local_table);
            if (expr->semantic_type == TY_INT) {
                printf("  %%t%d = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.int, i32 0, i32 0), i32 %%t%d)\n", tmp_reg++, val_reg);
            } else if (expr->semantic_type == TY_DOUBLE) {
                printf("  %%t%d = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.fmt.double, i32 0, i32 0), double %%t%d)\n", tmp_reg++, val_reg);
            } else if (expr->semantic_type == TY_BOOL) {
                int br_true  = label_cnt++;
                int br_false = label_cnt++;
                int br_end   = label_cnt++;
                printf("  br i1 %%t%d, label %%L%d, label %%L%d\n", val_reg, br_true, br_false);
                printf("L%d:\n", br_true);
                printf("  %%t%d = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.fmt.true, i32 0, i32 0))\n", tmp_reg++);
                printf("  br label %%L%d\n", br_end);
                printf("L%d:\n", br_false);
                printf("  %%t%d = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.fmt.false, i32 0, i32 0))\n", tmp_reg++);
                printf("  br label %%L%d\n", br_end);
                printf("L%d:\n", br_end);
            }
        }
        return 0;
    } else if (n->category == ReturnNode) {
        if (n->children->next) {
            Symbol *ret_sym = search_variable(local_table, "return");
            int val = gen_expr_cast(n->children->next->node, ret_sym->type, local_table);
            printf("  store %s %%t%d, %s* %%return_val\n", get_llvm_type(ret_sym->type), val, get_llvm_type(ret_sym->type));
        }
        printf("  br label %%return_end\n");
        return 1;
    } else if (n->category == IfNode) {
        struct node *cond_expr = n->children->next->node;
        struct node *then_stmt = n->children->next->next->node;
        struct node *else_stmt = n->children->next->next->next ? n->children->next->next->next->node : NULL;

        int cond_reg = gen_expr(cond_expr, local_table);
        int l_then = label_cnt++;
        int l_else = label_cnt++;
        int l_end  = label_cnt++;

        int has_real_else = else_stmt && else_stmt->category != NullNode && !is_empty_block(else_stmt);

        if (has_real_else) {
            printf("  br i1 %%t%d, label %%L%d, label %%L%d\n", cond_reg, l_then, l_else);
        } else {
            printf("  br i1 %%t%d, label %%L%d, label %%L%d\n", cond_reg, l_then, l_end);
        }

        printf("L%d:\n", l_then);
        int ret_then = gen_stmt(then_stmt, local_table);
        if (!ret_then) printf("  br label %%L%d\n", l_end);

        int ret_else = 0;
        if (has_real_else) {
            printf("L%d:\n", l_else);
            ret_else = gen_stmt(else_stmt, local_table);
            if (!ret_else) printf("  br label %%L%d\n", l_end);
        }

        if (!ret_then || (has_real_else && !ret_else) || !has_real_else) {
            printf("L%d:\n", l_end);
            return 0;
        } else {
            return 1;
        }
    } else if (n->category == WhileNode) {
        struct node *cond_expr = n->children->next->node;
        struct node *do_stmt   = n->children->next->next->node;

        int l_cond = label_cnt++;
        int l_loop = label_cnt++;
        int l_end  = label_cnt++;

        printf("  br label %%L%d\n", l_cond);
        printf("L%d:\n", l_cond);
        int cond_reg = gen_expr(cond_expr, local_table);
        printf("  br i1 %%t%d, label %%L%d, label %%L%d\n", cond_reg, l_loop, l_end);
        printf("L%d:\n", l_loop);
        int ret_do = gen_stmt(do_stmt, local_table);
        if (!ret_do) printf("  br label %%L%d\n", l_cond);

        printf("L%d:\n", l_end);
        return 0;
    } else if (n->category == CallNode) {
        gen_expr(n, local_table);
        return 0;
    } else if (n->category == BlockNode || n->category == MethodBodyNode) {
        struct node_list *c = n->children->next;
        int returned = 0;
        while (c) {
            if (!returned) returned = gen_stmt(c->node, local_table);
            c = c->next;
        }
        return returned;
    }
    return 0;
}

void generate_code(struct node *program) {
    if (!program || program->category != ProgramNode) return;

    printf("declare i32 @printf(i8*, ...)\n");
    printf("declare i32 @atoi(i8*)\n\n");

    printf("@.fmt.int    = private unnamed_addr constant [3 x i8] c\"%%d\\00\"\n");
    printf("@.fmt.double = private unnamed_addr constant [6 x i8] c\"%%.16e\\00\"\n");
    printf("@.fmt.true   = private unnamed_addr constant [5 x i8] c\"true\\00\"\n");
    printf("@.fmt.false  = private unnamed_addr constant [6 x i8] c\"false\\00\"\n");
    printf("@.fmt.str    = private unnamed_addr constant [3 x i8] c\"%%s\\00\"\n\n");

    printf("@global_argc = global i32 0\n");
    printf("@global_argv = global i8** null\n\n");

    find_strings(program);
    printf("\n");

    Symbol *global_sym = global_table->first;
    while (global_sym) {
        if (!global_sym->is_method) {
            const char *type = get_llvm_type(global_sym->type);
            const char *init = (global_sym->type == TY_DOUBLE) ? "0.0" : "0";
            printf("@%s = global %s %s\n", global_sym->name, type, init);
        }
        global_sym = global_sym->next;
    }
    printf("\n");

    struct node_list *curr_decl = program->children->next->next;
    while (curr_decl) {
        if (curr_decl->node->category == MethodDeclNode) {
            struct node *header       = curr_decl->node->children->next->node;
            struct node *body         = curr_decl->node->children->next->next->node;
            struct node *ret_type_node = header->children->next->node;
            struct node *id_node      = header->children->next->next->node;
            struct node *params_node  = header->children->next->next->next->node;

            ParamList *p_head = NULL, *p_tail = NULL;
            struct node_list *p = params_node->children->next;
            while (p) {
                ParamList *new_p = malloc(sizeof(ParamList));
                new_p->type = get_type(p->node->children->next->node->category);
                new_p->next = NULL;
                if (!p_head) { p_head = new_p; p_tail = new_p; }
                else         { p_tail->next = new_p; p_tail = new_p; }
                p = p->next;
            }

            Symbol *meth_sym = search_method(global_table, id_node->token, p_head);
            if (!meth_sym) {
                Symbol *curr = global_table->first;
                while (curr) {
                    if (curr->is_method && strcmp(curr->name, id_node->token) == 0)
                        { meth_sym = curr; break; }
                    curr = curr->next;
                }
            }
            if (!meth_sym) { curr_decl = curr_decl->next; continue; }

            SymbolTable *local_table = meth_sym->method_table;
            const char  *llvm_ret   = get_llvm_type(get_type(ret_type_node->category));
            tmp_reg   = 1;
            label_cnt = 1;

            char mangled_name[512];
            get_mangled_name(mangled_name, id_node->token, p_head);

            if (strcmp(id_node->token, "main") == 0 && p_head && p_head->type == TY_STRING_ARRAY && p_head->next == NULL) {
                char *main_param = "args";
                Symbol *p_sym = local_table->first;
                while(p_sym) {
                    if (p_sym->is_param) { main_param = p_sym->name; break; }
                    p_sym = p_sym->next;
                }

                printf("define i32 @main(i32 %%argc, i8** %%argv) {\n");
                printf("  store i32 %%argc, i32* @global_argc\n");
                printf("  store i8** %%argv, i8*** @global_argv\n");
                printf("  call void @juc_main(i8** %%argv)\n"); 
                printf("  ret i32 0\n}\n\n");
                
                printf("define void @juc_main(i8** %%%s_val) {\n", main_param);

            } else {
                printf("define %s @%s(", llvm_ret, mangled_name);
                Symbol *p_sym = local_table->first;
                int first = 1;
                while (p_sym) {
                    if (p_sym->is_param) {
                        if (!first) printf(", ");
                        printf("%s %%%s_val", get_llvm_type(p_sym->type), p_sym->name);
                        first = 0;
                    }
                    p_sym = p_sym->next;
                }
                printf(") {\n");
            }

            if (strcmp(llvm_ret, "void") != 0) {
                printf("  %%return_val = alloca %s\n", llvm_ret);
                if      (strcmp(llvm_ret, "double") == 0) printf("  store double 0.0, double* %%return_val\n");
                else if (strcmp(llvm_ret, "i1") == 0)     printf("  store i1 0, i1* %%return_val\n");
                else                                      printf("  store i32 0, i32* %%return_val\n");
            }

            printf("  %%logic_res = alloca i1\n");

            Symbol *var = local_table->first;
            while (var) {
                if (!var->is_method && strcmp(var->name, "return") != 0) {

                    printf("  %%%s = alloca %s\n", var->name, get_llvm_type(var->type));

                    if      (var->type == TY_DOUBLE) printf("  store double 0.0, double* %%%s\n", var->name);
                    else if (var->type == TY_BOOL)   printf("  store i1 0, i1* %%%s\n", var->name);
                    else if (var->type == TY_STRING_ARRAY) printf("  store i8** null, i8*** %%%s\n", var->name);
                    else                             printf("  store i32 0, i32* %%%s\n", var->name);

                    if (var->is_param)
                        printf("  store %s %%%s_val, %s* %%%s\n", get_llvm_type(var->type), var->name, get_llvm_type(var->type), var->name);
                }
                var = var->next;
            }

            int returned = 0;
            struct node_list *stmt = body->children->next;
            while (stmt) {
                if (!returned) returned = gen_stmt(stmt->node, local_table);
                stmt = stmt->next;
            }

            if (!returned) printf("  br label %%return_end\n");

            printf("return_end:\n");
            if (strcmp(llvm_ret, "void") == 0) {
                printf("  ret void\n");
            } else {
                int final_ret = tmp_reg++;
                printf("  %%t%d = load %s, %s* %%return_val\n", final_ret, llvm_ret, llvm_ret);
                printf("  ret %s %%t%d\n", llvm_ret, final_ret);
            }

            printf("}\n\n");
        }
        curr_decl = curr_decl->next;
    }
}