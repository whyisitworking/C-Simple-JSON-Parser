#include <stdio.h>
#include <stdlib.h>

#include "json.h"

static char *read_file(const char *path) {
  FILE *file;
  long len;
  char *buffer;
  size_t read_len;

  file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Expected file \"%s\" not found\n", path);
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  len = ftell(file);
  if (len < 0) {
    fclose(file);
    return NULL;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }

  buffer = (char *)malloc((size_t)len + 1u);
  if (buffer == NULL) {
    fclose(file);
    return NULL;
  }

  read_len = fread(buffer, 1u, (size_t)len, file);
  fclose(file);
  if (read_len != (size_t)len) {
    free(buffer);
    return NULL;
  }
  buffer[len] = '\0';
  return buffer;
}

int main(void) {
  char *text;
  json_root root;
  json_error error;
  json_status status;
  const json_value *status_value;
  const char *str;
  size_t len;
  long a;
  int b;

  text = read_file("sample/simple.json");
  if (text == NULL) {
    return 1;
  }

  status = json_parse(text, &root, &error);
  free(text);
  if (status != JSON_OK) {
    fprintf(stderr, "Parse failed at byte %lu: %s\n",
            (unsigned long)error.offset, json_error_to_string(status));
    return 1;
  }

  status_value = json_object_get(&root.value, "status");
  if (json_string_get(status_value, &str, &len) == JSON_OK) {
    printf("status: %.*s\n", (int)len, str);
  }

  if (json_path_get_string(&root.value, "status", &str, &len) == JSON_OK) {
    printf("path status: %.*s\n", (int)len, str);
  }

  if (json_path_get_long(&root.value, "data.a", &a) == JSON_OK) {
    printf("data.a: %ld\n", a);
  }

  if (json_path_get_bool(&root.value, "data.b", &b) == JSON_OK) {
    printf("data.b: %s\n", b ? "true" : "false");
  }

  json_free(&root);
  return 0;
}
