%{
/*
 * João Natálio
 * Numero de Estudante: 2023205576
 */
#include "y.tab.h"
#include <string.h>
#include "ast.h"

int line = 1, column = 1;
int syn_line = 1, syn_column = 1;
int comment_line, comment_column;
int string_line, string_column;

char strbuf[4096];
int stridx, string_error;
char *error_yytext = ""; 

extern int flag_l;

#define YY_USER_ACTION { \
    syn_line = line; syn_column = column; \
    column += yyleng; \
    error_yytext = yytext; \
}

// MACROS ATUALIZADOS: Guardamos a linha e coluna exata no token struct!
#define RET(tok, text) { \
    if(flag_l) printf(text); \
    yylval.token.str = strdup(yytext); \
    yylval.token.line = line; \
    yylval.token.col = column - yyleng; \
    return tok; \
}
#define RET_VAL(tok, text, val) { \
    if(flag_l) printf(text, val); \
    yylval.token.str = strdup(val); \
    yylval.token.line = line; \
    yylval.token.col = column - yyleng; \
    return tok; \
}
%}

%x COMMENT STR

digit [0-9]
digits [0-9]([0-9_]*[0-9])?
exponent [eE][+-]?{digits}
decimal ((({digits}"."{digits}?)|("."{digits})|({digits}".")){exponent}?|({digits}{exponent}))
natural (0|[1-9]([0-9_]*[0-9])?)
identifier [a-zA-Z_$][a-zA-Z0-9_$]*

%%

"/*" { comment_line = line; comment_column = column - yyleng; BEGIN(COMMENT); }
<COMMENT>"*/" { BEGIN(INITIAL); }
<COMMENT>\r\n|\n|\r { line++; column = 1; }
<COMMENT>. { ; }
<COMMENT><<EOF>> { 
    printf("Line %d, col %d: unterminated comment\n", comment_line, comment_column); 
    syn_line = line; syn_column = column; error_yytext = "";
    return 0; 
}

"//"[^\n\r]* { ; }

\" { string_line = line; string_column = column - yyleng; string_error = 0; stridx = 0; strbuf[stridx++] = '"'; BEGIN(STR); }
<STR>\\[ntrf\"\\] { if(!string_error) { for(int i=0; i<yyleng; i++) strbuf[stridx++] = yytext[i]; } }
<STR>\\[^ntrf\"\\\n\r] { printf("Line %d, col %d: invalid escape sequence (%s)\n", line, column - yyleng, yytext); string_error = 1; }
<STR>\\ { printf("Line %d, col %d: invalid escape sequence (\\)\n", line, column - yyleng); string_error = 1; }
<STR>[^\\\"\n\r]+ { if(!string_error) { for(int i=0; i<yyleng; i++) strbuf[stridx++] = yytext[i]; } }
<STR>\" {
    if(!string_error) {
        strbuf[stridx++] = '"'; strbuf[stridx] = '\0';
        if(flag_l) printf("STRLIT(%s)\n", strbuf);
        yylval.token.str = strdup(strbuf);
        yylval.token.line = string_line;
        yylval.token.col = string_column;
        syn_line = string_line; syn_column = string_column; error_yytext = strbuf; 
        BEGIN(INITIAL); return STRLIT;
    }
    BEGIN(INITIAL);
}
<STR>\r\n|\n|\r { printf("Line %d, col %d: unterminated string literal\n", string_line, string_column); line++; column = 1; BEGIN(INITIAL); }
<STR><<EOF>> { 
    printf("Line %d, col %d: unterminated string literal\n", string_line, string_column); 
    syn_line = line; syn_column = column; error_yytext = "";
    return 0; 
}

"System.out.print" { RET(PRINT, "PRINT\n"); }
"Integer.parseInt" { RET(PARSEINT, "PARSEINT\n"); }
".length"          { RET(DOTLENGTH, "DOTLENGTH\n"); }
"boolean" { RET(BOOL, "BOOL\n"); }
"class"   { RET(CLASS, "CLASS\n"); }
"double"  { RET(DOUBLE, "DOUBLE\n"); }
"else"    { RET(ELSE, "ELSE\n"); }
"if"      { RET(IF, "IF\n"); }
"int"     { RET(INT, "INT\n"); }
"public"  { RET(PUBLIC, "PUBLIC\n"); }
"return"  { RET(RETURN, "RETURN\n"); }
"static"  { RET(STATIC, "STATIC\n"); }
"String"  { RET(STRING, "STRING\n"); }
"void"    { RET(VOID, "VOID\n"); }
"while"   { RET(WHILE, "WHILE\n"); }

"true"|"false" { RET_VAL(BOOLLIT, "BOOLLIT(%s)\n", yytext); }

"for"|"++"|"--"|"Integer"|"null"|"System"|"abstract"|"continue"|"switch"|"assert"|"default"|"package"|"synchronized"|"do"|"goto"|"private"|"new"|"this"|"break"|"implements"|"protected"|"throw"|"byte"|"import"|"throws"|"case"|"enum"|"instanceof"|"transient"|"catch"|"extends"|"short"|"try"|"char"|"final"|"interface"|"finally"|"long"|"strictfp"|"volatile"|"const"|"float"|"native"|"super" { RET(RESERVED, "RESERVED\n"); }

"<<" { RET(LSHIFT, "LSHIFT\n"); }
">>" { RET(RSHIFT, "RSHIFT\n"); }
"==" { RET(EQ, "EQ\n"); }
"!=" { RET(NE, "NE\n"); }
">=" { RET(GE, "GE\n"); }
"<=" { RET(LE, "LE\n"); }
"&&" { RET(AND, "AND\n"); }
"||" { RET(OR, "OR\n"); }
"->" { RET(ARROW, "ARROW\n"); }
"!"  { RET(NOT, "NOT\n"); }
">"  { RET(GT, "GT\n"); }
"<"  { RET(LT, "LT\n"); }
"="  { RET(ASSIGN, "ASSIGN\n"); }
"^"  { RET(XOR, "XOR\n"); }
"+"  { RET(PLUS, "PLUS\n"); }
"-"  { RET(MINUS, "MINUS\n"); }
"*"  { RET(STAR, "STAR\n"); }
"/"  { RET(DIV, "DIV\n"); }
"%"  { RET(MOD, "MOD\n"); }
";"  { RET(SEMICOLON, "SEMICOLON\n"); }
","  { RET(COMMA, "COMMA\n"); }
"("  { RET(LPAR, "LPAR\n"); }
")"  { RET(RPAR, "RPAR\n"); }
"{"  { RET(LBRACE, "LBRACE\n"); }
"}"  { RET(RBRACE, "RBRACE\n"); }
"["  { RET(LSQ, "LSQ\n"); }
"]"  { RET(RSQ, "RSQ\n"); }

{decimal} { RET_VAL(DECIMAL, "DECIMAL(%s)\n", yytext); }
{natural} { RET_VAL(NATURAL, "NATURAL(%s)\n", yytext); }
{identifier} { RET_VAL(IDENTIFIER, "IDENTIFIER(%s)\n", yytext); }

[ \t\f]+ { ; }
(\r\n|\n|\r) { line++; column = 1; }
. { printf("Line %d, col %d: illegal character (%c)\n", line, column - yyleng, yytext[0]); }

<<EOF>> { 
    syn_line = line; syn_column = column; error_yytext = "";
    return 0;
}

%%
int yywrap() { return 1; }