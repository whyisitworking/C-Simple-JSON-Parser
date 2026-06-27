# csjp: Tiny JSON, Sharp C

`single-header` `ANSI C89` `zero dependencies` `strict JSON`
`arena DOM` `path reads` `CMake-friendly`

`csjp` is a small JSON parser for C projects that care about a clean API more
than a mountain of knobs. Drop in `json.h`, parse strict JSON, read values
directly or by path, and release the whole DOM with one `json_free`.

The V3 rewrite is intentionally lean: one header, one ownership model, one
obvious way to use it.

## Why It Feels Good To Use

- **Include and go:** `#include "json.h"` is the whole integration.
- **C89-clean:** builds with `-std=c89 -pedantic -Wall -Wextra -Werror`.
- **Strict parser:** invalid JSON fails with a `json_status` and byte offset.
- **Simple lifetime:** the parsed root owns every string, array item, object
  member, and number text.
- **Readable API:** `json_string_get`, `json_array_get`, `json_object_get`,
  `json_path_get_long`.
- **Path reads built in:** grab `data[0].items[0].name` without hand-walking the
  tree.
- **Useful numbers:** keep exact source text plus `double` and exact-fit `long`
  conversions.
- **No dependency story:** no package manager, code generator, runtime, or link
  target required.

## Integration

Copy `json.h` into your project and include it from any `.c` file that needs
JSON:

```c
#include "json.h"
```

That is the recommended path.

If you already use CMake, this repository also exposes a tiny interface target:

```cmake
add_subdirectory(C-Simple-JSON-Parser)
target_link_libraries(app PRIVATE csjp::csjp)
```

When embedded with `add_subdirectory`, local tests and benchmarks default to
off.

## Quick Start

```c
#include <stdio.h>
#include "json.h"

int main(void) {
  json_value root;
  json_error err;
  long width;

  if (json_parse("{\"image\":{\"width\":640}}", &root, &err) != JSON_OK) {
    fprintf(stderr, "JSON error at byte %lu\n", (unsigned long)err.offset);
    return 1;
  }

  if (json_path_get_long(&root, "image.width", &width) == JSON_OK) {
    printf("width: %ld\n", width);
  }

  json_free(&root);
  return 0;
}
```

Everything returned from `root` is valid until `json_free(&root)`.

## Path Reads

Path helpers are for the common case: read a nested field and move on.

```c
const char *name;
size_t name_len;
long calories;
int active;

json_path_get_string(&root, "data.items[0].name", &name, &name_len);
json_path_get_long(&root, "data.calories", &calories);
json_path_get_bool(&root, "active", &active);
```

Supported syntax:

- `.` selects object keys: `image.width`.
- `[0]` selects array items: `items[0].name`.
- Numeric dot segments are accepted as array shorthand:
  `items.0.name` is the same as `items[0].name`.
- Escape a literal dot in an object key with `\\.`:
  `metadata.file\\.name`.

Typed path helpers return `JSON_OK`, `JSON_NOT_FOUND`, `JSON_WRONG_TYPE`, or
`JSON_INVALID_PATH`.

## API

### Parse

```c
json_status json_parse(const char *text, json_value *out, json_error *err);
json_status json_parse_len(const char *text, size_t len,
                           const json_config *cfg, json_value *out,
                           json_error *err);
void json_free(json_value *value);
const char *json_error_to_string(json_status code);
```

Use `json_parse` for NUL-terminated strings and `json_parse_len` for explicit
byte buffers. Parse failures report a zero-based byte offset in `json_error`.

### Values

```c
json_type json_type_get(const json_value *value);
json_status json_bool_get(const json_value *value, int *out);
json_status json_string_get(const json_value *value, const char **data,
                            size_t *len);
json_status json_number_get(const json_value *value, const char **text,
                            size_t *len, double *as_double, long *as_long,
                            int *is_long);
```

Typed getters return `JSON_WRONG_TYPE` when the value is not the requested type.
Output pointers may be `NULL` when you do not need that field, except for the
actual destination in scalar reads such as `json_bool_get`.

### Arrays

```c
size_t json_array_size(const json_value *array);
const json_value *json_array_get(const json_value *array, size_t index);
```

### Objects

```c
size_t json_object_size(const json_value *object);
const char *json_object_key_at(const json_value *object, size_t index,
                               size_t *len);
const json_value *json_object_value_at(const json_value *object, size_t index);
const json_value *json_object_get(const json_value *object, const char *key);
const json_value *json_object_get_len(const json_value *object,
                                      const char *key, size_t len);
```

Objects preserve insertion order. Duplicate keys are preserved during
iteration; keyed lookup returns the last matching key.

### Paths

```c
const json_value *json_path_get(const json_value *root, const char *path);
json_status json_path_get_bool(const json_value *root, const char *path,
                               int *out);
json_status json_path_get_string(const json_value *root, const char *path,
                                 const char **data, size_t *len);
json_status json_path_get_long(const json_value *root, const char *path,
                               long *out);
json_status json_path_get_double(const json_value *root, const char *path,
                                 double *out);
```

## Numbers

`json_number` keeps the original number text and two convenience conversions:

```c
typedef struct json_number {
  const char *text;
  size_t len;
  double as_double;
  long as_long;
  int is_long;
} json_number;
```

`is_long` is true only when the JSON number is an integer that fits in `long`.
Overflow does not make valid JSON fail to parse; it only means `as_long` is not
available.

## Configuration

`json_parse_len` accepts an optional `json_config`:

```c
typedef struct json_config {
  size_t max_depth;
  json_allocator allocator;
} json_config;
```

`max_depth` defaults to `JSON_DEFAULT_MAX_DEPTH`, currently `256`. Custom
allocators are optional; provide `malloc_fn`, `realloc_fn`, and `free_fn`
together.

## Performance

The bundled benchmark measures parse plus DOM allocation/free on the included
sample files. On this machine, the larger fixtures from `csjp_bench 1000`
landed here:

| Fixture | Throughput |
| --- | ---: |
| `sample/food.json` | 108.91 MB/s |
| `sample/rickandmorty.json` | 120.94 MB/s |
| `sample/reddit.json` | 123.88 MB/s |

This is not a portable guarantee, but it is the right kind of simple: strict
JSON, useful DOM, no dependency tax, and fast enough to stay out of the way.

Run it locally:

```sh
cmake -S . -B build
cmake --build build
./build/csjp_bench 1000
```

## Build And Test

```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests
./csjp_tests

cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The repository test matrix also covers C99, C11, sanitizer builds, the example
program, CMake install smoke, and multi-translation-unit include smoke.

## V3 Notes

V3 intentionally breaks old layouts and old helper names. The cleaned API keeps
the parts worth having:

- `json_parse` / `json_parse_len` return `json_status`.
- `json_error` reports byte offsets.
- `json_free` releases the full DOM.
- `json_object_get` returns the last duplicate key.
- `json_path_get_*` is part of the core library.

## Contributing

See `CONTRIBUTING.md` for coding standards, tests, and review expectations.

## License

`csjp` is distributed under the MIT License. See `LICENSE`.
