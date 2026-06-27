# csjp

![single-header](https://img.shields.io/badge/single--header-blue?style=flat-square)
![ANSI C89](https://img.shields.io/badge/ANSI-C89-orange?style=flat-square)
![zero dependencies](https://img.shields.io/badge/dependencies-zero-green?style=flat-square)
![strict JSON](https://img.shields.io/badge/RFC%208259-strict-red?style=flat-square)
![arena DOM](https://img.shields.io/badge/memory-arena%20DOM-purple?style=flat-square)
![MIT](https://img.shields.io/badge/license-MIT-lightgrey?style=flat-square)

**One header. Strict JSON. Blazing fast.**

`csjp` parses JSON into a clean, arena-owned DOM. Drop a single file into any
C project, parse once, read with typed getters or dot-path expressions, and
free everything with one call. No build system. No generated code. No runtime.
No allocator drama.

---

## 30-Second Start

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

  if (json_path_get_long(&root.value, "image.width", &width) != JSON_OK ||
      json_path_get_string(&root.value, "image.label", &label, &label_len) != JSON_OK) {
    return 1;
  }

  printf("Label: %s\n", label);

  json_free(&root);   /* one call releases the whole tree */
  return 0;
}
```

Copy `json.h`. Include it. Ship it.

---

## Why csjp

**Strict by default.** Trailing commas, leading zeros, bad escapes, lone
surrogates, overlong UTF-8 — all rejected, all with a zero-based byte offset
pointing at the exact mistake. There is no lenient mode and no silent
truncation.

**Ownership is obvious.** `json_root` holds the arena. Every string, number
text, array item, and object member points into it. Nothing is heap-allocated
separately. When you call `json_free`, it is gone — all of it, one free.

**Path reads for humans.** `json_path_get_long(&root.value, "hits[0].score",
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
a Macbook Pro M5 with 32GB of RAM:

| File | Size | Throughput |
|------|-----:|----------:|
| `reddit.json` | 97 KB | **205 MB/s** |
| `rickandmorty.json` | 19 KB | **267 MB/s** |
| `food.json` | 32 KB | **163 MB/s** |

These include full parse, DOM build, and `json_free`. Run them yourself:

```sh
cmake -S . -B build && cmake --build build
./build/csjp_bench 1000
```

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

## Migration from V2 to V3

V3 is a full rewrite. V2 and V3 share no API surface. The concepts map across,
but every type name, every function call, and the build model itself changed.

### Build model

V2 was a two-file library. You compiled `json.c` and linked it.

```sh
# V2
cc main.c json.c -I. -o app
```

V3 is a single header. There is nothing to compile separately.

```sh
# V3
cc main.c -I. -o app
```

### Type names

| V2 | V3 |
|----|----|
| `json_element_t` | `json_value` |
| `json_object_t` | `json_object` (inside `json_value.as.object`) |
| `json_array_t` | `json_array` (inside `json_value.as.array`) |
| `json_entry_t` | `json_member` |
| `json_number_t` | `json_number` (inside `json_value.as.number`) |
| `json_string_t` (typedef for `const char *`) | `json_string` (struct with `.data` + `.len`) |
| `json_boolean_t` | `int` |
| `json_error_t` | `json_status` (enum) + `json_error` (struct with byte offset) |
| `json_element_type_t` | `json_type` |

### Parsing

V2 returned a result monad that had to be unwrapped:

```c
/* V2 */
result(json_element) res = json_parse(text);
if (result_is_err(json_element)(&res)) {
    json_error_t err = result_unwrap_err(json_element)(&res);
    /* err is an enum, no byte offset */
}
json_element_t root = result_unwrap(json_element)(&res);
/* must manually free later */
```

V3 writes into a caller-owned `json_root` and returns a status code:

```c
/* V3 */
json_root root;
json_error err;   /* includes a byte offset */
if (json_parse(text, &root, &err) != JSON_OK) {
    fprintf(stderr, "error at byte %lu\n", (unsigned long)err.offset);
}
/* root owns everything; json_free releases it all */
```

### Object lookup

V2 stored object members in a hash map and looked them up with `json_object_find`:

```c
/* V2 */
result(json_element) res = json_object_find(obj, "key");
json_element_t val = result_unwrap(json_element)(&res);
```

V3 stores members in insertion order (iterable by index) and looks up by key
with `json_object_get`. Duplicate keys are preserved; the last one wins on
lookup:

```c
/* V3 */
const json_value *val = json_object_get(&root.value, "key");
```

### Number access

V2 numbers carried a type tag — a value was either a `long` or a `double`, never both:

```c
/* V2 */
if (elem.value.as_number.type == JSON_NUMBER_TYPE_LONG)
    long v = elem.value.as_number.value.as_long;
else
    double v = elem.value.as_number.value.as_double;
```

V3 always computes both. `as_double` is always valid. `as_long` is valid when
`is_long` is true (integer that fits in `long`). Overflow is not an error:

```c
/* V3 */
double d;
long   l;
int    fits;
json_number_get(val, NULL, NULL, &d, &l, &fits);
/* or the typed shortcuts: */
json_number_get_double(val, &d);
json_number_get_long(val, &l);   /* JSON_WRONG_TYPE if overflows */
```

### Memory

V2 required manual element-by-element cleanup. V3 frees the entire tree in
one call:

```c
/* V3 */
json_free(&root);
```

### What V3 adds that V2 never had

- Byte-accurate error offsets in `json_error.offset`
- Path expressions: `json_path_get_long(&root.value, "a.b[0].c", &v)`
- Depth limit via `json_config.max_depth`
- Custom allocators via `json_config.allocator`
- Validate-only mode: pass `NULL` for `out` to check syntax without a DOM
- Preserved number source text in `json_number.text`
- Strict RFC 8259 enforcement (surrogates, overlong UTF-8, trailing commas, etc.)

---

## License

MIT. See `LICENSE`.
