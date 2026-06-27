#ifndef CSJP_JSON_H
#define CSJP_JSON_H

#include <stddef.h>

typedef enum json_type {
  JSON_NULL = 0,
  JSON_BOOL,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT
} json_type;

typedef enum json_status {
  JSON_OK = 0,
  JSON_EMPTY_INPUT,
  JSON_TRAILING_INPUT,
  JSON_EXPECTED_VALUE,
  JSON_EXPECTED_KEY,
  JSON_EXPECTED_COLON,
  JSON_EXPECTED_COMMA_OR_END,
  JSON_INVALID_LITERAL,
  JSON_INVALID_NUMBER,
  JSON_INVALID_STRING,
  JSON_INVALID_ESCAPE,
  JSON_INVALID_UNICODE,
  JSON_DEPTH_LIMIT,
  JSON_ALLOCATION_FAILED,
  JSON_WRONG_TYPE,
  JSON_NOT_FOUND,
  JSON_INVALID_PATH
} json_status;

typedef struct json_error {
  json_status code;
  size_t offset;
} json_error;

typedef struct json_string {
  const char *data;
  size_t len;
} json_string;

typedef struct json_number {
  const char *text;
  size_t len;
  double as_double;
  long as_long;
  int is_long;
} json_number;

typedef struct json_value json_value;

typedef struct json_array {
  size_t count;
  json_value *items;
} json_array;

typedef struct json_member {
  json_string key;
  json_value *value;
} json_member;

typedef struct json_object {
  size_t count;
  json_member *members;
} json_object;

typedef struct json_allocator {
  void *ctx;
  void *(*malloc_fn)(void *ctx, size_t size);
  void *(*realloc_fn)(void *ctx, void *ptr, size_t old_size, size_t new_size);
  void (*free_fn)(void *ctx, void *ptr, size_t size);
} json_allocator;

typedef struct json_config {
  size_t max_depth;
  json_allocator allocator;
} json_config;

struct json_value {
  json_type type;
  union {
    int boolean;
    json_number number;
    json_string string;
    json_array array;
    json_object object;
  } as;
};

typedef struct json_root {
  json_value value;
  void *arena;
} json_root;

#ifndef CSJP_API
#if defined(__GNUC__) || defined(__clang__)
#define CSJP_API static __attribute__((unused))
#else
#define CSJP_API static
#endif
#endif

CSJP_API json_status json_parse(const char *text, json_root *out,
                                json_error *err);
CSJP_API json_status json_parse_len(const char *text, size_t len,
                                    const json_config *cfg, json_root *out,
                                    json_error *err);
CSJP_API void json_free(json_root *root);
CSJP_API const char *json_error_to_string(json_status code);
CSJP_API const json_value *json_root_value(const json_root *root);

CSJP_API json_type json_type_get(const json_value *value);
CSJP_API json_status json_bool_get(const json_value *value, int *out);
CSJP_API json_status json_string_get(const json_value *value,
                                     const char **data, size_t *len);
CSJP_API json_status json_number_get(const json_value *value, const char **text,
                                     size_t *len, double *as_double,
                                     long *as_long, int *is_long);
CSJP_API json_status json_number_get_double(const json_value *value,
                                            double *out);
CSJP_API json_status json_number_get_long(const json_value *value, long *out);

CSJP_API size_t json_array_size(const json_value *array);
CSJP_API const json_value *json_array_get(const json_value *array,
                                          size_t index);

CSJP_API size_t json_object_size(const json_value *object);
CSJP_API const char *json_object_key_at(const json_value *object, size_t index,
                                        size_t *len);
CSJP_API const json_value *json_object_value_at(const json_value *object,
                                                size_t index);
CSJP_API const json_value *json_object_member_at(const json_value *object,
                                                  size_t index,
                                                  const char **key,
                                                  size_t *key_len);
CSJP_API const json_value *json_object_get(const json_value *object,
                                           const char *key);
CSJP_API const json_value *json_object_get_len(const json_value *object,
                                               const char *key, size_t len);

CSJP_API const json_value *json_path_get(const json_value *root,
                                         const char *path);
CSJP_API json_status json_path_get_bool(const json_value *root,
                                        const char *path, int *out);
CSJP_API json_status json_path_get_string(const json_value *root,
                                          const char *path, const char **data,
                                          size_t *len);
CSJP_API json_status json_path_get_long(const json_value *root,
                                        const char *path, long *out);
CSJP_API json_status json_path_get_double(const json_value *root,
                                          const char *path, double *out);

/* ---- implementation (internal) ---- */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef JSON_DEFAULT_MAX_DEPTH
#define JSON_DEFAULT_MAX_DEPTH 256u
#endif

#ifndef JSON_ARENA_CHUNK_SIZE
#define JSON_ARENA_CHUNK_SIZE 4096u
#endif

typedef struct json__chunk {
  struct json__chunk *next;
  size_t used;
  size_t cap;
  char data[1];
} json__chunk;

typedef struct json__arena {
  json_allocator allocator;
  json__chunk *chunks;
} json__arena;

typedef struct json__parser {
  const char *src;
  const char *cur;
  const char *end;
  json__arena *arena;
  json_config cfg;
  json_error error;
} json__parser;

/* ---- allocator ---- */

static void *json__default_malloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void *json__default_realloc(void *ctx, void *ptr, size_t old_size,
                                   size_t new_size) {
  (void)ctx;
  (void)old_size;
  return realloc(ptr, new_size);
}

