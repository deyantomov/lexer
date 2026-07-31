#pragma once
#include <stddef.h>
#include <stdio.h>

typedef struct {
  int line;
  int col;
} Position;

typedef enum {
  TOKEN_SYMBOL,
  TOKEN_NUMBER,
  TOKEN_KEYWORD,
  TOKEN_PUNCT,
  TOKEN_EOF
} TokenType;

typedef struct {
  TokenType type;
  const char *start; // pointer into source text, NULL for EOF
  size_t length;
  Position position;
} Token;

typedef struct {
  Token *items;
  size_t capacity;
  size_t count;
} DynamicArray;

typedef enum {
  LEXER_OK,
  LEXER_ERROR_ALLOC,
  LEXER_ERROR_ILLEGAL_CHAR
} LexerErrorCode;

typedef struct {
  LexerErrorCode code;
  Position position;
} LexerError;

typedef struct {
  const char **keywords;
  size_t keyword_count;
  const char **puncts;
  size_t punct_count;
  const char *comment;
  size_t comment_len;
} LexerConfig;

typedef struct {
  LexerConfig config;
  DynamicArray tokens;
  LexerError *error;
} Lexer;

#ifdef LEXER_IMPLEMENTATION
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DA_CAPACITY 16

static void emit_error(Lexer *lexer, LexerErrorCode code, Position position) {
  LexerError *err = malloc(sizeof(LexerError));
  if (err) {
    err->code = code;
    err->position = position;
  }
  lexer->error = err;
}

static void da_init(DynamicArray *da) {
  da->count = 0;
  da->capacity = DEFAULT_DA_CAPACITY;
  da->items = malloc(da->capacity * sizeof(Token));
}

static int da_add(DynamicArray *da, Token item) {
  if (da->count == da->capacity) {
    size_t new_capacity = da->capacity * 2;
    Token *new_items = realloc(da->items, new_capacity * sizeof(Token));
    if (new_items == NULL) {
      return -1;
    }

    da->items = new_items;
    da->capacity = new_capacity;
  }

  da->items[da->count++] = item;

  return 0;
}

const char *token_type_to_string(TokenType type) {
  switch (type) {
  case TOKEN_SYMBOL:
    return "TOKEN_SYMBOL";
  case TOKEN_NUMBER:
    return "TOKEN_NUMBER";
  case TOKEN_KEYWORD:
    return "TOKEN_KEYWORD";
  case TOKEN_PUNCT:
    return "TOKEN_PUNCT";
  case TOKEN_EOF:
    return "TOKEN_EOF";
  }
  return "UNKNOWN";
}

Lexer *lexer_init(LexerConfig config) {
  Lexer *lexer = malloc(sizeof(Lexer));
  if (lexer == NULL) {
    fprintf(stderr, "Failed to allocate lexer\n");
    return NULL;
  }

  lexer->config = config;
  da_init(&lexer->tokens);
  lexer->error = NULL;

  return lexer;
}

void lexer_free(Lexer *lexer) {
  free(lexer->tokens.items);
  free(lexer->error);
  free(lexer);
}

static void lexer_emit(Token *token, TokenType type, const char *start,
                       size_t length, Position position) {
  token->type = type;
  token->start = start;
  token->length = length;
  token->position = position;
}

static int lex_identifier(Lexer *lexer, Position position, const char *text,
                          size_t text_len, size_t start, size_t *consumed) {
  size_t i = start;

  while (i < text_len && (isalnum((unsigned char)text[i]) || text[i] == '_')) {
    i++;
  }

  *consumed = i - start;

  Token token;

  TokenType type = TOKEN_SYMBOL;
  if (lexer->config.keywords != NULL && lexer->config.keyword_count > 0) {
    for (size_t keyword_idx = 0; keyword_idx < lexer->config.keyword_count; keyword_idx++) {
      const char *current = lexer->config.keywords[keyword_idx];
      size_t keyword_len = strlen(current);
      if (keyword_len == *consumed &&
          memcmp(current, text + start, keyword_len) == 0) {
        type = TOKEN_KEYWORD;
        break;
      }
    }
  }

  lexer_emit(&token, type, text + start, *consumed, position);

  if (da_add(&lexer->tokens, token) != 0) {
    emit_error(lexer, LEXER_ERROR_ALLOC, position);
    return -1;
  }
  return 0;
}

