#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

// Helper: attempt parse with lazy parser, return true if succeeded
static bool lazy_ok(std::string_view json) {
  try {
    Document doc;
    parse(doc, json);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

// Single quotes: not valid JSON, lazy parser rejects them
TEST(RelaxedParsing, SingleQuotesNotSupported) {
  EXPECT_FALSE(lazy_ok("{'a': 'b'}"));
}

// Unquoted keys: not valid JSON, lazy parser rejects them
TEST(RelaxedParsing, UnquotedKeysNotSupported) {
  EXPECT_FALSE(lazy_ok("{a: 1}"));
}

// Trailing commas: lazy parser accepts them
TEST(RelaxedParsing, TrailingCommasAccepted) {
  EXPECT_TRUE(lazy_ok("[1, 2, ]"));
  EXPECT_TRUE(lazy_ok("{\"a\": 1, }"));
}

// Duplicate keys: lazy parser accepts them (tape preserves all entries)
TEST(RelaxedParsing, DuplicateKeysAccepted) {
  EXPECT_TRUE(lazy_ok(R"({"a": 1, "a": 2})"));
}

// Valid JSON: basic positive cases
TEST(RelaxedParsing, ValidJsonAccepted) {
  EXPECT_TRUE(lazy_ok(R"({"key": "value"})"));
  EXPECT_TRUE(lazy_ok("[1, 2, 3]"));
  EXPECT_TRUE(lazy_ok("null"));
  EXPECT_TRUE(lazy_ok("true"));
  EXPECT_TRUE(lazy_ok("false"));
  EXPECT_TRUE(lazy_ok("42"));
}
