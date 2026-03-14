#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string_view>

using namespace qbuem;

// parse accepts trailing commas
TEST(TrailingCommas, ArrayTrailingCommaAccepted) {
  Document doc;
  EXPECT_NO_THROW(parse(doc, "[1, 2, 3, ]"));
  EXPECT_NO_THROW(parse(doc, "[\"a\", ]"));
  EXPECT_NO_THROW(parse(doc, "[[], ]"));
}

TEST(TrailingCommas, ObjectTrailingCommaAccepted) {
  Document doc;
  EXPECT_NO_THROW(parse(doc, "{\"a\": 1, }"));
  EXPECT_NO_THROW(parse(doc, "{\"a\": {\"b\": 1, }, }"));
}

TEST(TrailingCommas, DOMParserAcceptsTrailingComma) {
  std::string arr = "[1, 2, ]";
  Document doc;
  EXPECT_NO_THROW(parse(doc, arr));

  std::string obj = "{\"k\": 1, }";
  EXPECT_NO_THROW(parse(doc, obj));
}

// Round-trip: dump() emits compact JSON without trailing commas
TEST(TrailingCommas, RoundTripPreservesStructure) {
  Document doc;
  auto v = parse(doc, "[1,2,]");
  std::string out = v.dump();
  EXPECT_EQ(out, "[1,2]");
}
