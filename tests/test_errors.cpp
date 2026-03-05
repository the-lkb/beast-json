#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace beast;

// Helper: attempt lazy parse, return true on success
static bool lazy_ok(std::string_view j) {
  try {
    Document doc;
    parse(doc, j);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

// Unterminated containers: depth != 0 at end → parse error
TEST(ErrorHandling, UnterminatedContainers) {
  EXPECT_FALSE(lazy_ok("["));
  EXPECT_FALSE(lazy_ok("{"));
  EXPECT_FALSE(lazy_ok("[1, 2"));
  EXPECT_FALSE(lazy_ok("{\"a\":"));
  EXPECT_FALSE(lazy_ok("[[["));
}

// Invalid literals: prefix-length check fails
TEST(ErrorHandling, InvalidLiterals) {
  EXPECT_FALSE(lazy_ok("tru"));
  EXPECT_FALSE(lazy_ok("truth"));
  EXPECT_FALSE(lazy_ok("fal"));
  EXPECT_FALSE(lazy_ok("falsy"));
  EXPECT_FALSE(lazy_ok("nul"));
  EXPECT_FALSE(lazy_ok("nulls"));
}

// Empty input: skip_to_action returns 0 → parse error
TEST(ErrorHandling, EmptyInput) {
  EXPECT_FALSE(lazy_ok(""));
  EXPECT_FALSE(lazy_ok("   "));
}

// Unrecognized value-start chars
TEST(ErrorHandling, UnrecognizedValueChars) {
  EXPECT_FALSE(lazy_ok("[!]"));
  EXPECT_FALSE(lazy_ok("[?]"));
  EXPECT_FALSE(lazy_ok("[&]"));
}

// Unbalanced depth: unclosed containers
TEST(ErrorHandling, UnbalancedDepth) {
  EXPECT_FALSE(lazy_ok("[1,2"));
  EXPECT_FALSE(lazy_ok("{\"a\":1"));
  EXPECT_FALSE(lazy_ok("{\"key\":\"value\""));
}