static void json__default_free(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

static json_allocator json__default_allocator(void) {
  json_allocator a;
  a.ctx = NULL;
  a.malloc_fn = json__default_malloc;
  a.realloc_fn = json__default_realloc;
  a.free_fn = json__default_free;
  return a;
}

/* ---- arena ---- */

static size_t json__align_size(size_t size) {
  size_t align;
  size_t rem;
  align = sizeof(void *);
  rem = size % align;
  if (rem != 0u) {
    if (size > (size_t)(-1) - (align - rem)) {
      return 0;
    }
    size += align - rem;
  }
  return size;
}

static json__arena *json__arena_new(json_allocator allocator) {
  json__arena *arena;
  if (allocator.malloc_fn == NULL) {
    allocator = json__default_allocator();
  }
  arena = (json__arena *)allocator.malloc_fn(allocator.ctx, sizeof(*arena));
  if (arena == NULL) {
    return NULL;
  }
  arena->allocator = allocator;
  arena->chunks = NULL;
  return arena;
}

static void json__arena_destroy(json__arena *arena) {
  json__chunk *chunk;
  json__chunk *next;
  json_allocator allocator;
  size_t size;
  if (arena == NULL) {
    return;
  }
  allocator = arena->allocator;
  chunk = arena->chunks;
  while (chunk != NULL) {
    next = chunk->next;
    size = sizeof(json__chunk) + chunk->cap;
    allocator.free_fn(allocator.ctx, chunk, size);
    chunk = next;
  }
  allocator.free_fn(allocator.ctx, arena, sizeof(*arena));
}

static void *json__arena_alloc(json__arena *arena, size_t size) {
  json__chunk *chunk;
  size_t need;
  size_t cap;
  size_t total;
  void *ptr;
  if (arena == NULL) {
    return NULL;
  }
  need = json__align_size(size);
  if (need == 0u) {
    return NULL;
  }
  chunk = arena->chunks;
  if (chunk == NULL || chunk->cap - chunk->used < need) {
    cap = JSON_ARENA_CHUNK_SIZE;
    if (cap < need) {
      cap = need;
    }
    if (cap > (size_t)(-1) - sizeof(json__chunk)) {
      return NULL;
    }
    total = sizeof(json__chunk) + cap;
    chunk = (json__chunk *)arena->allocator.malloc_fn(arena->allocator.ctx,
                                                      total);
    if (chunk == NULL) {
      return NULL;
    }
    chunk->next = arena->chunks;
    chunk->used = 0u;
    chunk->cap = cap;
    arena->chunks = chunk;
  }
  ptr = (void *)(chunk->data + chunk->used);
  chunk->used += need;
  memset(ptr, 0, size);
  return ptr;
}

static char *json__arena_copy(json__arena *arena, const char *data,
                              size_t len) {
  char *copy;
  copy = (char *)json__arena_alloc(arena, len + 1u);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, data, len);
  copy[len] = '\0';
  return copy;
}

/* ---- value init ---- */

static void json__value_null(json_value *value) {
  memset(value, 0, sizeof(*value));
  value->type = JSON_NULL;
}

/* ---- parser utilities ---- */

static void json__set_error(json__parser *p, json_status code,
                            const char *where) {
  if (p->error.code == JSON_OK) {
    p->error.code = code;
    if (where == NULL) {
      p->error.offset = (size_t)(p->cur - p->src);
    } else {
      p->error.offset = (size_t)(where - p->src);
    }
  }
}

