/*
 * Copyright (C) 2026 rar
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "../headers/util.h"
#include <stddef.h>

#include <stdbool.h>
#include <string.h>

typedef enum {
    // Structural Tokens
    TOKEN_EOF = 0,
    TOKEN_ERROR,

    // Identifiers & Literals
    TOKEN_IDENTIFIER,
    TOKEN_STRING_LITERAL,
    TOKEN_INT_LITERAL,

    // Keywords
    TOKEN_IMPORT,
    TOKEN_AS,
    TOKEN_ASM,
    TOKEN_COMPTIME,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_DEFER,
    TOKEN_DEFER_ERR,
    TOKEN_FOR,
    TOKEN_IF,
    TOKEN_ENUM,
    TOKEN_CONSTEXPR,
    TOKEN_ELSE,
    TOKEN_EXTERN,
    TOKEN_FALSE,
    TOKEN_IN,
    TOKEN_LOOP,
    TOKEN_MATCH,
    TOKEN_MODULE,
    TOKEN_NOT,
    TOKEN_OR,
    TOKEN_AND,
    TOKEN_RETURN,
    TOKEN_STRUCT,
    TOKEN_TRUE,
    TOKEN_UNION,
    TOKEN_WHILE,



    // Attributes
    TOKEN_MUT_ATTR,       // @mut
    TOKEN_UNINIT_ATTR,    // @uninit

    // Symbols & Operators
    TOKEN_DOT,            // .
    TOKEN_SEMICOLON,      // ;
    TOKEN_LEFT_PAREN,     // (
    TOKEN_RIGHT_PAREN,    // )
    TOKEN_LEFT_BRACE,     // {
    TOKEN_RIGHT_BRACE,    // }
    TOKEN_ASSIGN          // =
} token_type;

typedef struct {
    token_type type;
    const char *start;    // Points to the first character of the token in the source buffer
    size_t length;        // Length of the token string slice
    int line;             // Track line count for descriptive error reports
} token_t;

/* maybe imlement this later. */
typedef struct {
    const char *source;   // The full loaded file buffer
    const char *cursor;   // Current character pointer position
    int line;             // Current line counter state
} lexer_state;

/* TODO: implement this */
// Lexer Lifecycle API
void lexer_init(lexer_state *lexer, const char *source_code);
token_t lexer_next_token(lexer_state *lexer);

/* Totally 27 keywords, (see section 3.3)
 * specified by the forge version 1.4 spec. */
static const char keywords[27][10] = {
        "as","asm","break","comptime","constexpr",
        "continue","defer","defer_err","else","enum",
        "extern","false","for","if","import",
        "in","loop","match","module","not",
        "or","and","return","struct","true",
        "union","while"
};

/* 'strongly discouraged' identifier names, 
 * according to forge version 1.4 spec, (see section 3.3)
 * compiler throws a warning.
 */
static const char discouraged_words[2][8] = {
        "mut","uninit"
};

