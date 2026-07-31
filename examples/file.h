#pragma once
#include <stdio.h>
#include <stdlib.h>

static char *open_file(const char *file_path) {
  FILE *file = fopen(file_path, "r");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *content = (char*)malloc(size + 1);
  fread(content, 1, size, file);
  content[size] = '\0';

  fclose(file);
  return content;
}
