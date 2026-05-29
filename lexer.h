#pragma once
#include <stddef.h>
#include <stdio.h>

typedef struct {
	int x;
	int y;
} Vector2;

typedef enum {
	TOKEN_SYMBOL,
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
	Token **items; // consider *items here, limiting to one allocation per token 
	size_t capacity;
	size_t count;
} DynamicArray;

// TODO: decouple lexer config from lexer state
typedef struct {
	const char **keywords;
	size_t keyword_count;
	const char **puncts;
	size_t punct_count;
	// TODO: add support for single-line comments
	DynamicArray *tokens;
	// TODO: add error object instead of using stderr
} Lexer;

#ifdef LEXER_IMPLEMENTATION 
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static DynamicArray *da_init() 
{
	DynamicArray *da = malloc(sizeof(DynamicArray));
	if (da == NULL)
	{
		fprintf(stderr, "Failed to allocate dynamic array\n");
		return NULL;
	}

	size_t count = 0;
	size_t capacity = 16;
	Token **items = malloc(capacity * sizeof(Token*));

	if (items == NULL)
	{
		fprintf(stderr, "Failed to allocate dynamic array items\n");
		free(da);
		return NULL;
	}

	da->count = count;
	da->capacity = capacity;
	da->items = items;

	return da;
}

static int da_add(DynamicArray *da, Token *item)
{
	if (da->count == da->capacity)
	{
		size_t new_capacity = da->capacity * 2;
		Token **new_items = realloc(da->items, new_capacity * sizeof(Token*));
		if (new_items == NULL)
		{
			fprintf(stderr, "Failed to reallocate dynamic array\n");
			free(item);
			return -1;
		}

		da->items = new_items;
		da->capacity = new_capacity;
	}

	da->items[da->count++] = item;

	return 0;
}

static void da_free(DynamicArray *da) 
{
	for (size_t i = 0; i < da->count; i++)
	{
		free(da->items[i]->lexeme);	
		free(da->items[i]);	
	}
	free(da->items);
	free(da);
}

// TODO: add token_type_to_string_helper

Lexer *lexer_init()
{
	Lexer *lexer = malloc(sizeof(Lexer));
	if (lexer == NULL)
	{
		fprintf(stderr, "Failed to allocate lexer\n");
		return NULL;
	}
	
	lexer->keywords = NULL;
	lexer->keyword_count = 0;
	lexer->puncts = NULL;
	lexer->punct_count = 0;

	DynamicArray* tokens = da_init();
	if (!tokens)
	{
		free(lexer);
		return NULL;
	}
	lexer->tokens = tokens;

	return lexer;
}

void lexer_free(Lexer *lexer)
{
	da_free(lexer->tokens);
	free(lexer);
}

/*
 * TODO: implement following functions instead of having a monolithic lexer loop
 * static Token *lex_identifier(...)
 * static Token *lex_number(...)
 * static Token *lex_punct(...)
*/

static void lexer_emit(Token *token, TokenType type, char *lexeme, Vector2 position)
{
	token->type = type;
	token->lexeme = lexeme;
	token->position = position;
}

