# Migrating from V2 to V3

V3 is a full rewrite. V2 and V3 share no API surface. The concepts map across,
but every type name, every function call, and the build model itself changed.

## Build model

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

## Type names

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

## Parsing

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

## Object lookup

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

## Number access

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

## Memory

V2 required manual element-by-element cleanup. V3 frees the entire tree in
one call:

```c
/* V3 */
json_free(&root);
```

## What V3 adds that V2 never had

- Byte-accurate error offsets in `json_error.offset`
- Path expressions: `json_path_get_long(&root.value, "a.b[0].c", &v)`
- Depth limit via `json_config.max_depth`
- Custom allocators via `json_config.allocator`
- Validate-only mode: pass `NULL` for `out` to check syntax without a DOM
- Preserved number source text in `json_number.text`
- Strict RFC 8259 enforcement (surrogates, overlong UTF-8, trailing commas, etc.)
