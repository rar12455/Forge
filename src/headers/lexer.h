/*
 * Copyright (C) 2026 rar
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

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
} TokenType;

typedef struct {
    TokenType type;
    const char *start;    // Points to the first character of the token in the source buffer
    size_t length;        // Length of the token string slice
    int line;             // Track line count for descriptive error reports
} Token;

typedef struct {
    const char *source;   // The full loaded file buffer
    const char *cursor;   // Current character pointer position
    int line;             // Current line counter state
} LexerState;

// Lexer Lifecycle API
void lexer_init(LexerState *lexer, const char *source_code);
Token lexer_next_token(LexerState *lexer);

#endif
