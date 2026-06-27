# CSJP

![single-header](https://img.shields.io/badge/single--header-blue?style=flat-square)
![ANSI C89](https://img.shields.io/badge/ANSI-C89-orange?style=flat-square)
![zero dependencies](https://img.shields.io/badge/dependencies-zero-green?style=flat-square)
![strict JSON](https://img.shields.io/badge/RFC%208259-strict-red?style=flat-square)
![arena DOM](https://img.shields.io/badge/memory-arena%20DOM-purple?style=flat-square)
![MIT](https://img.shields.io/badge/license-MIT-lightgrey?style=flat-square)

`CSJP` is a single-header JSON parser for C89 — strict RFC 8259, arena-owned DOM, zero dependencies.

**One header. Strict. Blazing fast.**

---

## Features

- **Single header** — copy `json.h`, include it, done. Nothing to compile or link.
- **Strict** — trailing commas, lone surrogates, overlong UTF-8, bad escapes all fail with a zero-based byte offset pointing at the exact mistake.
- **Arena DOM** — one `json_free(&root)` releases the entire tree. No per-node cleanup, no leaks.
- **Path expressions** — `json_path_get_long(&v, "hits[0].score", &n)` reads deeply nested values in one call.
- **Honest numbers** — source text always preserved; `as_long`/`is_long` only set when the integer fits exactly in `long`. Overflow never fails the parse.
- **C89 portable** — builds clean with `-std=c89 -pedantic -Wall -Wextra -Werror`. No `bool`, no VLAs, no platform guards. C99/C11 work too.
- **Configurable** — custom allocator and depth limit via `json_config`. Validate without building a DOM by passing `NULL` for `out`.

---

## Quick Start

```c
#include "json.h"

int main(void) {
  json_root root;
  json_error err;
  long width;
  const char *label;
  size_t label_len;

  if (json_parse("{\"image\":{\"width\":640,\"label\":\"hero\"}}", &root, &err)
      != JSON_OK) {
    fprintf(stderr, "parse error at byte %lu\n", (unsigned long)err.offset);
    return 1;
  }

  json_path_get_long(&root.value, "image.width", &width);
  json_path_get_string(&root.value, "image.label", &label, &label_len);

  printf("Label: %s, width: %ld\n", label, width);
  /* Output: Label: hero, width: 640 */

  json_free(&root);   /* one call releases the whole tree */
  return 0;
}
```

Copy `json.h`. Include it. Ship it.

---

## Design Goals

**Strict by default.** Trailing commas, leading zeros, bad escapes, lone
surrogates, overlong UTF-8 — all rejected, all with a zero-based byte offset
pointing at the exact mistake. There is no lenient mode and no silent
truncation.

**Ownership is obvious.** `json_root` holds the arena. Every string, number
text, array item, and object member points into it. Nothing is heap-allocated
separately. When you call `json_free`, it is gone — all of it, one free.

**Easy path-based access.** `json_path_get_long(&root.value, "hits[0].score",
&score)` reads a deeply nested integer in one call. No walking. No casting. No
intermediate pointers to manage. If the path is wrong or the type doesn't
match, you get a `json_status` that says exactly why.

**Numbers that tell the truth.** Every number keeps its original source text.
`as_double` is always set. `as_long` is set — and `is_long` is true — only
when the value is an integer that fits exactly in `long`. Overflow never fails
the parse; it just means `is_long` is false. You decide what to do.

**C89 all the way down.** Builds clean with `-std=c89 -pedantic -Wall -Wextra
-Werror`. Works with any C99 or C11 compiler too. No `bool`, no
`//` comments, no VLAs, no platform guards, no surprises.

---

## Performance

Two-pass parsing allocates each array and object exactly once — no
capacity-doubling, no stranded arena slabs. Numbers from `csjp_bench 1000` on
a MacBook Pro M5 with 32 GB of RAM:

| File | Size | Throughput |
|------|-----:|----------:|
| `reddit.json` | 97 KB | **205 MB/s** |
| `rickandmorty.json` | 19 KB | **267 MB/s** |
| `food.json` | 32 KB | **163 MB/s** |

These include full parse, DOM build, and `json_free`.

For context — csjp targets correctness and portability over raw speed. SIMD-accelerated parsers like [yyjson](https://github.com/ibireme/yyjson) reach 1–4 GB/s on the same hardware by requiring modern compilers and CPU features. csjp trades that ceiling for C89 portability, strict validation, and a simple arena API.

Run the benchmarks yourself:

```sh
cmake -S . -B build && cmake --build build
./build/csjp_bench 1000
```

---

## API

### Parse and free

```c
/* NUL-terminated input */
json_status json_parse(const char *text, json_root *out, json_error *err);

/* Explicit length + optional config (depth limit, custom allocator) */
json_status json_parse_len(const char *text, size_t len,
                           const json_config *cfg, json_root *out,
                           json_error *err);

/* Pass NULL for out to validate without building a DOM */
json_status json_parse_len(text, len, NULL, NULL, &err);

void        json_free(json_root *root);     /* NULL-safe */
const char *json_error_to_string(json_status code);

/* Entry point into the value tree */
const json_value *json_root_value(const json_root *root); /* returns &root->value */
```

### Type inspection and scalar getters

```c
json_type   json_type_get(const json_value *v);

json_status json_bool_get(const json_value *v, int *out);

json_status json_string_get(const json_value *v,
                            const char **data, size_t *len);

/* Full number info — all output pointers optional */
json_status json_number_get(const json_value *v,
                            const char **text, size_t *len,
                            double *as_double, long *as_long, int *is_long);

/* Typed shortcuts */
json_status json_number_get_double(const json_value *v, double *out);
json_status json_number_get_long(const json_value *v, long *out);
/* json_number_get_long returns JSON_WRONG_TYPE when value overflows long */
```

### Arrays

```c
size_t            json_array_size(const json_value *array);
const json_value *json_array_get(const json_value *array, size_t index);
```

### Objects

```c
size_t            json_object_size(const json_value *object);

/* Index-based iteration */
const char       *json_object_key_at(const json_value *object,
                                     size_t index, size_t *len);
const json_value *json_object_value_at(const json_value *object, size_t index);

/* Combined key+value in one call */
const json_value *json_object_member_at(const json_value *object,
                                        size_t index,
                                        const char **key, size_t *key_len);

/* Key lookup — returns the last match (duplicate keys are preserved) */
const json_value *json_object_get(const json_value *object, const char *key);
const json_value *json_object_get_len(const json_value *object,
                                      const char *key, size_t len);
```

### Path expressions

```c
const json_value *json_path_get(const json_value *root, const char *path);
json_status json_path_get_bool(const json_value *root, const char *path, int *out);
json_status json_path_get_string(const json_value *root, const char *path,
                                 const char **data, size_t *len);
json_status json_path_get_long(const json_value *root, const char *path, long *out);
json_status json_path_get_double(const json_value *root, const char *path, double *out);
```

Path syntax:

| Expression | Meaning |
|-----------|---------|
| `image.width` | object key chain |
| `hits[0].score` | array index with bracket |
| `hits.0.score` | array index with dot (shorthand) |
| `meta.file\\.name` | literal dot in key (`\\.` escapes it) |

Return codes: `JSON_OK`, `JSON_NOT_FOUND`, `JSON_WRONG_TYPE`, `JSON_INVALID_PATH`.

### Configuration

```c
typedef struct json_config {
  size_t max_depth;       /* default: 256 */
  json_allocator allocator;
} json_config;

typedef struct json_allocator {
  void *ctx;
  void *(*malloc_fn)(void *ctx, size_t size);
  void *(*realloc_fn)(void *ctx, void *ptr, size_t old_size, size_t new_size);
  void (*free_fn)(void *ctx, void *ptr, size_t size);
} json_allocator;
```

All three allocator hooks must be set together or none at all.

---

## Integration

**Recommended:** copy `json.h` into your tree and include it.

```c
#include "json.h"
```

**CMake embed:**

```cmake
add_subdirectory(csjp)
target_link_libraries(myapp PRIVATE csjp::csjp)
```

Tests and benchmarks are off by default when embedded.

---

## Build and Test

```sh
# fastest — no cmake needed
cc -std=c89 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests
./csjp_tests

# with sanitizers (recommended when touching parser internals)
cc -std=c89 -pedantic -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  tests/test_json.c -I. -o csjp_tests_san
./csjp_tests_san

# cmake
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Caveats

- **Arena lifetime.** Strings and number text point directly into the arena. Do not use them after calling `json_free(&root)`.
- **Duplicate keys.** `json_object_get` returns the **last** matching key, not the first. All duplicates are preserved and accessible by index via `json_object_member_at`.
- **Long overflow.** `json_number_get_long` returns `JSON_WRONG_TYPE` for floats and integers that overflow `long`. Use `json_number_get` with `is_long` to distinguish overflow from a type mismatch.
- **Custom allocator.** All three hooks (`malloc_fn`, `realloc_fn`, `free_fn`) must be set together — partial configuration is not supported.
- **Validate-only mode.** `json_parse_len(text, len, NULL, NULL, &err)` checks syntax without building a DOM. The `err` pointer must be non-NULL.

---

## Migrating from V2

V3 is a full rewrite with no shared API surface. See [MIGRATION.md](MIGRATION.md) for a complete mapping of type names, parsing patterns, object lookup, and number access.

---

## License

MIT. See `LICENSE`.
