#pragma once
#include <stddef.h>
#include <stdio.h>

typedef struct {
  int x;
  int y;
} Vector2;

typedef enum {
  TOKEN_SYMBOL,
  TOKEN_NUMBER,
  TOKEN_KEYWORD,
  TOKEN_PUNCT,
  TOKEN_EOF
} TokenType;

typedef struct {
  TokenType type;
  // NOTE: lexeme may be replaced with spans (start, length)
  char *lexeme; // NULL for EOF token
  Vector2 position;
} Token;

typedef struct {
  Token *items;
  size_t capacity;
  size_t count;
} DynamicArray;

typedef struct {
  char *data;
  size_t capacity;
  size_t offset;
} Arena;

// TODO: decouple lexer config from lexer state
typedef struct {
  const char **keywords;
  size_t keyword_count;
  const char **puncts;
  size_t punct_count;
  // TODO: add support for single-line comments
  DynamicArray tokens;
  Arena arena;
  // TODO: add error object instead of using stderr
} Lexer;

#ifdef LEXER_IMPLEMENTATION
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void da_init(DynamicArray *da) {
  da->count = 0;
  da->capacity = 16;
  da->items = malloc(da->capacity * sizeof(Token));
}

static int da_add(DynamicArray *da, Token item) {
  if (da->count == da->capacity) {
    size_t new_capacity = da->capacity + da->capacity / 2;
    Token *new_items = realloc(da->items, new_capacity * sizeof(Token));
    if (new_items == NULL) {
      fprintf(stderr, "Failed to reallocate dynamic array\n");
      return -1;
    }

    da->items = new_items;
    da->capacity = new_capacity;
  }

  da->items[da->count++] = item;

  return 0;
}

static void arena_init(Arena *arena, size_t capacity) {
  arena->data = malloc(capacity);
  arena->capacity = arena->data ? capacity : 0;
  arena->offset = 0;
}

static char *arena_alloc(Arena *arena, size_t size) {
  if (arena->offset + size > arena->capacity) {
    return NULL;
  }

  char *ptr = arena->data + arena->offset;
  arena->offset += size;
  return ptr;
}

static void arena_free(Arena *arena) { free(arena->data); }

