#define LEXER_IMPLEMENTATION
#include "../../lexer.h"
#include "../file.h"

const char *keywords[3] = {"return", "int", "void"};
const char *puncts[6] = {"=", ";", "(", ")", "{", "}"};
const char *comment = "//";

int main(void) {
  char *file_name = "code.c";
  char *text = open_file(file_name);
  if (text == NULL) {
    fprintf(stderr, "Failed to open file %s\n", file_name);
    exit(1);
  }

  LexerConfig config = {keywords, 3, puncts, 6, comment, 2};
  Lexer *lexer = lexer_init(config);
  if (lexer == NULL) {
    fprintf(stderr, "Failed to initialize lexer\n");
    free(text);
    exit(1);
  }

  if (lex(lexer, text, strlen(text)) != 0) {
    fprintf(stderr, "Failed to tokenize text: %d\n", lexer->error->code);
    lexer_free(lexer);
    free(text);
    exit(1);
  }

  for (size_t i = 0; i < lexer->tokens.count; i++) {
    const Token *t = &lexer->tokens.items[i];
    printf("(%d, %d): %s: %.*s\n", t->position.col, t->position.line,
           token_type_to_string(t->type), (int)t->length,
           t->start ? t->start : "");
  }

  lexer_free(lexer);
  free(text);
  return 0;
}