static int json__is_ws(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static void json__skip_ws(json__parser *p) {
  while (p->cur < p->end && json__is_ws(*p->cur)) {
    p->cur++;
  }
}

static int json__is_digit(char c) { return c >= '0' && c <= '9'; }

static int json__is_delim(const char *cur, const char *end) {
  if (cur >= end) {
    return 1;
  }
  return json__is_ws(*cur) || *cur == ',' || *cur == ']' || *cur == '}';
}

static int json__match(json__parser *p, const char *literal, size_t len) {
  if ((size_t)(p->end - p->cur) < len) {
    return 0;
  }
  return memcmp(p->cur, literal, len) == 0;
}

static int json__hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

/* ---- buffer helpers ---- */

static int json__grow_buffer(json__parser *p, char **buf, size_t *cap,
                             size_t needed) {
  char *next;
  size_t old_cap;
  size_t new_cap;
  old_cap = *cap;
  new_cap = old_cap == 0u ? 16u : old_cap;
  while (new_cap < needed) {
    if (new_cap > ((size_t)-1) / 2u) {
      json__set_error(p, JSON_ALLOCATION_FAILED, p->cur);
      return 0;
    }
    new_cap *= 2u;
  }
  if (new_cap == old_cap) {
    return 1;
  }
  next = (char *)p->cfg.allocator.realloc_fn(p->cfg.allocator.ctx, *buf,
                                             old_cap, new_cap);
  if (next == NULL) {
    json__set_error(p, JSON_ALLOCATION_FAILED, p->cur);
    return 0;
  }
  *buf = next;
  *cap = new_cap;
  return 1;
}

static json_status json__append_byte(json__parser *p, char **buf,
                                     size_t *len, size_t *cap, char ch) {
  if (!json__grow_buffer(p, buf, cap, *len + 2u)) {
    return JSON_ALLOCATION_FAILED;
  }
  (*buf)[*len] = ch;
  *len += 1u;
  return JSON_OK;
}

static json_status json__append_utf8(json__parser *p, char **buf,
                                     size_t *len, size_t *cap,
                                     unsigned long cp) {
  json_status st;
  st = JSON_OK;
  if (cp <= 0x7Ful) {
    st = json__append_byte(p, buf, len, cap, (char)cp);
  } else if (cp <= 0x7FFul) {
    if (!json__grow_buffer(p, buf, cap, *len + 3u)) {
      return JSON_ALLOCATION_FAILED;
    }
    (*buf)[(*len)++] = (char)(0xC0u | ((cp >> 6) & 0x1Fu));
    (*buf)[(*len)++] = (char)(0x80u | (cp & 0x3Fu));
  } else if (cp <= 0xFFFFul) {
    if (!json__grow_buffer(p, buf, cap, *len + 4u)) {
      return JSON_ALLOCATION_FAILED;
    }
    (*buf)[(*len)++] = (char)(0xE0u | ((cp >> 12) & 0x0Fu));
    (*buf)[(*len)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    (*buf)[(*len)++] = (char)(0x80u | (cp & 0x3Fu));
  } else if (cp <= 0x10FFFFul) {
    if (!json__grow_buffer(p, buf, cap, *len + 5u)) {
      return JSON_ALLOCATION_FAILED;
    }
    (*buf)[(*len)++] = (char)(0xF0u | ((cp >> 18) & 0x07u));
    (*buf)[(*len)++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    (*buf)[(*len)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    (*buf)[(*len)++] = (char)(0x80u | (cp & 0x3Fu));
  } else {
    json__set_error(p, JSON_INVALID_UNICODE, p->cur);
    st = JSON_INVALID_UNICODE;
  }
  return st;
}

static json_status json__append_raw_utf8(json__parser *p, char **buf,
                                         size_t *len, size_t *cap,
                                         const char **it) {
  const unsigned char *s;
  unsigned long cp;
  size_t need;
  size_t i;
  unsigned char first;
  s = (const unsigned char *)*it;
  first = s[0];
  if (first < 0x80u) {
    if ((char)first == '"' || (char)first == '\\' || first < 0x20u) {
      json__set_error(p, JSON_INVALID_STRING, *it);
      return JSON_INVALID_STRING;
    }
    if (!json__grow_buffer(p, buf, cap, *len + 2u)) {
      return JSON_ALLOCATION_FAILED;
    }
    (*buf)[(*len)++] = (char)first;
    *it += 1;
    return JSON_OK;
  }
  if (first >= 0xC2u && first <= 0xDFu) {
    need = 2u;
    cp = (unsigned long)(first & 0x1Fu);
  } else if (first >= 0xE0u && first <= 0xEFu) {
    need = 3u;
    cp = (unsigned long)(first & 0x0Fu);
  } else if (first >= 0xF0u && first <= 0xF4u) {
    need = 4u;
    cp = (unsigned long)(first & 0x07u);
  } else {
    json__set_error(p, JSON_INVALID_UNICODE, *it);
    return JSON_INVALID_UNICODE;
  }
  if ((size_t)(p->end - *it) < need) {
    json__set_error(p, JSON_INVALID_UNICODE, *it);
    return JSON_INVALID_UNICODE;
  }
  for (i = 1u; i < need; i++) {
    if ((s[i] & 0xC0u) != 0x80u) {
      json__set_error(p, JSON_INVALID_UNICODE, *it + i);
      return JSON_INVALID_UNICODE;
    }
    cp = (cp << 6) | (unsigned long)(s[i] & 0x3Fu);
  }
  if ((need == 3u && cp < 0x800ul) ||
      (need == 4u && cp < 0x10000ul) ||
      (cp >= 0xD800ul && cp <= 0xDFFFul) ||
      cp > 0x10FFFFul) {
    json__set_error(p, JSON_INVALID_UNICODE, *it);
    return JSON_INVALID_UNICODE;
  }
  if (!json__grow_buffer(p, buf, cap, *len + need + 1u)) {
    return JSON_ALLOCATION_FAILED;
  }
  for (i = 0u; i < need; i++) {
    (*buf)[(*len)++] = (char)s[i];
  }
  *it += need;
  return JSON_OK;
}

static json_status json__read_u4(json__parser *p, const char **it,
                                 unsigned long *out) {
  int i;
  int hv;
  unsigned long cp;
  cp = 0ul;
  if ((size_t)(p->end - *it) < 4u) {
    json__set_error(p, JSON_INVALID_UNICODE, *it);
    return JSON_INVALID_UNICODE;
  }
  for (i = 0; i < 4; i++) {
    hv = json__hex_value((*it)[i]);
    if (hv < 0) {
      json__set_error(p, JSON_INVALID_UNICODE, *it + i);
      return JSON_INVALID_UNICODE;
    }
    cp = (cp << 4) | (unsigned long)hv;
  }
  *it += 4;
  *out = cp;
  return JSON_OK;
}

/* ---- two-pass count ---- */

/* Counts top-level comma-separated items; skips strings, depth-tracks brackets. */
static size_t json__count_items(const char *cur, const char *end) {
  size_t depth;
  size_t count;
  char ch;
  char c2;
  depth = 0u;
  if (cur >= end || *cur == ']' || *cur == '}') {
    return 0u;
  }
  count = 1u;
  while (cur < end) {
    ch = *cur++;
    if (ch == '"') {
      while (cur < end) {
        c2 = *cur++;
        if (c2 == '\\') {
          if (cur < end) cur++;
        } else if (c2 == '"') {
          break;
        }
      }
    } else if (ch == '[' || ch == '{') {
      depth++;
    } else if (ch == ']' || ch == '}') {
      if (depth == 0u) break;
      depth--;
    } else if (ch == ',' && depth == 0u) {
      count++;
    }
  }
  return count;
}

/* ---- string parser ---- */

static json_status json__parse_string_raw(json__parser *p, json_string *out) {
  const char *it;
  char *buf;
  char *copy;
  size_t len;
  size_t cap;
  unsigned long cp;
  unsigned long low;
  json_status st = JSON_OK;
  char esc;
  if (p->cur >= p->end || *p->cur != '"') {
    json__set_error(p, JSON_INVALID_STRING, p->cur);
    return JSON_INVALID_STRING;
  }
  p->cur++;
  it = p->cur;
  buf = NULL;
  len = 0u;
  cap = 0u;
  while (it < p->end) {
    if (*it == '"') {
      it++;
      if (!json__grow_buffer(p, &buf, &cap, len + 1u)) {
        st = JSON_ALLOCATION_FAILED;
      } else {
        buf[len] = '\0';
        copy = json__arena_copy(p->arena, buf, len);
        if (copy == NULL) {
          json__set_error(p, JSON_ALLOCATION_FAILED, it);
          st = JSON_ALLOCATION_FAILED;
        } else {
          out->data = copy;
          out->len = len;
          p->cur = it;
          st = JSON_OK;
        }
      }
      goto cleanup;
    }
    if ((unsigned char)*it < 0x20u) {
      json__set_error(p, JSON_INVALID_STRING, it);
      st = JSON_INVALID_STRING;
      goto cleanup;
    }
    if (*it != '\\') {
      st = json__append_raw_utf8(p, &buf, &len, &cap, &it);
      if (st != JSON_OK) {
        goto cleanup;
      }
      continue;
    }
    it++;
    if (it >= p->end) {
      json__set_error(p, JSON_INVALID_ESCAPE, it);
      st = JSON_INVALID_ESCAPE;
      goto cleanup;
    }
    esc = *it++;
    switch (esc) {
    case '"':
    case '\\':
    case '/':
      st = json__append_byte(p, &buf, &len, &cap, esc);
      break;
    case 'b':
      st = json__append_byte(p, &buf, &len, &cap, '\b');
      break;
    case 'f':
      st = json__append_byte(p, &buf, &len, &cap, '\f');
      break;
    case 'n':
      st = json__append_byte(p, &buf, &len, &cap, '\n');
      break;
    case 'r':
      st = json__append_byte(p, &buf, &len, &cap, '\r');
      break;
    case 't':
      st = json__append_byte(p, &buf, &len, &cap, '\t');
      break;
    case 'u':
      st = json__read_u4(p, &it, &cp);
      if (st == JSON_OK) {
        if (cp >= 0xD800ul && cp <= 0xDBFFul) {
          if ((size_t)(p->end - it) < 6u || it[0] != '\\' || it[1] != 'u') {
            json__set_error(p, JSON_INVALID_UNICODE, it);
            st = JSON_INVALID_UNICODE;
          } else {
            it += 2;
            st = json__read_u4(p, &it, &low);
            if (st == JSON_OK) {
              if (low < 0xDC00ul || low > 0xDFFFul) {
                json__set_error(p, JSON_INVALID_UNICODE, it - 4);
                st = JSON_INVALID_UNICODE;
              } else {
                cp = 0x10000ul + (((cp - 0xD800ul) << 10) |
                                   (low - 0xDC00ul));
                st = json__append_utf8(p, &buf, &len, &cap, cp);
              }
            }
          }
        } else if (cp >= 0xDC00ul && cp <= 0xDFFFul) {
          json__set_error(p, JSON_INVALID_UNICODE, it - 4);
          st = JSON_INVALID_UNICODE;
        } else {
          st = json__append_utf8(p, &buf, &len, &cap, cp);
        }
      }
      break;
    default:
      json__set_error(p, JSON_INVALID_ESCAPE, it - 1);
      st = JSON_INVALID_ESCAPE;
      break;
    }
    if (st != JSON_OK) {
      goto cleanup;
    }
  }
  json__set_error(p, JSON_INVALID_STRING, p->end);
  st = JSON_INVALID_STRING;
cleanup:
  if (buf != NULL) {
    p->cfg.allocator.free_fn(p->cfg.allocator.ctx, buf, cap);
  }
  return st;
}

static json_status json__parse_string_value(json__parser *p, json_value *out) {
  json_status st;
  json_string s;
  s.data = NULL;
  s.len = 0u;
  st = json__parse_string_raw(p, &s);
  if (st != JSON_OK) {
    return st;
  }
  json__value_null(out);
  out->type = JSON_STRING;
  out->as.string = s;
  return JSON_OK;
}

/* ---- number parser ---- */

static json_status json__parse_number(json__parser *p, json_value *out) {
  const char *start;
  const char *it;
  char *copy;
  char *endptr;
  long lv;
  double dv;
  int is_integer;
  start = p->cur;
  it = p->cur;
  is_integer = 1;
  if (it < p->end && *it == '-') {
    it++;
  }
  if (it >= p->end) {
    json__set_error(p, JSON_INVALID_NUMBER, it);
    return JSON_INVALID_NUMBER;
  }
  if (*it == '0') {
    it++;
    if (it < p->end && json__is_digit(*it)) {
      json__set_error(p, JSON_INVALID_NUMBER, it);
      return JSON_INVALID_NUMBER;
    }
  } else if (*it >= '1' && *it <= '9') {
    while (it < p->end && json__is_digit(*it)) {
      it++;
    }
  } else {
    json__set_error(p, JSON_INVALID_NUMBER, it);
    return JSON_INVALID_NUMBER;
  }
  if (it < p->end && *it == '.') {
    is_integer = 0;
    it++;
    if (it >= p->end || !json__is_digit(*it)) {
      json__set_error(p, JSON_INVALID_NUMBER, it);
      return JSON_INVALID_NUMBER;
    }
    while (it < p->end && json__is_digit(*it)) {
      it++;
    }
  }
  if (it < p->end && (*it == 'e' || *it == 'E')) {
    is_integer = 0;
    it++;
    if (it < p->end && (*it == '+' || *it == '-')) {
      it++;
    }
    if (it >= p->end || !json__is_digit(*it)) {
      json__set_error(p, JSON_INVALID_NUMBER, it);
      return JSON_INVALID_NUMBER;
    }
    while (it < p->end && json__is_digit(*it)) {
      it++;
    }
  }
  if (!json__is_delim(it, p->end)) {
    json__set_error(p, JSON_INVALID_NUMBER, it);
    return JSON_INVALID_NUMBER;
  }
  copy = json__arena_copy(p->arena, start, (size_t)(it - start));
  if (copy == NULL) {
    json__set_error(p, JSON_ALLOCATION_FAILED, start);
    return JSON_ALLOCATION_FAILED;
  }
  errno = 0;
  dv = strtod(copy, NULL);
  json__value_null(out);
  out->type = JSON_NUMBER;
  out->as.number.text = copy;
  out->as.number.len = (size_t)(it - start);
  out->as.number.as_double = dv;
  if (is_integer) {
    errno = 0;
    lv = strtol(copy, &endptr, 10);
    if (errno != ERANGE && *endptr == '\0') {
      out->as.number.as_long = lv;
      out->as.number.is_long = 1;
    }
  }
  p->cur = it;
  return JSON_OK;
}

/* ---- literal parser ---- */

static json_status json__parse_literal(json__parser *p, const char *literal,
                                       size_t len, json_type type, int boolean,
                                       json_value *out) {
  if (!json__match(p, literal, len)) {
    json__set_error(p, JSON_INVALID_LITERAL, p->cur);
    return JSON_INVALID_LITERAL;
  }
  if (!json__is_delim(p->cur + len, p->end)) {
    json__set_error(p, JSON_INVALID_LITERAL, p->cur + len);
    return JSON_INVALID_LITERAL;
  }
  json__value_null(out);
  out->type = type;
  if (type == JSON_BOOL) {
    out->as.boolean = boolean;
  }
  p->cur += len;
  return JSON_OK;
}

/* ---- array and object parsers (call json__parse_value forward) ---- */

static json_status json__parse_value(json__parser *p, json_value *out,
                                     size_t depth);

static json_status json__parse_array(json__parser *p, json_value *out,
                                     size_t depth) {
  json_value *items;
  size_t count;
  size_t i;
  json_status st;
  p->cur++; /* skip '[' */
  json__skip_ws(p);
  count = json__count_items(p->cur, p->end);
  if (count == 0u) {
    if (p->cur >= p->end || *p->cur != ']') {
      json__set_error(p, JSON_EXPECTED_COMMA_OR_END, p->cur);
      return JSON_EXPECTED_COMMA_OR_END;
    }
    p->cur++;
    json__value_null(out);
    out->type = JSON_ARRAY;
    out->as.array.count = 0u;
    out->as.array.items = NULL;
    return JSON_OK;
  }
  items = (json_value *)json__arena_alloc(p->arena, count * sizeof(json_value));
  if (items == NULL) {
    json__set_error(p, JSON_ALLOCATION_FAILED, p->cur);
    return JSON_ALLOCATION_FAILED;
  }
  for (i = 0u; i < count; i++) {
    st = json__parse_value(p, &items[i], depth + 1u);
    if (st != JSON_OK) {
      return st;
    }
    json__skip_ws(p);
    if (i + 1u < count) {
      if (p->cur >= p->end || *p->cur != ',') {
        json__set_error(p, JSON_EXPECTED_COMMA_OR_END, p->cur);
        return JSON_EXPECTED_COMMA_OR_END;
      }
      p->cur++;
      json__skip_ws(p);
    }
  }
  json__skip_ws(p);
  if (p->cur >= p->end || *p->cur != ']') {
    json__set_error(p, JSON_EXPECTED_COMMA_OR_END, p->cur);
    return JSON_EXPECTED_COMMA_OR_END;
  }
  p->cur++;
  json__value_null(out);
  out->type = JSON_ARRAY;
  out->as.array.count = count;
  out->as.array.items = items;
  return JSON_OK;
}

static json_status json__parse_object(json__parser *p, json_value *out,
                                      size_t depth) {
  json_member *members;
  json_value *values;
  json_string key;
  size_t count;
  size_t i;
  json_status st;
  p->cur++; /* skip '{' */
  json__skip_ws(p);
  count = json__count_items(p->cur, p->end);
  if (count == 0u) {
    if (p->cur >= p->end || *p->cur != '}') {
      json__set_error(p, JSON_EXPECTED_COMMA_OR_END, p->cur);
      return JSON_EXPECTED_COMMA_OR_END;
    }
    p->cur++;
    json__value_null(out);
    out->type = JSON_OBJECT;
    out->as.object.count = 0u;
    out->as.object.members = NULL;
    return JSON_OK;
  }
  members = (json_member *)json__arena_alloc(p->arena,
                                             count * sizeof(json_member));
  if (members == NULL) {
    json__set_error(p, JSON_ALLOCATION_FAILED, p->cur);
    return JSON_ALLOCATION_FAILED;
  }
  values = (json_value *)json__arena_alloc(p->arena,
                                           count * sizeof(json_value));
  if (values == NULL) {
    json__set_error(p, JSON_ALLOCATION_FAILED, p->cur);
    return JSON_ALLOCATION_FAILED;
  }
  for (i = 0u; i < count; i++) {
    if (p->cur >= p->end || *p->cur != '"') {
      json__set_error(p, JSON_EXPECTED_KEY, p->cur);
      return JSON_EXPECTED_KEY;
    }
    st = json__parse_string_raw(p, &key);
    if (st != JSON_OK) {
      return st;
    }
    json__skip_ws(p);
    if (p->cur >= p->end || *p->cur != ':') {
      json__set_error(p, JSON_EXPECTED_COLON, p->cur);
      return JSON_EXPECTED_COLON;
    }
    p->cur++;
    json__skip_ws(p);
    st = json__parse_value(p, &values[i], depth + 1u);
    if (st != JSON_OK) {
      return st;
    }
    members[i].key = key;
    members[i].value = &values[i];
    json__skip_ws(p);
    if (i + 1u < count) {
      if (p->cur >= p->end || *p->cur != ',') {
        json__set_error(p, JSON_EXPECTED_COMMA_OR_END, p->cur);
        return JSON_EXPECTED_COMMA_OR_END;
      }
      p->cur++;
      json__skip_ws(p);
    }
  }
  json__skip_ws(p);
  if (p->cur >= p->end || *p->cur != '}') {
    json__set_error(p, JSON_EXPECTED_COMMA_OR_END, p->cur);
    return JSON_EXPECTED_COMMA_OR_END;
  }
  p->cur++;
  json__value_null(out);
  out->type = JSON_OBJECT;
  out->as.object.count = count;
  out->as.object.members = members;
  return JSON_OK;
}

static json_status json__parse_value(json__parser *p, json_value *out,
                                     size_t depth) {
  if (depth > p->cfg.max_depth) {
    json__set_error(p, JSON_DEPTH_LIMIT, p->cur);
    return JSON_DEPTH_LIMIT;
  }
  json__skip_ws(p);
  if (p->cur >= p->end) {
    json__set_error(p, JSON_EXPECTED_VALUE, p->cur);
    return JSON_EXPECTED_VALUE;
  }
  switch (*p->cur) {
  case 'n':
    return json__parse_literal(p, "null", 4u, JSON_NULL, 0, out);
  case 't':
    return json__parse_literal(p, "true", 4u, JSON_BOOL, 1, out);
  case 'f':
    return json__parse_literal(p, "false", 5u, JSON_BOOL, 0, out);
  case '"':
    return json__parse_string_value(p, out);
  case '[':
    return json__parse_array(p, out, depth);
  case '{':
    return json__parse_object(p, out, depth);
  default:
    if (*p->cur == '-' || json__is_digit(*p->cur)) {
      return json__parse_number(p, out);
    }
    json__set_error(p, JSON_EXPECTED_VALUE, p->cur);
    return JSON_EXPECTED_VALUE;
  }
}

/* ---- top-level parse / free ---- */

CSJP_API json_status json_parse_len(const char *text, size_t len,
                                    const json_config *cfg, json_root *out,
                                    json_error *err) {
  json__parser p;
  json__arena *arena;
  json_status st;
  json_value root;
  json_config local_cfg;
  if (out != NULL) {
    memset(out, 0, sizeof(*out));
  }
  if (err != NULL) {
    err->code = JSON_OK;
    err->offset = 0u;
  }
  if (text == NULL || len == 0u) {
    if (err != NULL) {
      err->code = JSON_EMPTY_INPUT;
      err->offset = 0u;
    }
    return JSON_EMPTY_INPUT;
  }
  local_cfg.max_depth = JSON_DEFAULT_MAX_DEPTH;
  local_cfg.allocator = json__default_allocator();
  if (cfg != NULL) {
    if (cfg->max_depth != 0u) {
      local_cfg.max_depth = cfg->max_depth;
    }
    if (cfg->allocator.malloc_fn != NULL && cfg->allocator.free_fn != NULL &&
        cfg->allocator.realloc_fn != NULL) {
      local_cfg.allocator = cfg->allocator;
    }
  }
  arena = json__arena_new(local_cfg.allocator);
  if (arena == NULL) {
    if (err != NULL) {
      err->code = JSON_ALLOCATION_FAILED;
      err->offset = 0u;
    }
    return JSON_ALLOCATION_FAILED;
  }
  p.src = text;
  p.cur = text;
  p.end = text + len;
  p.arena = arena;
  p.cfg = local_cfg;
  p.error.code = JSON_OK;
  p.error.offset = 0u;
  json__skip_ws(&p);
  if (p.cur >= p.end) {
    json__arena_destroy(arena);
    if (err != NULL) {
      err->code = JSON_EMPTY_INPUT;
      err->offset = len;
    }
    return JSON_EMPTY_INPUT;
  }
  st = json__parse_value(&p, &root, 0u);
  if (st == JSON_OK) {
    json__skip_ws(&p);
    if (p.cur != p.end) {
      json__set_error(&p, JSON_TRAILING_INPUT, p.cur);
      st = JSON_TRAILING_INPUT;
    }
  }
  if (st != JSON_OK) {
    if (err != NULL) {
      *err = p.error;
    }
    json__arena_destroy(arena);
    return st;
  }
  if (out != NULL) {
    out->value = root;
    out->arena = arena;
  } else {
    json__arena_destroy(arena);
  }
  return JSON_OK;
}

CSJP_API json_status json_parse(const char *text, json_root *out,
                                json_error *err) {
  return json_parse_len(text, text != NULL ? strlen(text) : 0u, NULL, out, err);
}

CSJP_API void json_free(json_root *root) {
  json__arena *arena;
  if (root == NULL) {
    return;
  }
  arena = (json__arena *)root->arena;
  memset(root, 0, sizeof(*root));
  if (arena != NULL) {
    json__arena_destroy(arena);
  }
}

/* ---- error string ---- */

CSJP_API const char *json_error_to_string(json_status code) {
  switch (code) {
  case JSON_OK:
    return "OK";
  case JSON_EMPTY_INPUT:
    return "Empty input";
  case JSON_TRAILING_INPUT:
    return "Trailing input";
  case JSON_EXPECTED_VALUE:
    return "Expected value";
  case JSON_EXPECTED_KEY:
    return "Expected object key";
  case JSON_EXPECTED_COLON:
    return "Expected colon";
  case JSON_EXPECTED_COMMA_OR_END:
    return "Expected comma or end";
  case JSON_INVALID_LITERAL:
    return "Invalid literal";
  case JSON_INVALID_NUMBER:
    return "Invalid number";
  case JSON_INVALID_STRING:
    return "Invalid string";
  case JSON_INVALID_ESCAPE:
    return "Invalid escape";
  case JSON_INVALID_UNICODE:
    return "Invalid unicode escape";
  case JSON_DEPTH_LIMIT:
    return "Depth limit exceeded";
  case JSON_ALLOCATION_FAILED:
    return "Allocation failed";
  case JSON_WRONG_TYPE:
    return "Wrong type";
  case JSON_NOT_FOUND:
    return "Not found";
  case JSON_INVALID_PATH:
    return "Invalid path";
  default:
    return "Unknown error";
  }
}

/* ---- accessors ---- */

CSJP_API const json_value *json_root_value(const json_root *root) {
  if (root == NULL) {
    return NULL;
  }
  return &root->value;
}

CSJP_API json_type json_type_get(const json_value *value) {
  if (value == NULL) {
    return JSON_NULL;
  }
  return value->type;
}

CSJP_API json_status json_bool_get(const json_value *value, int *out) {
  if (value == NULL || value->type != JSON_BOOL || out == NULL) {
    return JSON_WRONG_TYPE;
  }
  *out = value->as.boolean;
  return JSON_OK;
}

CSJP_API json_status json_string_get(const json_value *value,
                                     const char **data, size_t *len) {
  if (value == NULL || value->type != JSON_STRING) {
    return JSON_WRONG_TYPE;
  }
  if (data != NULL) {
    *data = value->as.string.data;
  }
  if (len != NULL) {
    *len = value->as.string.len;
  }
  return JSON_OK;
}

CSJP_API json_status json_number_get(const json_value *value, const char **text,
                                     size_t *len, double *as_double,
                                     long *as_long, int *is_long) {
  if (value == NULL || value->type != JSON_NUMBER) {
    return JSON_WRONG_TYPE;
  }
  if (text != NULL) {
    *text = value->as.number.text;
  }
  if (len != NULL) {
    *len = value->as.number.len;
  }
  if (as_double != NULL) {
    *as_double = value->as.number.as_double;
  }
  if (as_long != NULL) {
    *as_long = value->as.number.as_long;
  }
  if (is_long != NULL) {
    *is_long = value->as.number.is_long;
  }
  return JSON_OK;
}

CSJP_API json_status json_number_get_double(const json_value *value,
                                            double *out) {
  if (value == NULL || value->type != JSON_NUMBER || out == NULL) {
    return JSON_WRONG_TYPE;
  }
  *out = value->as.number.as_double;
  return JSON_OK;
}

CSJP_API json_status json_number_get_long(const json_value *value, long *out) {
  if (value == NULL || value->type != JSON_NUMBER || out == NULL) {
    return JSON_WRONG_TYPE;
  }
  if (!value->as.number.is_long) {
    return JSON_WRONG_TYPE;
  }
  *out = value->as.number.as_long;
  return JSON_OK;
}

CSJP_API size_t json_array_size(const json_value *array) {
  if (array == NULL || array->type != JSON_ARRAY) {
    return 0u;
  }
  return array->as.array.count;
}

CSJP_API const json_value *json_array_get(const json_value *array,
                                          size_t index) {
  if (array == NULL || array->type != JSON_ARRAY) {
    return NULL;
  }
  if (index >= array->as.array.count) {
    return NULL;
  }
  return &array->as.array.items[index];
}

CSJP_API size_t json_object_size(const json_value *object) {
  if (object == NULL || object->type != JSON_OBJECT) {
    return 0u;
  }
  return object->as.object.count;
}

CSJP_API const char *json_object_key_at(const json_value *object, size_t index,
                                        size_t *len) {
  if (object == NULL || object->type != JSON_OBJECT) {
    return NULL;
  }
  if (index >= object->as.object.count) {
    return NULL;
  }
  if (len != NULL) {
    *len = object->as.object.members[index].key.len;
  }
  return object->as.object.members[index].key.data;
}

CSJP_API const json_value *json_object_value_at(const json_value *object,
                                                size_t index) {
  if (object == NULL || object->type != JSON_OBJECT) {
    return NULL;
  }
  if (index >= object->as.object.count) {
    return NULL;
  }
  return object->as.object.members[index].value;
}

CSJP_API const json_value *json_object_member_at(const json_value *object,
                                                  size_t index,
                                                  const char **key,
                                                  size_t *key_len) {
  if (object == NULL || object->type != JSON_OBJECT) {
    return NULL;
  }
  if (index >= object->as.object.count) {
    return NULL;
  }
  if (key != NULL) {
    *key = object->as.object.members[index].key.data;
  }
  if (key_len != NULL) {
    *key_len = object->as.object.members[index].key.len;
  }
  return object->as.object.members[index].value;
}

static int json__same(const char *a, size_t alen, const char *b, size_t blen) {
  if (alen != blen) {
    return 0;
  }
  return memcmp(a, b, alen) == 0;
}

CSJP_API const json_value *json_object_get_len(const json_value *object,
                                               const char *key, size_t len) {
  size_t i;
  json_member *member;
  if (object == NULL || object->type != JSON_OBJECT || key == NULL) {
    return NULL;
  }
  i = object->as.object.count;
  while (i > 0u) {
    i--;
    member = &object->as.object.members[i];
    if (json__same(member->key.data, member->key.len, key, len)) {
      return member->value;
    }
  }
  return NULL;
}

CSJP_API const json_value *json_object_get(const json_value *object,
                                           const char *key) {
  return json_object_get_len(object, key, key != NULL ? strlen(key) : 0u);
}

/* ---- path helpers ---- */

static int json__parse_size_index(const char *text, size_t len, size_t *out) {
  size_t index;
  size_t pos;
  size_t digit;
  if (len == 0u) {
    return 0;
  }
  index = 0u;
  pos = 0u;
  while (pos < len) {
    if (!json__is_digit(text[pos])) {
      return 0;
    }
    digit = (size_t)(text[pos] - '0');
    if (index > (((size_t)-1) - digit) / 10u) {
      return 0;
    }
    index = index * 10u + digit;
    pos++;
  }
  *out = index;
  return 1;
}

static const json_value *json__path_get_status(const json_value *root,
                                               const char *path,
                                               json_status *status) {
  const json_value *cur;
  const char *p;
  size_t index;
  const char *digits;
  char stack_key[256];
  char *key;
  char *key_next;
  size_t len;
  size_t cap;
  size_t next_cap;

  if (status != NULL) {
    *status = JSON_OK;
  }
  if (root == NULL || path == NULL) {
    if (status != NULL) {
      *status = JSON_NOT_FOUND;
    }
    return NULL;
  }
  cur = root;
  p = path;
  if (*p == '\0') {
    return root;
  }
  while (*p != '\0') {
    if (*p == '.') {
      p++;
      if (*p == '\0') {
        if (status != NULL) {
          *status = JSON_INVALID_PATH;
        }
        return NULL;
      }
    }
    if (*p == '[') {
      p++;
      if (!json__is_digit(*p)) {
        if (status != NULL) {
          *status = JSON_INVALID_PATH;
        }
        return NULL;
      }
      digits = p;
      while (json__is_digit(*p)) {
        p++;
      }
      if (!json__parse_size_index(digits, (size_t)(p - digits), &index)) {
        if (status != NULL) {
          *status = JSON_INVALID_PATH;
        }
        return NULL;
      }
      if (*p != ']') {
        if (status != NULL) {
          *status = JSON_INVALID_PATH;
        }
        return NULL;
      }
      p++;
      if (cur == NULL || cur->type != JSON_ARRAY) {
        if (status != NULL) {
          *status = JSON_WRONG_TYPE;
        }
        return NULL;
      }
      cur = json_array_get(cur, index);
      if (cur == NULL) {
        if (status != NULL) {
          *status = JSON_NOT_FOUND;
        }
        return NULL;
      }
      continue;
    }
    /* key segment: stack buffer for keys <= 255 chars, heap beyond that.
       the custom allocator is not reachable from json_value*, so the heap
       fallback uses the system allocator directly. */
    key = stack_key;
    cap = sizeof(stack_key);
    len = 0u;
    while (*p != '\0' && *p != '.' && *p != '[') {
      if (*p == '\\' && p[1] != '\0') {
        p++;
      }
      if (len == (size_t)(-1)) {
        if (key != stack_key) {
          free(key);
        }
        if (status != NULL) {
          *status = JSON_INVALID_PATH;
        }
        return NULL;
      }
      if (len + 1u >= cap) {
        next_cap = cap * 2u;
        if (next_cap < cap) {
          if (key != stack_key) {
            free(key);
          }
          if (status != NULL) {
            *status = JSON_ALLOCATION_FAILED;
          }
          return NULL;
        }
        key_next = (char *)malloc(next_cap);
        if (key_next == NULL) {
          if (key != stack_key) {
            free(key);
          }
          if (status != NULL) {
            *status = JSON_ALLOCATION_FAILED;
          }
          return NULL;
        }
        memcpy(key_next, key, len);
        if (key != stack_key) {
          free(key);
        }
        key = key_next;
        cap = next_cap;
      }
      key[len++] = *p++;
    }
    key[len] = '\0';
    if (len == 0u) {
      if (key != stack_key) {
        free(key);
      }
      if (status != NULL) {
        *status = JSON_INVALID_PATH;
      }
      return NULL;
    }
    if (cur != NULL && cur->type == JSON_ARRAY) {
      if (json__parse_size_index(key, len, &index)) {
        if (key != stack_key) {
          free(key);
        }
        cur = json_array_get(cur, index);
        if (cur == NULL) {
          if (status != NULL) {
            *status = JSON_NOT_FOUND;
          }
          return NULL;
        }
        continue;
      }
      if (json__is_digit(key[0])) {
        if (key != stack_key) {
          free(key);
        }
        if (status != NULL) {
          *status = JSON_INVALID_PATH;
        }
        return NULL;
      }
    }
    if (cur == NULL || cur->type != JSON_OBJECT) {
      if (key != stack_key) {
        free(key);
      }
      if (status != NULL) {
        *status = JSON_WRONG_TYPE;
      }
      return NULL;
    }
    cur = json_object_get_len(cur, key, len);
    if (key != stack_key) {
      free(key);
    }
    if (cur == NULL) {
      if (status != NULL) {
        *status = JSON_NOT_FOUND;
      }
      return NULL;
    }
  }
  return cur;
}

CSJP_API const json_value *json_path_get(const json_value *root,
                                         const char *path) {
  return json__path_get_status(root, path, NULL);
}

CSJP_API json_status json_path_get_bool(const json_value *root,
                                        const char *path, int *out) {
  const json_value *value;
  json_status st;
  value = json__path_get_status(root, path, &st);
  if (value == NULL) {
    return st;
  }
  return json_bool_get(value, out);
}

CSJP_API json_status json_path_get_string(const json_value *root,
                                          const char *path, const char **data,
                                          size_t *len) {
  const json_value *value;
  json_status st;
  value = json__path_get_status(root, path, &st);
  if (value == NULL) {
    return st;
  }
  return json_string_get(value, data, len);
}

CSJP_API json_status json_path_get_long(const json_value *root,
                                        const char *path, long *out) {
  const json_value *value;
  json_status st;
  value = json__path_get_status(root, path, &st);
  if (value == NULL) {
    return st;
  }
  return json_number_get_long(value, out);
}

CSJP_API json_status json_path_get_double(const json_value *root,
                                          const char *path, double *out) {
  const json_value *value;
  json_status st;
  value = json__path_get_status(root, path, &st);
  if (value == NULL) {
    return st;
  }
  return json_number_get_double(value, out);
}

#endif