static const char *token_type_to_string(TokenType type) {
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

Lexer *lexer_init() {
  Lexer *lexer = malloc(sizeof(Lexer));
  if (lexer == NULL) {
    fprintf(stderr, "Failed to allocate lexer\n");
    return NULL;
  }

  lexer->keywords = NULL;
  lexer->keyword_count = 0;
  lexer->puncts = NULL;
  lexer->punct_count = 0;

  da_init(&lexer->tokens);
  lexer->arena.data = NULL;
  lexer->arena.capacity = 0;
  lexer->arena.offset = 0;

  return lexer;
}

void lexer_free(Lexer *lexer) {
  free(lexer->tokens.items);
  arena_free(&lexer->arena);
  free(lexer);
}

static void lexer_emit(Token *token, TokenType type, char *lexeme,
                       Vector2 position) {
  token->type = type;
  token->lexeme = lexeme;
  token->position = position;
}

static int lex_identifier(Lexer *lexer, Vector2 position, const char *text,
                          size_t text_len, size_t start, size_t *consumed) {
  size_t i = start;

  while (i < text_len && (isalpha((unsigned char)text[i]) || text[i] == '_')) {
    i++;
  }

  *consumed = i - start;

  Token token;

  if (lexer->keywords != NULL && lexer->keyword_count > 0) {
    bool is_keyword = false;
    for (size_t kw_idx = 0; kw_idx < lexer->keyword_count; kw_idx++) {
      const char *kw = lexer->keywords[kw_idx];
      size_t kw_len = strlen(kw);
      if (kw_len == *consumed && memcmp(kw, text + start, *consumed) == 0) {
        is_keyword = true;
        break;
      }
    }

    if (is_keyword) {
      char *lexeme = arena_alloc(&lexer->arena, *consumed + 1);
      if (!lexeme) {
        return -1;
      }

      memcpy(lexeme, text + start, *consumed);
      lexeme[*consumed] = '\0';
      lexer_emit(&token, TOKEN_KEYWORD, lexeme, position);

      if (da_add(&lexer->tokens, token) != 0) {
        return -1;
      }
    } else {
      char *lexeme = arena_alloc(&lexer->arena, *consumed + 1);
      if (!lexeme) {
        return -1;
      }

      memcpy(lexeme, text + start, *consumed);
      lexeme[*consumed] = '\0';
      lexer_emit(&token, TOKEN_SYMBOL, lexeme, position);

      if (da_add(&lexer->tokens, token) != 0) {
        return -1;
      }
    }
  } else {
    char *lexeme = arena_alloc(&lexer->arena, *consumed + 1);
    if (!lexeme) {
      return -1;
    }

    memcpy(lexeme, text + start, *consumed);
    lexeme[*consumed] = '\0';
    lexer_emit(&token, TOKEN_SYMBOL, lexeme, position);

    if (da_add(&lexer->tokens, token) != 0) {
      return -1;
    }
  }

  return 0;
}

static int lex_number(Lexer *lexer, Vector2 position, const char *text,
                      size_t text_len, size_t start, size_t *consumed) {
  size_t i = start;

  while (i < text_len && isdigit((unsigned char)text[i])) {
    i++;
  }

  *consumed = i - start;

  Token token;

  char *lexeme = arena_alloc(&lexer->arena, *consumed + 1);
  if (!lexeme) {
    return -1;
  }

  memcpy(lexeme, text + start, *consumed);
  lexeme[*consumed] = '\0';
  lexer_emit(&token, TOKEN_NUMBER, lexeme, position);

  if (da_add(&lexer->tokens, token) != 0) {
    return -1;
  }

  return 0;
}

static int lex_punct(Lexer *lexer, Vector2 position, const char *text,
                     size_t text_len, size_t start, size_t *consumed) {
  /*
   * TODO: longest-match punctuation
   * Currently, this works for preserving tokens such as '==' or '->'
   * But passing something such as '()' will break it
   */
  size_t i = start;

  while (i < text_len && ispunct((unsigned char)text[i])) {
    i++;
  }

  *consumed = i - start;

  Token token;

  bool is_char = false;

  for (size_t punct_idx = 0; punct_idx < lexer->punct_count; punct_idx++) {
    const char *punct = lexer->puncts[punct_idx];
    size_t punct_len = strlen(punct);
    if (punct_len == *consumed && memcmp(punct, text + start, *consumed) == 0) {
      is_char = true;
      break;
    }
  }

  if (is_char) {
    char *lexeme = arena_alloc(&lexer->arena, *consumed + 1);
    if (!lexeme) {
      return -1;
    }

    memcpy(lexeme, text + start, *consumed);
    lexeme[*consumed] = '\0';
    lexer_emit(&token, TOKEN_PUNCT, lexeme, position);

    if (da_add(&lexer->tokens, token) != 0) {
      return -1;
    }
  } else {
    fprintf(stderr, "Illegal character sequence: %.*s\n", (int)*consumed,
            text + start);
    return -1;
  }

  return 0;
}

int lex(Lexer *lexer, const char *text) {
  int x_pos = 1;
  int y_pos = 1;

  size_t text_len = strlen(text);

  arena_free(&lexer->arena);
  arena_init(&lexer->arena, text_len * 2 + 1);
  if (!lexer->arena.data) {
    return -1;
  }

  for (size_t i = 0; i < text_len; i++) {
    if (text[i] == '\n') {
      y_pos++;
      x_pos = 1;
      continue;
    }

    if (isspace((unsigned char)text[i])) {
      x_pos++;
      continue;
    }

    Vector2 position = {x_pos, y_pos};

    if (isalpha((unsigned char)text[i]) || text[i] == '_') {
      size_t consumed = 0;
      if (lex_identifier(lexer, position, text, text_len, i, &consumed) != 0) {
        return -1;
      }

      x_pos += consumed;
      i += consumed - 1;
    } else if (isdigit((unsigned char)text[i])) {
      size_t consumed = 0;
      if (lex_number(lexer, position, text, text_len, i, &consumed) != 0) {
        return -1;
      }

      x_pos += consumed;
      i += consumed - 1;
    } else if (ispunct((unsigned char)text[i])) {
      size_t consumed = 0;
      if (lex_punct(lexer, position, text, text_len, i, &consumed) != 0) {
        return -1;
      }

      x_pos += consumed;
      i += consumed - 1;
    } else {
      fprintf(stderr, "Illegal character: %c\n", text[i]);
      return -1;
    }
  }

  Vector2 pos = {x_pos, y_pos};

  Token eof;

  lexer_emit(&eof, TOKEN_EOF, NULL, pos);

  if (da_add(&lexer->tokens, eof) != 0)
    return -1;

  if (lexer->tokens.count < lexer->tokens.capacity) {
    Token *shrunk =
        realloc(lexer->tokens.items, lexer->tokens.count * sizeof(Token));
    if (shrunk) {
      lexer->tokens.items = shrunk;
      lexer->tokens.capacity = lexer->tokens.count;
    }
  }

  return 0;
}

#endif // LEXER_IMPLEMENTATION
