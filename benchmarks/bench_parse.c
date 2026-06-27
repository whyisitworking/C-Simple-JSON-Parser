#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "json.h"

static char *read_file(const char *path, size_t *len_out) {
  FILE *file;
  long len;
  char *buffer;
  size_t read_len;

  file = fopen(path, "rb");
  if (file == NULL) {
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
  if (len_out != NULL) {
    *len_out = (size_t)len;
  }
  return buffer;
}

static int bench_file(const char *path, int iterations) {
  char *text;
  size_t len;
  int i;
  clock_t start;
  clock_t end;
  double seconds;
  double mb;
  json_status st;
  json_error err;
  json_root root;

  text = read_file(path, &len);
  if (text == NULL) {
    printf("%s: skipped\n", path);
    return 1;
  }

  start = clock();
  for (i = 0; i < iterations; i++) {
    st = json_parse_len(text, len, NULL, &root, &err);
    if (st != JSON_OK) {
      printf("%s: parse failed at %lu: %s\n", path, (unsigned long)err.offset,
             json_error_to_string(st));
      free(text);
      return 1;
    }
    json_free(&root);
  }
  end = clock();

  seconds = (double)(end - start) / (double)CLOCKS_PER_SEC;
  mb = ((double)len * (double)iterations) / (1024.0 * 1024.0);
  printf("%s: %.2f MB in %.4fs = %.2f MB/s\n", path, mb, seconds,
         seconds > 0.0 ? mb / seconds : 0.0);

  free(text);
  return 0;
}

int main(int argc, char **argv) {
  int iterations;
  int failed;
  char *end;
  long val;

  iterations = 1000;
  if (argc > 1) {
    end = NULL;
    val = strtol(argv[1], &end, 10);
    if (end == argv[1] || val <= 0 || val > 100000L) {
      fprintf(stderr, "Usage: %s <iterations>\n", argv[0]);
      return 1;
    }
    iterations = (int)val;
  }

  failed = 0;
  failed += bench_file("sample/simple.json", iterations);
  failed += bench_file("sample/multidim_arr.json", iterations);
  failed += bench_file("sample/random.json", iterations);
  failed += bench_file("sample/food.json", iterations);
  failed += bench_file("sample/rickandmorty.json", iterations);
  failed += bench_file("sample/reddit.json", iterations);
  return failed == 0 ? 0 : 1;
}
