# Contributing

Thank you for improving `csjp`. This project is intentionally small: one public
header, strict JSON behavior, no dependency tree, and a compact test suite that
is easy to run locally.

## Project Principles

- Keep `json.h` the product. Do not add wrapper source files or generated
  distribution artifacts.
- Preserve ANSI C89 compatibility unless the project deliberately changes its
  portability target.
- Prefer explicit status returns and documented ownership over hidden global
  state or process-wide configuration.
- Keep allocation behavior simple: parsed DOM data belongs to the root arena and
  is released by `json_free`.
- Avoid dependencies. A new dependency should be treated as a major design
  decision, not a convenience.
- Keep CMake small. It exists to expose the header target and run local checks.

## Development Setup

No package manager setup is required. A C compiler and CMake are enough.

Run the direct test build:

```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests
./csjp_tests
```

Run the CMake build:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the example:

```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror example.c -I. -o csjp_example
./csjp_example
```

Run the benchmark smoke:

```sh
./build/csjp_bench 1000
```

## Code Style

- Write C89-compatible C.
- Keep public API declarations near the top of `json.h`.
- Keep implementation-only names prefixed with `json__`.
- Use `size_t` for sizes and byte offsets.
- Keep comments sparse and useful. Prefer clear code over explanatory noise.
- Do not introduce platform-specific code without a guarded fallback and tests.
- Preserve the single-header pattern:

```c
#include "json.h"
```

## Behavior To Preserve

Changes should not casually alter these V3 contracts:

- Strict parsing: trailing input, malformed literals, invalid numbers, invalid
  strings, bad escapes, and invalid UTF-8 must fail.
- Error offsets are zero-based byte offsets into the original input.
- Empty objects, empty arrays, empty strings, and `null` are valid.
- Duplicate object keys are preserved during iteration.
- `json_object_get` and `json_object_get_len` return the last matching key.
- Number source text is preserved.
- `as_long` is available only for exact `long` integers.
- `json_free(NULL)` is valid.
- A zeroed `json_value` is safe to free.
- Custom allocators are used only when all three hooks are provided.

## Tests

Add or update tests in `tests/test_json.c` for parser behavior changes. Prefer
small, named cases that document the contract they protect.

Use direct compiler tests for language-level portability:

```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests
cc -std=c99 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests_c99
cc -std=c11 -pedantic -Wall -Wextra -Werror tests/test_json.c -I. -o csjp_tests_c11
```

Run sanitizers when touching parser internals:

```sh
cc -std=c89 -pedantic -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer tests/test_json.c -I. -o csjp_tests_san
./csjp_tests_san
```

## Documentation

Update `README.md` whenever public behavior, integration steps, or API shape
changes. Keep examples buildable and short. Do not document unsupported package
manager, `find_package`, generated-source, or multi-file distribution flows
unless the repository actually supports them.

## Pull Request Checklist

Before proposing a change, verify:

- The public API remains intentional and minimal.
- The direct C89 test build passes.
- CMake configure, build, and CTest pass.
- New parser behavior has tests.
- README and contribution notes still match the repository.
- Benchmark numbers, if mentioned, are clearly scoped as local smoke signals.
- No generated build outputs are committed.

## Security Issues

Do not publish exploit details in a normal issue. Follow `SECURITY.md` for
private reporting guidance.
