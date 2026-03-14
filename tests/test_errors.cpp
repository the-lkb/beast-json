#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

// Helper: attempt dom parse, return true on success
static bool dom_ok(std::string_view j) {
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
  EXPECT_FALSE(dom_ok("["));
  EXPECT_FALSE(dom_ok("{"));
  EXPECT_FALSE(dom_ok("[1, 2"));
  EXPECT_FALSE(dom_ok("{\"a\":"));
  EXPECT_FALSE(dom_ok("[[["));
}

// Invalid literals: prefix-length check fails
TEST(ErrorHandling, InvalidLiterals) {
  EXPECT_FALSE(dom_ok("tru"));
  EXPECT_FALSE(dom_ok("truth"));
  EXPECT_FALSE(dom_ok("fal"));
  EXPECT_FALSE(dom_ok("falsy"));
  EXPECT_FALSE(dom_ok("nul"));
  EXPECT_FALSE(dom_ok("nulls"));
}

// Empty input: skip_to_action returns 0 → parse error
TEST(ErrorHandling, EmptyInput) {
  EXPECT_FALSE(dom_ok(""));
  EXPECT_FALSE(dom_ok("   "));
}

// Unrecognized value-start chars
TEST(ErrorHandling, UnrecognizedValueChars) {
  EXPECT_FALSE(dom_ok("[!]"));
  EXPECT_FALSE(dom_ok("[?]"));
  EXPECT_FALSE(dom_ok("[&]"));
}

// Unbalanced depth: unclosed containers
TEST(ErrorHandling, UnbalancedDepth) {
  EXPECT_FALSE(dom_ok("[1,2"));
  EXPECT_FALSE(dom_ok("{\"a\":1"));
  EXPECT_FALSE(dom_ok("{\"key\":\"value\""));
}
