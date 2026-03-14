#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

// Helper: attempt parse with DOM parser, return true if succeeded
static bool dom_ok(std::string_view json) {
  try {
    Document doc;
    parse(doc, json);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

// Single quotes: not valid JSON, DOM parser rejects them
TEST(RelaxedParsing, SingleQuotesNotSupported) {
  EXPECT_FALSE(dom_ok("{'a': 'b'}"));
}

// Unquoted keys: not valid JSON, DOM parser rejects them
TEST(RelaxedParsing, UnquotedKeysNotSupported) {
  EXPECT_FALSE(dom_ok("{a: 1}"));
}

// Trailing commas: DOM parser accepts them
TEST(RelaxedParsing, TrailingCommasAccepted) {
  EXPECT_TRUE(dom_ok("[1, 2, ]"));
  EXPECT_TRUE(dom_ok("{\"a\": 1, }"));
}

// Duplicate keys: DOM parser accepts them (tape preserves all entries)
TEST(RelaxedParsing, DuplicateKeysAccepted) {
  EXPECT_TRUE(dom_ok(R"({"a": 1, "a": 2})"));
}

// Valid JSON: basic positive cases
TEST(RelaxedParsing, ValidJsonAccepted) {
  EXPECT_TRUE(dom_ok(R"({"key": "value"})"));
  EXPECT_TRUE(dom_ok("[1, 2, 3]"));
  EXPECT_TRUE(dom_ok("null"));
  EXPECT_TRUE(dom_ok("true"));
  EXPECT_TRUE(dom_ok("false"));
  EXPECT_TRUE(dom_ok("42"));
}
