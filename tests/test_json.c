#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "json.h"

static int failures = 0;

static void check(int condition, const char *name) {
  if (!condition) {
    printf("FAIL %s\n", name);
    failures++;
  }
}

static void expect_ok(const char *text, const char *name) {
  json_root root;
  json_error err;
  json_status st;
  st = json_parse(text, &root, &err);
  if (st != JSON_OK) {
    printf("FAIL %s: %s at %lu\n", name, json_error_to_string(st),
           (unsigned long)err.offset);
    failures++;
    return;
  }
  json_free(&root);
}

static void expect_error(const char *text, json_status want, const char *name) {
  json_root root;
  json_error err;
  json_status st;
  st = json_parse(text, &root, &err);
  if (st != want) {
    printf("FAIL %s: got %s at %lu, wanted %s\n", name,
           json_error_to_string(st), (unsigned long)err.offset,
           json_error_to_string(want));
    failures++;
    if (st == JSON_OK) {
      json_free(&root);
    }
  }
}

static void test_valid_basics(void) {
  expect_ok("null", "null parses");
  expect_ok("\"\"", "empty string parses");
  expect_ok("[]", "empty array parses");
  expect_ok("{}", "empty object parses");
  expect_ok("{\"a\":1,\"a\":2}", "duplicate keys parse");
  expect_ok("{\"u\":\"\\uD834\\uDD1E\"}", "surrogate pair parses");
  expect_ok("\"valid \342\202\254 utf8\"", "raw utf8 parses");
}

static void test_invalid_basics(void) {
  expect_error("truexxx", JSON_INVALID_LITERAL, "trailing literal junk");
  expect_error("tru", JSON_INVALID_LITERAL, "short true rejected");
  expect_error("\"\\", JSON_INVALID_ESCAPE, "trailing string slash");
  expect_error("\"\\g\"", JSON_INVALID_ESCAPE, "bad escape rejected");
  expect_error("{\"x\":n\303\276ll}", JSON_INVALID_LITERAL,
               "bad null rejected");
  expect_error("+1", JSON_EXPECTED_VALUE, "plus number rejected");
  expect_error(".1", JSON_EXPECTED_VALUE, "dot number rejected");
  expect_error("01", JSON_INVALID_NUMBER, "leading zero rejected");
  expect_error("1e", JSON_INVALID_NUMBER, "bad exponent rejected");
  expect_error("[1,]", JSON_EXPECTED_VALUE, "trailing array comma rejected");
  expect_error("\"\377\"", JSON_INVALID_UNICODE, "raw invalid utf8 rejected");
  expect_error("\"\300\200\"", JSON_INVALID_UNICODE,
               "raw overlong utf8 rejected");
}

static void test_accessors_and_paths(void) {
  json_root root;
  json_error err;
  json_status st;
  const json_value *v;
  const char *s;
  size_t len;
  long width;
  double width_d;
  int flag;
  const char *text;

  text = "{\"data\":[{\"items\":[{\"size\":{\"width\":42,\"height\":7}}]}],"
         "\"metadata\":{\"file.name\":\"demo\"},\"flag\":true,"
         "\"a\":1,\"a\":2}";
  st = json_parse(text, &root, &err);
  check(st == JSON_OK, "path fixture parses");
  if (st != JSON_OK) {
    return;
  }

  st = json_path_get_long(&root.value, "data[0].items[0].size.width", &width);
  check(st == JSON_OK && width == 42, "bracket path long");

  st = json_path_get_long(&root.value, "data.0.items.0.size.width", &width);
  check(st == JSON_OK && width == 42, "dot numeric array shorthand");

  st = json_path_get_double(&root.value, "data[0].items[0].size.width",
                            &width_d);
  check(st == JSON_OK && width_d == 42.0, "path double");

  st = json_path_get_string(&root.value, "metadata.file\\.name", &s, &len);
  check(st == JSON_OK && len == 4u && strncmp(s, "demo", len) == 0,
        "escaped dot path string");

  st = json_path_get_bool(&root.value, "flag", &flag);
  check(st == JSON_OK && flag == 1, "path bool");

  v = json_object_get(&root.value, "a");
  st = json_number_get(v, NULL, NULL, NULL, &width, &flag);
  check(st == JSON_OK && flag == 1 && width == 2, "duplicate lookup last");

  check(json_path_get(&root.value, "data[9]") == NULL, "missing path null");

  st = json_path_get_long(&root.value, "data[18446744073709551616]", &width);
  check(st == JSON_INVALID_PATH, "huge bracket index invalid");

  st = json_path_get_long(&root.value, "data.18446744073709551616", &width);
  check(st == JSON_INVALID_PATH, "huge dot index invalid");

  json_free(&root);
}

