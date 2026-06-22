# C Simple JSON Parser

A tiny ANSI C JSON parser that is easy to embed: one required header, no
mandatory dependencies, strict parsing, arena-owned memory, path lookup, and
optional CMake targets for larger projects.

## Features

- Single-header distribution through `json.h`
- ANSI C89-compatible implementation style
- Strict JSON parsing with error offsets
- Valid `null`, empty strings, empty arrays, and empty objects
- Owned UTF-8 strings with `\uXXXX` decoding and surrogate validation
- Exact number text plus `double` and exact-fit `long` accessors
- Arena-owned DOM released with one `json_free`
- Object and array iteration helpers
- Dotted/bracket path lookup such as `data[0].items[0].size.width`
- Optional static CMake target for professional projects

## Quick Start

Use `json.c` if you want a conventional two-file build:

```c
#include "json.h"

int main(void) {
  json_value root;
  json_error err;
  json_status st;
  long width;

  st = json_parse("{\"data\":[{\"size\":{\"width\":42}}]}", &root, &err);
  if (st != JSON_OK) {
    return 1;
  }

  if (json_path_get_long(&root, "data[0].size.width", &width) == JSON_OK) {
    /* width == 42 */
  }

  json_free(&root);
  return 0;
}
```

Or use `json.h` as a single-header library by defining `JSON_IMPLEMENTATION`
in exactly one translation unit:

```c
#define JSON_IMPLEMENTATION
#include "json.h"
```

## CMake

Header-only target:

```cmake
add_subdirectory(C-Simple-JSON-Parser)
target_link_libraries(app PRIVATE csjp::csjp)
```

With this mode, define `JSON_IMPLEMENTATION` in one of your source files.

Static target:

```cmake
set(CSJP_BUILD_STATIC ON)
add_subdirectory(C-Simple-JSON-Parser)
target_link_libraries(app PRIVATE csjp::csjp_static)
```

The static target compiles a generated implementation translation unit for you.
When included with `add_subdirectory`, tests and benchmarks default to off unless
this project is the top-level build.

Installed package:

```cmake
find_package(csjp CONFIG REQUIRED)
target_link_libraries(app PRIVATE csjp::csjp)
```

The package also exports `csjp::csjp_static` when installed with
`CSJP_BUILD_STATIC=ON`.

## API Overview

```c
json_status json_parse(const char *text, json_value *out, json_error *err);
json_status json_parse_len(const char *text, size_t len,
                           const json_config *cfg, json_value *out,
                           json_error *err);
void json_free(json_value *value);
const char *json_error_to_string(json_status code);
```

`json_error.offset` is a zero-based byte offset into the original input. For
unexpected end-of-input it is the input length. For trailing input it is the
first non-whitespace byte after the parsed value.

Accessors return `JSON_WRONG_TYPE` when the value is not the requested type:

```c
json_type json_get_type(const json_value *value);
json_status json_get_bool(const json_value *value, int *out);
json_status json_get_string(const json_value *value, const char **data,
                            size_t *len);
json_status json_get_number(const json_value *value, const char **text,
                            size_t *len, double *as_double, long *as_long,
                            int *has_long);
```

Objects preserve insertion order. Duplicate keys are valid; iteration returns
all members, while lookup returns the last matching key.

```c
size_t json_object_size(const json_value *object);
const char *json_object_key(const json_value *object, size_t index,
                            size_t *len);
const json_value *json_object_value(const json_value *object, size_t index);
const json_value *json_object_find(const json_value *object, const char *key);
```

Arrays are contiguous:

```c
size_t json_array_size(const json_value *array);
const json_value *json_array_get(const json_value *array, size_t index);
```

## Path Lookup

Path helpers are part of the core API:

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

Path rules:

- `.` selects object keys
- `[0]` selects array indices
- numeric dot segments are accepted as array shorthand
- `data[0].items[0].size.width` and `data.0.items.0.size.width` both work
- escape a literal dot in a key with `\\.`, for example `metadata.file\\.name`

Typed path helpers return `JSON_NOT_FOUND`, `JSON_WRONG_TYPE`, or
`JSON_INVALID_PATH` as appropriate.

## Numbers

JSON has one number type, so the parser keeps the exact source text. Convenience
fields are also available:

- `as_double` is parsed with the C library after grammar validation
- `as_long` is set only when the number is an exact `long`
- `has_long` tells whether `as_long` is valid

Overflow does not make valid JSON fail to parse. It only affects convenience
conversion fields.

## Ownership

The parser owns all DOM memory through an arena. Call `json_free(&root)` once on
the top-level parsed value. Returned strings, keys, numbers, and child values are
views into that arena and remain valid until the root is freed.

`json_free(NULL)` is allowed. A zeroed `json_value` is `JSON_NULL` and safe to
free.

Custom allocators are supplied through `json_config`. Provide `malloc_fn`,
`realloc_fn`, and `free_fn` together; partial allocator hooks are ignored so
memory from one allocation family is never passed to another family's `realloc`
or `free`. Path lookup uses temporary scratch buffers for escaped key segments;
those buffers use the root value's allocator when the value came from
`json_parse_len`.

## Build And Test

```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror json.c tests/test_json.c -I. -o csjp_tests
./csjp_tests

cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Benchmark:

```sh
./build/csjp_bench 1000
```

## Migration Notes

v2 intentionally breaks the old ABI and struct layout. The old result macros,
exact-sized hash table, whitespace compile flag, and recursive heap ownership
model are gone.

Familiar names remain where the new semantics are clean:

- `json_parse`
- `json_free`
- `json_object_find`
- `json_error_to_string`

The main migration change is to use explicit status returns and accessors rather
than unwrapping result macros and reading old exposed structs directly.
