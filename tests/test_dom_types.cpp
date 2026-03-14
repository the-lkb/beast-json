#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

// Helper: parse json and return Value; reuses doc
static Value parse_l(Document &doc, std::string_view json) {
  return parse(doc, json);
}

TEST(DOMTypes, Null) {
  Document doc;
  EXPECT_NO_THROW(parse_l(doc, "null"));
  EXPECT_EQ(parse_l(doc, "null").dump(), "null");
}

TEST(DOMTypes, BooleanTrue) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "true").dump(), "true");
}

TEST(DOMTypes, BooleanFalse) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "false").dump(), "false");
}

TEST(DOMTypes, IntegerZero) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "0").dump(), "0");
}

TEST(DOMTypes, IntegerPositive) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "123").dump(), "123");
}

TEST(DOMTypes, IntegerNegative) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "-456").dump(), "-456");
}

TEST(DOMTypes, FloatSimple) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "3.14").dump(), "3.14");
}

TEST(DOMTypes, FloatNegative) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "-0.5").dump(), "-0.5");
}

TEST(DOMTypes, FloatExponent) {
  Document doc;
  EXPECT_EQ(parse_l(doc, "1.5e2").dump(), "1.5e2");
}

TEST(DOMTypes, StringEmpty) {
  Document doc;
  EXPECT_EQ(parse_l(doc, R"("")").dump(), R"("")");
}

TEST(DOMTypes, StringSimple) {
  Document doc;
  EXPECT_EQ(parse_l(doc, R"("hello")").dump(), R"("hello")");
}

TEST(DOMTypes, StringWithEscape) {
  Document doc;
  EXPECT_EQ(parse_l(doc, R"("a\\b")").dump(), R"("a\\b")");
}

TEST(DOMTypes, EmptyArray) {
  Document doc;
  auto v = parse_l(doc, "[]");
  EXPECT_TRUE(v.is_array());
  EXPECT_FALSE(v.is_object());
  EXPECT_EQ(v.dump(), "[]");
}

TEST(DOMTypes, ArrayWithElements) {
  Document doc;
  auto v = parse_l(doc, "[1,2,3]");
  EXPECT_TRUE(v.is_array());
  EXPECT_EQ(v.dump(), "[1,2,3]");
}

TEST(DOMTypes, EmptyObject) {
  Document doc;
  auto v = parse_l(doc, "{}");
  EXPECT_TRUE(v.is_object());
  EXPECT_FALSE(v.is_array());
  EXPECT_EQ(v.dump(), "{}");
}

TEST(DOMTypes, ObjectWithPair) {
  Document doc;
  auto v = parse_l(doc, R"({"a":1})");
  EXPECT_TRUE(v.is_object());
  EXPECT_EQ(v.dump(), R"({"a":1})");
}

TEST(DOMTypes, NestedArrayInObject) {
  Document doc;
  std::string json = R"({"arr":[1,2,3]})";
  auto v = parse_l(doc, json);
  EXPECT_TRUE(v.is_object());
  EXPECT_EQ(v.dump(), json);
}

TEST(DOMTypes, NestedObjectInArray) {
  Document doc;
  std::string json = R"([{"a":1},{"b":2}])";
  auto v = parse_l(doc, json);
  EXPECT_TRUE(v.is_array());
  EXPECT_EQ(v.dump(), json);
}

// DocumentView reuse: multiple successive parses share the same tape arena
TEST(DOMTypes, DocumentViewReuse) {
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