static void test_depth_limit(void) {
  char buf[1024];
  size_t i;
  json_root root;
  json_error err;
  json_status st;
  json_config cfg;

  cfg.max_depth = 4u;
  cfg.allocator.malloc_fn = NULL;
  cfg.allocator.realloc_fn = NULL;
  cfg.allocator.free_fn = NULL;
  cfg.allocator.ctx = NULL;

  /* 4 levels deep: [[[[1]]]] — arrays count depth starting at 0 */
  /* depth check triggers when depth > max_depth, so max_depth=4 allows 4 */
  buf[0] = '\0';
  for (i = 0u; i < 4u; i++) strcat(buf, "[");
  strcat(buf, "1");
  for (i = 0u; i < 4u; i++) strcat(buf, "]");
  st = json_parse_len(buf, strlen(buf), &cfg, &root, &err);
  check(st == JSON_OK, "depth at limit parses");
  if (st == JSON_OK) json_free(&root);

  /* 5 levels deep: exceeds max_depth=4 */
  buf[0] = '\0';
  for (i = 0u; i < 5u; i++) strcat(buf, "[");
  strcat(buf, "1");
  for (i = 0u; i < 5u; i++) strcat(buf, "]");
  st = json_parse_len(buf, strlen(buf), &cfg, &root, &err);
  check(st == JSON_DEPTH_LIMIT, "depth over limit rejected");
}

/* --- counting allocator for custom allocator test --- */
static int g_alloc_count = 0;
static int g_free_count = 0;

static void *count_malloc(void *ctx, size_t size) {
  (void)ctx;
  g_alloc_count++;
  return malloc(size);
}
static void *count_realloc(void *ctx, void *ptr, size_t old_size,
                           size_t new_size) {
  (void)ctx;
  (void)old_size;
  g_alloc_count++;
  if (ptr != NULL) g_free_count++;
  return realloc(ptr, new_size);
}
static void count_free(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  g_free_count++;
  free(ptr);
}

static void test_custom_allocator(void) {
  json_root root;
  json_error err;
  json_status st;
  json_config cfg;
  int alloc_before;
  int free_before;

  cfg.max_depth = 0u;
  cfg.allocator.ctx = NULL;
  cfg.allocator.malloc_fn = count_malloc;
  cfg.allocator.realloc_fn = count_realloc;
  cfg.allocator.free_fn = count_free;

  g_alloc_count = 0;
  g_free_count = 0;

  alloc_before = g_alloc_count;

  st = json_parse_len("{\"x\":1,\"y\":[1,2,3]}", 19u, &cfg, &root, &err);
  check(st == JSON_OK, "custom allocator parse succeeds");
  check(g_alloc_count > alloc_before, "custom malloc was called");

  alloc_before = g_alloc_count;
  free_before = g_free_count;
  json_free(&root);
  check(g_free_count > free_before, "custom free was called on json_free");
}

static void test_error_offsets(void) {
  json_root root;
  json_error err;
  json_status st;

  st = json_parse("   @", &root, &err);
  check(st == JSON_EXPECTED_VALUE && err.offset == 3u,
        "error offset at bad char");

  st = json_parse("{\"k\":}", &root, &err);
  check(st == JSON_EXPECTED_VALUE && err.offset == 5u,
        "error offset missing value");

  st = json_parse("[1, 2  3]", &root, &err);
  check(st == JSON_EXPECTED_COMMA_OR_END && err.offset == 7u,
        "error offset missing comma");

  st = json_parse("nul", &root, &err);
  check(st == JSON_INVALID_LITERAL && err.offset == 0u,
        "error offset bad literal");

  st = json_parse("\"\\q\"", &root, &err);
  check(st == JSON_INVALID_ESCAPE && err.offset == 2u,
        "error offset bad escape");

  st = json_parse("\"\\uGGGG\"", &root, &err);
  check(st == JSON_INVALID_UNICODE && err.offset == 3u,
        "error offset bad unicode");
}

