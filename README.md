# Lexer

A small, single-file tokenizer for C. Give it some text and a few simple
rules, and it splits the text into tokens (symbols, punctuation, comments).

## Features

- Single header file
- Configurable keywords, punctuation, and single-line comments.
- Tracks the line and column of every token.
- Keeps error state which can be used for error handling or debugging.
- No dependencies beyond the C standard library.

## Getting started

Copy `lexer.h` into your project and include it once with
`LEXER_IMPLEMENTATION` defined so the implementation gets compiled:

```c
#define LEXER_IMPLEMENTATION
#include "lexer.h"
```

Build with any C compiler, e.g.:

```sh
cc -o my_program my_program.c
```

## Quick example

This tokenizes a tiny piece of code:

```c
#define LEXER_IMPLEMENTATION
#include "lexer.h"
#include <string.h>
#include <stdio.h>

int main(void) {
  const char *keywords[3] = {"return", "int", "void"};
  const char *puncts[6] = {"=", ";", "(", ")", "{", "}"};
  const char *comment = "//";

  LexerConfig config = {keywords, 3, puncts, 6, comment, 2};
  Lexer *lexer = lexer_init(config);

  const char *text = "int x = 42; // a comment";

  if (lex(lexer, text, strlen(text)) != 0) {
    fprintf(stderr, "Lexing failed: %d\n", lexer->error->code);
    lexer_free(lexer);
    return 1;
  }

  for (size_t i = 0; i < lexer->tokens.count; i++) {
    const Token *t = &lexer->tokens.items[i];
    printf("(%d, %d) %s: %.*s\n", t->position.line, t->position.col,
           token_type_to_string(t->type), (int)t->length,
           t->start ? t->start : "");
  }

  lexer_free(lexer);
  return 0;
}
```

Output:

```
(1, 1) TOKEN_KEYWORD: int
(1, 5) TOKEN_SYMBOL: x
(1, 7) TOKEN_PUNCT: =
(1, 9) TOKEN_NUMBER: 42
(1, 11) TOKEN_PUNCT: ;
(1, 25) TOKEN_EOF:
```

## Configuration

The `LexerConfig` struct controls how the text is tokenized:

| Field          | Purpose                                              |
| -------------- | ---------------------------------------------------- |
| `keywords`     | Words to recognize as keywords (or `NULL`)           |
| `keyword_count`| Number of keywords                                   |
| `puncts`       | Punctuation sequences to recognize (or `NULL`)       |
| `punct_count`  | Number of punctuation entries                        |
| `comment`      | Comment marker, e.g. `//`, `--`, `#` (or `NULL`)     |
| `comment_len`  | Length of the comment marker                         |

Every token has a type, the position where it starts, and a pointer to the
original text. The token types are:

- `TOKEN_SYMBOL` — identifiers and names
- `TOKEN_NUMBER` — sequences of digits
- `TOKEN_KEYWORD` — words from your keyword list
- `TOKEN_PUNCT` — punctuation from your list
- `TOKEN_EOF` — end of input

Punctuation uses longest-match: if both `-` and `->` are configured, `->`
wins. Comment markers also work with multiple characters.

## Examples

Ready-to-run examples live in [`examples/`](examples/):

- `code/` — tokenizing a C file with keywords, punctuation, and `//` comments.
- `double_chars/` — showing off multi-character punctuation like `->`.
- `lorem/` — tokenizing plain text.

## License

[MIT](LICENSE)
