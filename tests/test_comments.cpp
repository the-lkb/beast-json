#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string_view>

using namespace qbuem;

// NOTE: DOM parser does not support C-style comments.
// '/' is not a recognized JSON token; any JSON with comment syntax fails.

static bool dom_ok(std::string_view json) {
  try {
    Document doc;
    parse(doc, json);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

TEST(Comments, SingleLineCommentFails) {
  EXPECT_FALSE(dom_ok("{\"a\": 1 // comment\n}"));
}

TEST(Comments, BlockCommentFails) {
  EXPECT_FALSE(dom_ok("{\"a\": 1 /* comment */ }"));
}

TEST(Comments, LeadingSlashFails) {
  EXPECT_FALSE(dom_ok("// start\n{\"a\": 1}"));
}

// Valid JSON without comments is accepted
TEST(Comments, ValidJsonAccepted) {
  EXPECT_TRUE(dom_ok(R"({"a": 1, "b": 2})"));
  EXPECT_TRUE(dom_ok("[1, 2, 3]"));
}