static void test_number_boundaries(void) {
  char buf[64];
  json_root root;
  json_error err;
  json_status st;
  long lv;
  double dv;
  int is_long;

  /* LONG_MAX */
  snprintf(buf, sizeof(buf), "%ld", (long)LONG_MAX);
  st = json_parse(buf, &root, &err);
  check(st == JSON_OK, "LONG_MAX parses");
  if (st == JSON_OK) {
    st = json_number_get_long(&root.value, &lv);
    check(st == JSON_OK && lv == LONG_MAX, "LONG_MAX is_long and correct");
    json_free(&root);
  }

  /* LONG_MIN */
  snprintf(buf, sizeof(buf), "%ld", (long)LONG_MIN);
  st = json_parse(buf, &root, &err);
  check(st == JSON_OK, "LONG_MIN parses");
  if (st == JSON_OK) {
    st = json_number_get_long(&root.value, &lv);
    check(st == JSON_OK && lv == LONG_MIN, "LONG_MIN is_long and correct");
    json_free(&root);
  }

  /* LONG_MAX + 1 as a string — overflows long, should parse but is_long=0 */
  {
    unsigned long over;
    over = (unsigned long)LONG_MAX + 1ul;
    snprintf(buf, sizeof(buf), "%lu", over);
  }
  st = json_parse(buf, &root, &err);
  check(st == JSON_OK, "LONG_MAX+1 parses");
  if (st == JSON_OK) {
    json_number_get(&root.value, NULL, NULL, NULL, NULL, &is_long);
    check(!is_long, "LONG_MAX+1 is_long is 0");
    json_free(&root);
  }

  /* -0 parses as a valid number */
  st = json_parse("-0", &root, &err);
  check(st == JSON_OK, "-0 parses");
  if (st == JSON_OK) json_free(&root);

  /* 1e309 parses (double inf) */
  st = json_parse("1e309", &root, &err);
  check(st == JSON_OK, "1e309 parses");
  if (st == JSON_OK) {
    json_number_get_double(&root.value, &dv);
    check(dv > 1e308, "1e309 as_double is huge");
    json_free(&root);
  }

  /* number source text is preserved verbatim */
  {
    const char *txt;
    size_t tlen;
    txt = NULL;
    tlen = 0u;
    st = json_parse("42", &root, &err);
    check(st == JSON_OK, "number text: parse ok");
    if (st == JSON_OK) {
      st = json_number_get(&root.value, &txt, &tlen, NULL, NULL, NULL);
      check(st == JSON_OK, "number text: get ok");
      check(txt != NULL && tlen == 2u && strncmp(txt, "42", tlen) == 0,
            "number text preserved");
      json_free(&root);
    }
  }
}

static void test_empty_key(void) {
  json_root root;
  json_error err;
  json_status st;
  const json_value *v;
  long lv;

  st = json_parse("{\"\":42}", &root, &err);
  check(st == JSON_OK, "empty key object parses");
  if (st != JSON_OK) return;

  v = json_object_get(&root.value, "");
  check(v != NULL, "empty key lookup returns value");
  if (v != NULL) {
    json_number_get_long(v, &lv);
    check(lv == 42, "empty key value correct");
  }

  json_free(&root);
}

static void test_object_iteration(void) {
  json_root root;
  json_error err;
  json_status st;
  const json_value *v;
  const char *k;
  size_t klen;
  long lv;
  size_t n;

  st = json_parse("{\"a\":1,\"b\":2,\"c\":3}", &root, &err);
  check(st == JSON_OK, "iteration fixture parses");
  if (st != JSON_OK) return;

  n = json_object_size(&root.value);
  check(n == 3u, "object size is 3");

  /* json_object_key_at + json_object_value_at */
  k = json_object_key_at(&root.value, 0u, &klen);
  check(k != NULL && klen == 1u && k[0] == 'a', "key_at 0 is 'a'");
  v = json_object_value_at(&root.value, 0u);
  check(v != NULL, "value_at 0 non-null");
  if (v) { json_number_get_long(v, &lv); check(lv == 1, "value_at 0 is 1"); }

  k = json_object_key_at(&root.value, 1u, &klen);
  check(k != NULL && klen == 1u && k[0] == 'b', "key_at 1 is 'b'");

  /* json_object_member_at */
  v = json_object_member_at(&root.value, 2u, &k, &klen);
  check(v != NULL && k != NULL && klen == 1u && k[0] == 'c',
        "member_at 2 key is 'c'");
  if (v) { json_number_get_long(v, &lv); check(lv == 3, "member_at 2 value is 3"); }

  /* out-of-bounds */
  check(json_object_key_at(&root.value, 3u, NULL) == NULL,
        "key_at out-of-bounds null");
  check(json_object_value_at(&root.value, 3u) == NULL,
        "value_at out-of-bounds null");
  check(json_object_member_at(&root.value, 3u, NULL, NULL) == NULL,
        "member_at out-of-bounds null");

  json_free(&root);
}