static int lex_number(Lexer *lexer, Position position, const char *text,
                      size_t text_len, size_t start, size_t *consumed) {
  size_t i = start;

  while (i < text_len && isdigit((unsigned char)text[i])) {
    i++;
  }

  *consumed = i - start;

  Token token;
  lexer_emit(&token, TOKEN_NUMBER, text + start, *consumed, position);

  if (da_add(&lexer->tokens, token) != 0) {
    emit_error(lexer, LEXER_ERROR_ALLOC, position);
    return -1;
  }
  return 0;
}

static int lex_punct(Lexer *lexer, Position position, const char *text,
                     size_t text_len, size_t start, size_t *consumed) {
  size_t best_len = 0;

  for (size_t punct_idx = 0; punct_idx < lexer->config.punct_count; punct_idx++) {
    const char *current = lexer->config.puncts[punct_idx];
    size_t punct_len = strlen(current);
    if (start + punct_len <= text_len && punct_len > best_len &&
        memcmp(current, text + start, punct_len) == 0) {
      best_len = punct_len;
    }
  }

  if (best_len > 0) {
    *consumed = best_len;
    Token token;
    lexer_emit(&token, TOKEN_PUNCT, text + start, best_len, position);
    if (da_add(&lexer->tokens, token) != 0) {
      emit_error(lexer, LEXER_ERROR_ALLOC, position);
      return -1;
    }
    return 0;
  }

  emit_error(lexer, LEXER_ERROR_ILLEGAL_CHAR, position);
  return -1;
}

int lex(Lexer *lexer, const char *text, size_t text_len) {
  int col = 1;
  int line = 1;

  for (size_t i = 0; i < text_len; i++) {
    if (text[i] == '\n') {
      line++;
      col = 1;
      continue;
    }

    if (isspace((unsigned char)text[i])) {
      col++;
      continue;
    }


    if (lexer->config.comment != NULL &&
        i + lexer->config.comment_len <= text_len &&
        memcmp(text + i, lexer->config.comment, lexer->config.comment_len) == 0) {
      while (i < text_len && text[i] != '\n') {
        i++;
        col++;
      }

      i--;
      continue;
    }

    Position position = {line, col};

    if (isalpha((unsigned char)text[i]) || text[i] == '_') {
      size_t consumed = 0;
      if (lex_identifier(lexer, position, text, text_len, i, &consumed) != 0) {
        return -1;
      }

      col += consumed;
      i += consumed - 1;
    } else if (isdigit((unsigned char)text[i])) {
      size_t consumed = 0;
      if (lex_number(lexer, position, text, text_len, i, &consumed) != 0) {
        return -1;
      }

      col += consumed;
      i += consumed - 1;
    } else if (ispunct((unsigned char)text[i])) {
      size_t consumed = 0;
      if (lex_punct(lexer, position, text, text_len, i, &consumed) != 0) {
        return -1;
      }

      col += consumed;
      i += consumed - 1;
    } else {
      emit_error(lexer, LEXER_ERROR_ILLEGAL_CHAR, position);
      return -1;
    }
  }

  Position pos = {line, col};

  Token eof;

  lexer_emit(&eof, TOKEN_EOF, NULL, 0, pos);

  if (da_add(&lexer->tokens, eof) != 0) {
    emit_error(lexer, LEXER_ERROR_ALLOC, pos);
    return -1;
  }

  if (lexer->tokens.count < lexer->tokens.capacity) {
    Token *shrunk = realloc(lexer->tokens.items, lexer->tokens.count * sizeof(Token));
    if (shrunk) {
      lexer->tokens.items = shrunk;
      lexer->tokens.capacity = lexer->tokens.count;
    }
  }

  return 0;
}

#endif // LEXER_IMPLEMENTATION
