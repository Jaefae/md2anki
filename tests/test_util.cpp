#include "util.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("trim removes leading and trailing whitespace", "[util]") {
  CHECK(trim("  hello  ") == "hello");
  CHECK(trim("hello") == "hello");
  CHECK(trim("   ") == "");
  CHECK(trim("") == "");
  CHECK(trim("\t\nhello\r\n") == "hello");
}

TEST_CASE("ltrim only strips leading whitespace", "[util]") {
  CHECK(ltrim("  hello  ") == "hello  ");
  CHECK(ltrim("hello") == "hello");
}

TEST_CASE("rtrim only strips trailing whitespace", "[util]") {
  CHECK(rtrim("  hello  ") == "  hello");
  CHECK(rtrim("hello") == "hello");
}

TEST_CASE("toLower lowercases ASCII input", "[util]") {
  CHECK(toLower("HeLLo World") == "hello world");
  CHECK(toLower("") == "");
  CHECK(toLower("already lower") == "already lower");
}