void
tokengen(char* psuedo_token)
{
        token_t token;

        /* TODO: implement token line for helpful error messages */
        if (psuedo_token == "as") {
                token.type   = TOKEN_AS;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "asm") {
                token.type   = TOKEN_ASM;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "break") {
                token.type   = TOKEN_BREAK;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "comptime") {
                token.type   = TOKEN_COMPTIME;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "constexpr") {
                token.type   = TOKEN_CONSTEXPR;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "continue") {
                token.type   = TOKEN_CONTINUE;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "defer") {
                token.type   = TOKEN_DEFER;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "defer_err") {
                token.type   = TOKEN_DEFER_ERR;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "else") {
                token.type   = TOKEN_ELSE;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "enum") {
                token.type   = TOKEN_ENUM;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "extern") {
                token.type   = TOKEN_EXTERN;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "false") {
                token.type   = TOKEN_FALSE;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "for") {
                token.type   = TOKEN_FOR;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "if") {
                token.type   = TOKEN_IF;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "import") {
                token.type   = TOKEN_IMPORT;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "in") {
                token.type   = TOKEN_IN;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "loop") {
                token.type   = TOKEN_LOOP;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "match") {
                token.type   = TOKEN_MATCH;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "module") {
                token.type   = TOKEN_MODULE;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "not") {
                token.type   = TOKEN_NOT;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "or") {
                token.type   = TOKEN_OR;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "and") {
                token.type   = TOKEN_AND;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "return") {
                token.type   = TOKEN_RETURN;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "struct") {
                token.type   = TOKEN_STRUCT;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "true") {
                token.type   = TOKEN_TRUE;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "union") {
                token.type   = TOKEN_UNION;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else if (psuedo_token == "while") {
                token.type   = TOKEN_WHILE;
                token.start  = 0;
                token.length = 0;
                token.line   = 0;
        }
        else {
                /* TODO: handle identifiers */
                /* handle_id(); */
        }

        /* printf("psuedo_tokens: %s\n",psuedo_token); */
}

void
lex(char* buffer)
{
        /*
        * Make a 'lexer cache' for easly scanning keywords,
        * lexer stores all the chars in this cache until it hits a
        * newline if it hits a newline it compares (strcmp()) to with
        * keywords array to look if they match, if so generate a token for
        * matched keyword, if not check for if it's an identifier 
        * (see identifier rules in Forge spec version 1.4 section 3.4).
        */
        char lexer_cache[64];

        static bool in_comment_c = false; 
        static bool in_comment_cpp = false;
        int cache_index = 0;

        for (int i = 0; buffer[i] != '\0'; i++) {

                /* Comment style 1: Original C-style */
                /* state 1: inside a C-style comment. */
                if (in_comment_c) {
                        if (buffer[i] == '*' && buffer[i+1] == '/') {
                                in_comment_c = false;
                                /* skip the '/' */
                                i++;
                        }
                        /* skip all the text */
                        continue; 
                }
                /* Comment style 2: C++-style // comments */
                /* state 2: inside a C++-style comment. */
                if (in_comment_cpp) {
                        if (buffer[i] == '\n') {
                                in_comment_cpp = false;
                                /* no i++ needed here, let the loop handle the newline */
                                /* i++; */
                        }
                        /* skip all the text */
                        continue;
                }
                /* state 3: standard code, hunting for comments */
                if (buffer[i] == '/' && buffer[i+1] == '*') {
                        in_comment_c = true;
                        /* skip '*' */
                        i++; 
                        continue;
                }

                if (buffer[i] == '/' && buffer[i+1] == '/') {
                        in_comment_cpp = true;
                        /* skip the second '/' */
                        i++; 
                        continue;
                }

                
                /* other lexer actions for non-comments: */

                /* skip the tab and newline immediately, white space is useful now, */
                /* so skip the white space later. */
                if (buffer[i] == '\t' || buffer[i] == '\n')
                        continue;

                /* cache the buffer */
                if (isalnum(buffer[i]) || buffer[i] == '_' || buffer[i] == '@') {
                        if (cache_index < 63) {
                                lexer_cache[cache_index] = buffer[i];
                                cache_index++;
                        }
                }

                if (
                    buffer[i+1] == ' ' || buffer[i+1] == ';' || buffer[i+1] == '.' || 
                    buffer[i+1] == '(' || buffer[i+1] == ')' || buffer[i+1] == '{' || 
                    buffer[i+1] == '}' || buffer[i+1] == ','
                    ) 
                {
                        if (cache_index > 0) {

                                /* clean-up seal */
                                lexer_cache[cache_index] = '\0';

                                /* compare lexer_cache to keywords array. */
                                bool found_keyword = false;
                                for (int j = 0; j < 27; j++) {
                                        if (strcmp(lexer_cache,keywords[j]) == 0) {
                                                /* finally, generate tokens */
                                                tokengen(lexer_cache);
                                                found_keyword = true;
                                                break;
                                        }
                                }      
                                /* reset back to 0 so the next word starts at beginning */
                                cache_index = 0;

                                if (found_keyword) {
                                        continue;
                                }               
                        }    
                }

                /* skip whitespace */
                if (buffer[i] == ' ')
                        continue;

        }
}

void
open_file(char* file)
{
        char buffer[4096];
        FILE* fp = fopen(file,"r");

        while(fgets(buffer,sizeof(buffer),fp))
        {
                lex(buffer);
        }

        fclose(fp);
}

int
main(int argc, char** argv)
{
        if (argc != 2) {
                print_usage();
                exit(1);
        } 
        open_file(argv[1]);
        return 0;
}
