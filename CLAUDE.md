# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Test

**Direct build (fastest, no CMake required):**
```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests
./csjp_tests
```

**CMake build:**
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

**With sanitizers (required when touching parser internals):**
```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer tests/test_json.c -I. -o csjp_tests_san
./csjp_tests_san
```

**Portability matrix:**
```sh
cc -std=c99 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests_c99
cc -std=c11 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests_c11
```

**Example and benchmark:**
```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror example.c -I. -o csjp_example && ./csjp_example
./build/csjp_bench 1000
```

## Architecture

`csjp` is a **single-header library** — the entire implementation lives in `json.h`. There are no other source files to build or link. Consumers just `#include "json.h"` from any `.c` file.

**Ownership model:** `json_parse` allocates a single arena (linked list of `json__chunk` slabs) attached to `root.arena`. All strings, arrays, and object members point into that arena. One call to `json_free(&root)` releases everything.

**Key types:**
- `json_value` — tagged union holding a `json_type` and the arena pointer (only on the root value)
- `json__arena` / `json__chunk` — slab allocator; chunks default to `JSON_ARENA_CHUNK_SIZE` (4096 bytes)
- `json__parser` — cursor state `{src, cur, end}` plus arena and config

**Naming conventions:**
- Public API: `json_*` (declared `CSJP_API static`)
- Internal helpers: `json__*` (double underscore)
- All internal functions are `static`; the header is safe to include in multiple translation units because every symbol is `static`

**Object lookup:** `json_object_get` scans members in reverse insertion order and returns the *last* matching key. This is intentional V3 behavior.

**Path syntax:** `.` for keys, `[n]` or `.n` for array indices, `\\.` to escape a literal dot in a key.

## Code Style

- Write ANSI C89-compatible C (no `//` comments, no `int i` in for-loops, no C99 types like `bool`)
- `size_t` for all sizes and byte offsets
- No global state, no platform-specific code without a guarded fallback
- Public API declarations go near the top of `json.h`; implementation follows below the `#include` block of standard headers
- Keep comments sparse — only for non-obvious invariants

## Key Behaviors to Preserve (V3 Contracts)

- Strict parsing: trailing input, malformed literals, invalid UTF-8, bad escapes must all fail with a `json_status` and zero-based byte offset in `json_error`
- `json_free(NULL)` is valid; a zeroed `json_value` is safe to free
- Number source text is always preserved in `json_number.text`
- `as_long` / `is_long` are only set for integers that fit exactly in `long`; overflow does not fail the parse
- Custom allocators require all three hooks (`malloc_fn`, `realloc_fn`, `free_fn`)
- Do not commit generated build outputs (`build/`)
