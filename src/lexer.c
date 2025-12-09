// lexer.c  -- small single-file tokenizer
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    T_EOF,
    T_IDENT,
    T_NUMBER,
    T_STRING,
    T_CHAR,
    T_KEYWORD,
    T_OP,      // operators and punctuation
    T_COMMENT,
    T_UNKNOWN
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;      // allocated
    int line, col;
} Token;

typedef struct {
    const char *src;
    size_t i;
    int line, col;
} Lexer;

const char *keywords[] = { "mov","load","store","if","else","while","return","int","ptr", NULL };

int is_keyword(const char *s){
    for(int k=0; keywords[k]; ++k)
        if(strcmp(s, keywords[k])==0) return 1;
    return 0;
}

Lexer lexer_init(const char *s){
    return (Lexer){ .src = s, .i = 0, .line = 1, .col = 1 };
}

static char peek(Lexer *L){ return L->src[L->i]; }
static char get(Lexer *L){
    char c = L->src[L->i++];
    if(c == '\n'){ L->line++; L->col = 1; } else L->col++;
    return c;
}
static int starts_with(Lexer *L, const char *pat){
    return strncmp(L->src + L->i, pat, strlen(pat)) == 0;
}

Token make_token(TokenKind k, const char *text, int line, int col){
    Token t; t.kind=k; t.text = strdup(text); t.line=line; t.col=col; return t;
}

Token lex_number(Lexer *L, int st_line, int st_col){
    size_t start = L->i;
    // support 0x hex
    if(peek(L)=='0' && (L->src[L->i+1]=='x' || L->src[L->i+1]=='X')){
        get(L); get(L);
        while(isxdigit(peek(L))) get(L);
    } else {
        while(isdigit(peek(L))) get(L);
    }
    size_t len = L->i - start;
    char *buf = malloc(len+1); strncpy(buf, L->src+start, len); buf[len]=0;
    Token t = make_token(T_NUMBER, buf, st_line, st_col);
    free(buf);
    return t;
}

Token lex_ident_or_keyword(Lexer *L, int st_line, int st_col){
    size_t start = L->i;
    while(isalnum(peek(L)) || peek(L)=='_') get(L);
    size_t len = L->i - start;
    char *buf = malloc(len+1); strncpy(buf, L->src+start, len); buf[len]=0;
    Token t = is_keyword(buf) ? make_token(T_KEYWORD, buf, st_line, st_col)
                              : make_token(T_IDENT, buf, st_line, st_col);
    free(buf);
    return t;
}

Token lex_string(Lexer *L, int st_line, int st_col){
    get(L); // consume opening "
    size_t start = L->i;
    while(peek(L) && peek(L) != '"'){
        if(peek(L) == '\\') { get(L); if(peek(L)) get(L); } // skip escape pair
        else get(L);
    }
    size_t len = L->i - start;
    char *buf = malloc(len+1); strncpy(buf, L->src+start, len); buf[len]=0;
    if(peek(L) == '"') get(L); // closing
    Token t = make_token(T_STRING, buf, st_line, st_col);
    free(buf);
    return t;
}

Token next_token(Lexer *L){
    while(isspace(peek(L))){
        get(L);
    }
    if(peek(L) == 0) return make_token(T_EOF, "<eof>", L->line, L->col);
    int st_line = L->line, st_col = L->col;
    char c = peek(L);

    // comments: // to end of line
    if(c=='/' && L->src[L->i+1]=='/'){
        size_t start = L->i;
        while(peek(L) && peek(L)!='\n') get(L);
        size_t len = L->i - start;
        char *buf = malloc(len+1); strncpy(buf, L->src+start, len); buf[len]=0;
        Token t = make_token(T_COMMENT, buf, st_line, st_col);
        free(buf);
        return t;
    }

    // numbers
    if(isdigit(c)){
        return lex_number(L, st_line, st_col);
    }

    // identifiers / keywords
    if(isalpha(c) || c=='_'){
        return lex_ident_or_keyword(L, st_line, st_col);
    }

    // strings
    if(c == '"'){
        return lex_string(L, st_line, st_col);
    }

    // multi-char ops first
    const char *multi_ops[] = {"==","!=","<=",">=","<<",">>","<<=","+=","-=","*=","/=", NULL};
    for(int i=0; multi_ops[i]; ++i){
        size_t Lm = strlen(multi_ops[i]);
        if(strncmp(L->src + L->i, multi_ops[i], Lm) == 0){
            char buf[5]; strncpy(buf, multi_ops[i], Lm); buf[Lm]=0;
            for(size_t k=0;k<Lm;k++) get(L);
            return make_token(T_OP, buf, st_line, st_col);
        }
    }

    // single-char ops/punct
    char one[2] = { get(L), 0 };
    return make_token(T_OP, one, st_line, st_col);
}

// demo usage
int main(void){
    const char *code = "int x = 0x1F; // sample\nmov r0, x\nif (x == 31) return;";
    Lexer L = lexer_init(code);
    for(;;){
        Token t = next_token(&L);
        printf("Token: kind=%d text='%s' (line %d col %d)\n", t.kind, t.text, t.line, t.col);
        if(t.kind == T_EOF) break;
        free(t.text);
    }
    return 0;
}
