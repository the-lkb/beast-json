#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace beast;

// lazy parser accepts duplicate keys (tape preserves all entries)
TEST(DuplicateKeys, AlwaysAccepted) {
  Document doc;
  EXPECT_NO_THROW(parse(doc, R"({"key": 1, "key": 2})"));
  EXPECT_NO_THROW(parse(doc, R"({"a": 1, "b": 2, "a": 3, "a": 99})"));
}

TEST(DuplicateKeys, LazyRoundTrip) {
  std::string json = R"({"a":1,"a":2,"b":3})";
  Document doc;
  auto v = parse(doc, json);
  // dump() preserves all occurrences from the tape
  EXPECT_EQ(v.dump(), json);
}

// Multiple duplicates: all entries preserved in tape
TEST(DuplicateKeys, MultipleDuplicatesPreserved) {
  std::string json = R"({"x":1,"x":2,"x":3})";
  Document doc;
  auto v = parse(doc, json);
  EXPECT_EQ(v.dump(), json);
}
