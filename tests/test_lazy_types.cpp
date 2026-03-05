#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace beast;

// Helper: parse json and return Value; reuses doc
static Value parse_l(Document &doc, std::string_view json) {
  return parse(doc, json);
}

TEST(LazyTypes, Null) {
  Document doc;
  EXPECT_NO_THROW(parse_l(doc, "null"));
  EXPECT_EQ(parse_l(doc, "null").dump(), "null");
}

TEST(LazyTypes, BooleanTrue) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "true").dump(), "true");
}

TEST(LazyTypes, BooleanFalse) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "false").dump(), "false");
}

TEST(LazyTypes, IntegerZero) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "0").dump(), "0");
}

TEST(LazyTypes, IntegerPositive) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "123").dump(), "123");
}

TEST(LazyTypes, IntegerNegative) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "-456").dump(), "-456");
}

TEST(LazyTypes, FloatSimple) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "3.14").dump(), "3.14");
}

TEST(LazyTypes, FloatNegative) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "-0.5").dump(), "-0.5");
}

TEST(LazyTypes, FloatExponent) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "1.5e2").dump(), "1.5e2");
}

TEST(LazyTypes, StringEmpty) {
  Document doc;
  EXPECT_EQ(parse_l(doc, R"("")").dump(), R"("")");
}

TEST(LazyTypes, StringSimple) {
  Document doc;
  EXPECT_EQ(parse_l(doc, R"("hello")").dump(), R"("hello")");
}

TEST(LazyTypes, StringWithEscape) {
  Document doc;
  EXPECT_EQ(parse_l(doc, R"("a\\b")").dump(), R"("a\\b")");
}

TEST(LazyTypes, EmptyArray) {
  Document doc;
  auto v = parse_l(doc, "[]");
  EXPECT_TRUE(v.is_array());
  EXPECT_FALSE(v.is_object());
  EXPECT_EQ(v.dump(), "[]");
}

TEST(LazyTypes, ArrayWithElements) {
  Document doc;
  auto v = parse_l(doc, "[1,2,3]");
  EXPECT_TRUE(v.is_array());
  EXPECT_EQ(v.dump(), "[1,2,3]");
}

TEST(LazyTypes, EmptyObject) {
  Document doc;
  auto v = parse_l(doc, "{}");
  EXPECT_TRUE(v.is_object());
  EXPECT_FALSE(v.is_array());
  EXPECT_EQ(v.dump(), "{}");
}

TEST(LazyTypes, ObjectWithPair) {
  Document doc;
  auto v = parse_l(doc, R"({"a":1})");
  EXPECT_TRUE(v.is_object());
  EXPECT_EQ(v.dump(), R"({"a":1})");
}

TEST(LazyTypes, NestedArrayInObject) {
  Document doc;
  std::string json = R"({"arr":[1,2,3]})";
  auto v = parse_l(doc, json);
  EXPECT_TRUE(v.is_object());
  EXPECT_EQ(v.dump(), json);
}

TEST(LazyTypes, NestedObjectInArray) {
  Document doc;
  std::string json = R"([{"a":1},{"b":2}])";
  auto v = parse_l(doc, json);
  EXPECT_TRUE(v.is_array());
  EXPECT_EQ(v.dump(), json);
}

// DocumentView reuse: multiple successive parses share the same tape arena
TEST(LazyTypes, DocumentViewReuse) {
  Document doc;

  auto v1 = parse(doc, "null");
  EXPECT_EQ(v1.dump(), "null");

  auto v2 = parse(doc, "[1,2]");
  EXPECT_EQ(v2.dump(), "[1,2]");

  auto v3 = parse(doc, R"({"x":42})");
  EXPECT_EQ(v3.dump(), R"({"x":42})");

  auto v4 = parse(doc, "true");
  EXPECT_EQ(v4.dump(), "true");
}