static void test_typed_number_helpers(void) {
  json_root root;
  json_error err;
  json_status st;
  double dv;
  long lv;

  st = json_parse("3.14", &root, &err);
  check(st == JSON_OK, "float parses for helpers");
  if (st == JSON_OK) {
    st = json_number_get_double(&root.value, &dv);
    check(st == JSON_OK && dv > 3.13 && dv < 3.15,
          "number_get_double correct");
    st = json_number_get_long(&root.value, &lv);
    check(st == JSON_WRONG_TYPE, "number_get_long on float is WRONG_TYPE");
    json_free(&root);
  }

  st = json_parse("7", &root, &err);
  check(st == JSON_OK, "int parses for helpers");
  if (st == JSON_OK) {
    st = json_number_get_long(&root.value, &lv);
    check(st == JSON_OK && lv == 7, "number_get_long on int correct");
    json_free(&root);
  }

  /* wrong type: call number helpers on a string */
  st = json_parse("\"hello\"", &root, &err);
  check(st == JSON_OK, "string parses for wrong-type test");
  if (st == JSON_OK) {
    check(json_number_get_double(&root.value, &dv) == JSON_WRONG_TYPE,
          "number_get_double on string is WRONG_TYPE");
    check(json_number_get_long(&root.value, &lv) == JSON_WRONG_TYPE,
          "number_get_long on string is WRONG_TYPE");
    json_free(&root);
  }
}

static void test_validate_only(void) {
  json_error err;
  json_status st;

  /* valid JSON with out=NULL — should succeed (validate only) */
  st = json_parse_len("{\"x\":1}", 7u, NULL, NULL, &err);
  check(st == JSON_OK, "validate-only valid JSON returns OK");

  /* invalid JSON with out=NULL — should return parse error */
  st = json_parse_len("{bad}", 5u, NULL, NULL, &err);
  check(st != JSON_OK, "validate-only invalid JSON returns error");
}

static void test_parse_len_config(void) {
  json_root root;
  json_error err;
  json_status st;
  json_config cfg;
  const char *deep;

  cfg.max_depth = 2u;
  cfg.allocator.malloc_fn = NULL;
  cfg.allocator.realloc_fn = NULL;
  cfg.allocator.free_fn = NULL;
  cfg.allocator.ctx = NULL;

  /* [[[1]]] = 3 nested arrays (depth 1, 2, 3) with the value at depth 3
     depth check: triggers when depth > max_depth (2), so depth 3 > 2 fails */
  deep = "[[[1]]]";
  st = json_parse_len(deep, strlen(deep), &cfg, &root, &err);
  check(st == JSON_DEPTH_LIMIT, "custom max_depth=2 rejects depth 3");

  deep = "[[1]]";
  st = json_parse_len(deep, strlen(deep), &cfg, &root, &err);
  check(st == JSON_OK, "custom max_depth=2 allows depth 2");
  if (st == JSON_OK) json_free(&root);
}

static void test_trailing_input(void) {
  json_root root;
  json_error err;
  json_status st;
  memset(&root, 0, sizeof(root));
  memset(&err, 0, sizeof(err));
  st = json_parse("null extra", &root, &err);
  check(st == JSON_TRAILING_INPUT, "trailing input fails");
  check(err.offset == 5u, "trailing input offset");
  json_free(&root);
}

static void test_free_safety(void) {
  json_root r;
  json_free(NULL);  /* must not crash */
  memset(&r, 0, sizeof(r));
  json_free(&r);    /* must not crash */
}

int main(void) {
  test_valid_basics();
  test_invalid_basics();
  test_accessors_and_paths();
  test_depth_limit();
  test_custom_allocator();
  test_error_offsets();
  test_number_boundaries();
  test_empty_key();
  test_object_iteration();
  test_typed_number_helpers();
  test_validate_only();
  test_parse_len_config();
  test_trailing_input();
  test_free_safety();

  if (failures != 0) {
    printf("%d test(s) failed\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
