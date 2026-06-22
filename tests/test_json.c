#include <stdio.h>
#include <string.h>

#include "json.h"

static int failures = 0;

static void check(int condition, const char *name) {
  if (!condition) {
    printf("FAIL %s\n", name);
    failures++;
  }
}

static void expect_ok(const char *text, const char *name) {
  json_value root;
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
  json_value root;
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
  json_value root;
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

  st = json_path_get_long(&root, "data[0].items[0].size.width", &width);
  check(st == JSON_OK && width == 42, "bracket path long");

  st = json_path_get_long(&root, "data.0.items.0.size.width", &width);
  check(st == JSON_OK && width == 42, "dot numeric array shorthand");

  st = json_path_get_double(&root, "data[0].items[0].size.width", &width_d);
  check(st == JSON_OK && width_d == 42.0, "path double");

  st = json_path_get_string(&root, "metadata.file\\.name", &s, &len);
  check(st == JSON_OK && len == 4u && strncmp(s, "demo", len) == 0,
        "escaped dot path string");

  st = json_path_get_bool(&root, "flag", &flag);
  check(st == JSON_OK && flag == 1, "path bool");

  v = json_object_find(&root, "a");
  st = json_get_number(v, NULL, NULL, NULL, &width, &flag);
  check(st == JSON_OK && flag == 1 && width == 2, "duplicate lookup last");

  check(json_path_get(&root, "data[9]") == NULL, "missing path null");

  st = json_path_get_long(&root, "data[18446744073709551616]", &width);
  check(st == JSON_INVALID_PATH, "huge bracket index invalid");

  st = json_path_get_long(&root, "data.18446744073709551616", &width);
  check(st == JSON_INVALID_PATH, "huge dot index invalid");

  json_free(&root);
}

int main(void) {
  test_valid_basics();
  test_invalid_basics();
  test_accessors_and_paths();

  if (failures != 0) {
    printf("%d test(s) failed\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