int lex(Lexer *lexer, const char *text) 
{
	int x_pos = 1;
	int y_pos = 1;

	size_t text_len = strlen(text);

	for (size_t i = 0; i < text_len; i++)
	{
		if (i < text_len && text[i] == '\n')
		{
			y_pos++;
			x_pos = 1;
			continue;
		}

		if (i < text_len && isspace((unsigned char)text[i]))
		{
			x_pos++;
			continue;
		}

		Token* token = malloc(sizeof(Token));
		if (token == NULL)
		{
			fprintf(stderr, "Failed to allocate token\n");
			return -1;
		}

		Vector2 position = { x_pos, y_pos };

		// TODO: distinguish identifiers from numbers/strings
		// TODO: add support for '_' characters (requires separate branch for numbers)
		if (i < text_len && isalnum((unsigned char)text[i]))
		{
			size_t current_count = 0;
			size_t current_capacity = 16;
			char *current = malloc(current_capacity * sizeof(char));
		
			if (!current)
			{
				fprintf(stderr, "Failed to allocate string\n");
				free(token);
				return -1;
			}

			while (i < text_len && isalnum((unsigned char)text[i]))
			{
				if (current_count == current_capacity)
				{
					current_capacity = current_capacity * 2;
					
					char *tmp = realloc(current, current_capacity);
					if (tmp == NULL)
					{
						fprintf(stderr, "Failed to reallocate string\n");
						free(current);
						free(token);
						return -1;
					}

					current = tmp; 
				}

				current[current_count++] = text[i++];
				current[current_count] = '\0';
				x_pos++;
			}

			i--;	// compensate for the while loop skipping one character

			if (lexer->keywords != NULL && lexer->keyword_count > 0)
			{
				bool is_keyword = false;
				for (size_t kw_idx = 0; kw_idx < lexer->keyword_count; kw_idx++)
				{
					if (strcmp(lexer->keywords[kw_idx], current) == 0)
					{
						is_keyword = true;
						break;
					}
				}

				if (is_keyword)
				{
					lexer_emit(token, TOKEN_KEYWORD, current, position);

					if(da_add(lexer->tokens, token) != 0)
					{
						free(current);
						free(token);
						return -1;
					}
				}
				else
				{
					lexer_emit(token, TOKEN_SYMBOL, current, position);

					if(da_add(lexer->tokens, token) != 0)
					{
						free(current);
						free(token);
						return -1;
					}
				}
			}
			else
			{
				lexer_emit(token, TOKEN_SYMBOL, current, position);

				if(da_add(lexer->tokens, token) != 0)
				{
					free(current);
					free(token);
					return -1;
				}
			}
		} 
		else if (i < text_len && ispunct((unsigned char)text[i]))
		{
			/* 
			 * TODO: longest-match punctuation
			 * Currently, this works for preserving tokens such as '==' or '->'
			 * But passing something such as '()' will break it
			*/
			size_t current_count = 0;
			size_t current_capacity = 4;
			char *current = malloc(current_capacity * sizeof(char));

			if (current == NULL)
			{
				fprintf(stderr, "Failed to allocate string\n");
				free(token);
				return -1;
			}

			while (i < text_len && ispunct((unsigned char)text[i]))
			{
				if (current_count == current_capacity)
				{
					current_capacity = current_capacity * 2;
				
					char *tmp = realloc(current, current_capacity);
					if (tmp == NULL)
					{
						fprintf(stderr, "Failed to reallocate string\n");
						free(current);
						free(token);
						return -1;
					}
			
					current = tmp; 
				}

				current[current_count++] = text[i++];
				current[current_count] = '\0';
				x_pos++;
			}
			
			i--;

			bool is_char = false;

			for (size_t punct_idx = 0; punct_idx < lexer->punct_count; punct_idx++)
			{
				if (strcmp(lexer->puncts[punct_idx], current) == 0)
				{
					is_char = true;
					break;
				}
			}

			if (is_char) 
			{
				lexer_emit(token, TOKEN_PUNCT, current, position);
					
				if(da_add(lexer->tokens, token) != 0)
				{
					free(current);
					free(token);
					return -1;
				}
			}
			else 
			{
				fprintf(stderr, "Illegal character sequence: %s\n", current);
				free(current);
				free(token);
				return -1;
			}
		} 
		else
		{
			fprintf(stderr, "Illegal character: %c\n", text[i]);
			free(token);
			return -1;
		}
	}

	Token *eof = malloc(sizeof(Token));

	if (!eof)
		return -1;

	Vector2 pos = { x_pos, y_pos };

	lexer_emit(eof, TOKEN_EOF, NULL, pos);

	if (da_add(lexer->tokens, eof) != 0)
	{
		free(eof);
		return -1;
	}

	return 0;
}

#endif
