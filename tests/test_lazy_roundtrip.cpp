#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace beast;

// Helper: parse + dump in one step
static std::string roundtrip(std::string_view json) {
  Document doc;
  return parse(doc, json).dump();
}

TEST(LazyRoundTrip, Scalars) {
  EXPECT_EQ(roundtrip("null"), "null");
  EXPECT_EQ(roundtrip("true"), "true");
  EXPECT_EQ(roundtrip("false"), "false");
  EXPECT_EQ(roundtrip("0"), "0");
  EXPECT_EQ(roundtrip("42"), "42");
  EXPECT_EQ(roundtrip("-99"), "-99");
  EXPECT_EQ(roundtrip("3.14"), "3.14");
  EXPECT_EQ(roundtrip("-1.5e10"), "-1.5e10");
}

TEST(LazyRoundTrip, StringBasic) {
  EXPECT_EQ(roundtrip(R"("hello")"), R"("hello")");
  EXPECT_EQ(roundtrip(R"("")"), R"("")");
}

TEST(LazyRoundTrip, StringEscapes) {
  EXPECT_EQ(roundtrip(R"("a\"b")"), R"("a\"b")");
  EXPECT_EQ(roundtrip(R"("a\\b")"), R"("a\\b")");
  EXPECT_EQ(roundtrip(R"("\n\t\r")"), R"("\n\t\r")");
}

TEST(LazyRoundTrip, UnicodeEscapes) {
  // Unicode escapes stored raw; re-emitted unchanged
  EXPECT_EQ(roundtrip(R"("\u0041")"), R"("\u0041")");
  EXPECT_EQ(roundtrip(R"("\u20AC")"), R"("\u20AC")");
  EXPECT_EQ(roundtrip(R"("\uD834\uDD1E")"), R"("\uD834\uDD1E")");
}

TEST(LazyRoundTrip, Arrays) {
  EXPECT_EQ(roundtrip("[]"), "[]");
  EXPECT_EQ(roundtrip("[1]"), "[1]");
  EXPECT_EQ(roundtrip("[1,2,3]"), "[1,2,3]");
  EXPECT_EQ(roundtrip("[\"a\",\"b\"]"), "[\"a\",\"b\"]");
  EXPECT_EQ(roundtrip("[[],[]]"), "[[],[]]");
  EXPECT_EQ(roundtrip("[[1,[2,3]]]"), "[[1,[2,3]]]");
}

TEST(LazyRoundTrip, Objects) {
  EXPECT_EQ(roundtrip("{}"), "{}");
  EXPECT_EQ(roundtrip(R"({"a":1})"), R"({"a":1})");
  EXPECT_EQ(roundtrip(R"({"a":1,"b":2})"), R"({"a":1,"b":2})");
  EXPECT_EQ(roundtrip(R"({"a":1,"b":2,"c":3})"), R"({"a":1,"b":2,"c":3})");
}

TEST(LazyRoundTrip, Nested) {
  EXPECT_EQ(roundtrip(R"({"a":[1,2],"b":{"c":3}})"),
            R"({"a":[1,2],"b":{"c":3}})");
  EXPECT_EQ(roundtrip(R"([{"x":1},{"y":2}])"), R"([{"x":1},{"y":2}])");
  EXPECT_EQ(roundtrip(R"({"outer":{"inner":[1,2,3]}})"),
            R"({"outer":{"inner":[1,2,3]}})");
}

TEST(LazyRoundTrip, MixedTypes) {
  std::string json = R"([null,true,false,0,-1,3.14,"str",{},[]])";
  EXPECT_EQ(roundtrip(json), json);
}

TEST(LazyRoundTrip, DeepNesting) {
  // Build deeply nested object; safe: depth=50 << kMaxDepth=1024
  std::string json;
  const int depth = 50;
  for (int i = 0; i < depth; ++i)
    json += "{\"a\":";
  json += "1";
  for (int i = 0; i < depth; ++i)
    json += "}";
  EXPECT_EQ(roundtrip(json), json);
}

TEST(LazyRoundTrip, AllPrimitiveTypes) {
  // Object containing every primitive type
  std::string json =
      R"({"null":null,"t":true,"f":false,"i":42,"n":-7,"d":1.5,"s":"hello"})";
  EXPECT_EQ(roundtrip(json), json);
}

TEST(LazyRoundTrip, StressMultipleParsesOnSameDoc) {
  Document doc;
  const std::vector<std::string> cases = {
      "null",
      "[1,2,3]",
      R"({"a":1})",
      "true",
      R"([null,false,0,"x"])",
  };
  for (const auto &json : cases) {
    auto v = parse(doc, json);
    EXPECT_EQ(v.dump(), json) << "Roundtrip failed for: " << json;
  }
}
