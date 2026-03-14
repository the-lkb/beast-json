#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

using namespace qbuem;

// ── Helpers ───────────────────────────────────────────────────────────────────

static Value parse_root(Document &doc, std::string_view json) {
  return parse(doc, json);
}

// ── Type checkers ─────────────────────────────────────────────────────────────

TEST(ValueAccessors, TypeCheckers) {
  Document doc;
  {
    auto v = parse_root(doc, "null");
    EXPECT_TRUE(v.is_null());
    EXPECT_FALSE(v.is_bool());
    EXPECT_FALSE(v.is_int());
    EXPECT_FALSE(v.is_string());
  }
  {
    auto v = parse_root(doc, "true");
    EXPECT_TRUE(v.is_bool());
    EXPECT_FALSE(v.is_null());
  }
  {
    auto v = parse_root(doc, "false");
    EXPECT_TRUE(v.is_bool());
  }
  {
    auto v = parse_root(doc, "42");
    EXPECT_TRUE(v.is_int());
    EXPECT_TRUE(v.is_number());
    EXPECT_FALSE(v.is_double());
  }
  {
    auto v = parse_root(doc, "3.14");
    EXPECT_TRUE(v.is_number());
    EXPECT_FALSE(v.is_int());
  }
  {
    auto v = parse_root(doc, "\"hello\"");
    EXPECT_TRUE(v.is_string());
    EXPECT_FALSE(v.is_int());
  }
  {
    auto v = parse_root(doc, "{}");
    EXPECT_TRUE(v.is_object());
    EXPECT_FALSE(v.is_array());
  }
  {
    auto v = parse_root(doc, "[]");
    EXPECT_TRUE(v.is_array());
    EXPECT_FALSE(v.is_object());
  }
}

// ── as<T>(): typed extraction ─────────────────────────────────────────────────

TEST(ValueAccessors, AsInt) {
  Document doc;
  auto v = parse_root(doc, "42");
  EXPECT_EQ(v.as<int64_t>(), 42);
  EXPECT_EQ(v.as<int>(), 42);
  EXPECT_EQ(v.as<unsigned>(), 42u);
}

TEST(ValueAccessors, AsNegativeInt) {
  Document doc;
  auto v = parse_root(doc, "-7");
  EXPECT_EQ(v.as<int64_t>(), -7);
  EXPECT_EQ(v.as<int>(), -7);
}

TEST(ValueAccessors, AsDouble) {
  Document doc;
  auto v = parse_root(doc, "3.14");
  EXPECT_NEAR(v.as<double>(), 3.14, 1e-9);
}

TEST(ValueAccessors, AsDoubleFromInt) {
  Document doc;
  auto v = parse_root(doc, "10");
  EXPECT_NEAR(v.as<double>(), 10.0, 1e-9);
}

TEST(ValueAccessors, AsBool) {
  Document doc;
  EXPECT_TRUE(parse_root(doc, "true").as<bool>());
  EXPECT_FALSE(parse_root(doc, "false").as<bool>());
}

TEST(ValueAccessors, AsStringView) {
  Document doc;
  auto v = parse_root(doc, "\"beast\"");
  EXPECT_EQ(v.as<std::string_view>(), "beast");
}

TEST(ValueAccessors, AsString) {
  Document doc;
  auto v = parse_root(doc, "\"hello world\"");
  EXPECT_EQ(v.as<std::string>(), "hello world");
}

TEST(ValueAccessors, AsTypeMismatchThrows) {
  Document doc;
  auto v = parse_root(doc, "\"not a number\"");
  EXPECT_THROW(v.as<int64_t>(), std::runtime_error);
  EXPECT_THROW(v.as<bool>(), std::runtime_error);
}

// ── try_as<T>(): safe optional extraction ─────────────────────────────────────

TEST(ValueAccessors, TryAsSuccess) {
  Document doc;
  auto v = parse_root(doc, "99");
  auto result = v.try_as<int64_t>();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 99);
}

TEST(ValueAccessors, TryAsFailure) {
  Document doc;
  auto v = parse_root(doc, "\"text\"");
  EXPECT_FALSE(v.try_as<int64_t>().has_value());
  EXPECT_FALSE(v.try_as<double>().has_value());
  EXPECT_FALSE(v.try_as<bool>().has_value());
}

TEST(ValueAccessors, TryAsBool) {
  Document doc;
  EXPECT_EQ(parse_root(doc, "true").try_as<bool>(), std::optional<bool>(true));
  EXPECT_EQ(parse_root(doc, "false").try_as<bool>(), std::optional<bool>(false));
  EXPECT_FALSE(parse_root(doc, "0").try_as<bool>().has_value());
}

// ── operator[](key): object access ───────────────────────────────────────────

TEST(ValueAccessors, ObjectSubscript) {
  Document doc;
  auto root = parse_root(doc, R"({"age": 30, "name": "Alice"})");
  EXPECT_EQ(root["age"].as<int>(), 30);
  EXPECT_EQ(root["name"].as<std::string>(), "Alice");
}

TEST(ValueAccessors, ObjectSubscriptMissingKeyReturnsInvalid) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  // Non-throwing: missing key returns invalid Value (operator bool == false)
  Value v = root["missing"];
  EXPECT_FALSE(static_cast<bool>(v));
}

TEST(ValueAccessors, ObjectSubscriptOnNonObjectReturnsInvalid) {
  Document doc;
  auto root = parse_root(doc, "[1, 2]");
  Value v = root["key"];
  EXPECT_FALSE(static_cast<bool>(v));
}

// ── operator[](idx): array access ─────────────────────────────────────────────

TEST(ValueAccessors, ArraySubscript) {
  Document doc;
  auto root = parse_root(doc, "[10, 20, 30]");
  EXPECT_EQ(root[0].as<int>(), 10);
  EXPECT_EQ(root[1].as<int>(), 20);
  EXPECT_EQ(root[2].as<int>(), 30);
}

TEST(ValueAccessors, ArraySubscriptOutOfRangeReturnsInvalid) {
  Document doc;
  auto root = parse_root(doc, "[1, 2]");
  Value v = root[5];
  EXPECT_FALSE(static_cast<bool>(v));
}

TEST(ValueAccessors, ArraySubscriptOnNonArrayReturnsInvalid) {
  Document doc;
  auto root = parse_root(doc, R"({"a": 1})");
  Value v = root[0];
  EXPECT_FALSE(static_cast<bool>(v));
}

// ── find(): safe object lookup ────────────────────────────────────────────────

TEST(ValueAccessors, FindPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"score": 100})");
  auto result = root.find("score");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->as<int>(), 100);
}

TEST(ValueAccessors, FindAbsent) {
  Document doc;
  auto root = parse_root(doc, R"({"a": 1})");
  EXPECT_FALSE(root.find("missing").has_value());
}

TEST(ValueAccessors, FindOnNonObject) {
  Document doc;
  auto root = parse_root(doc, "[1]");
  EXPECT_FALSE(root.find("key").has_value());
}

// ── size() and empty() ────────────────────────────────────────────────────────

TEST(ValueAccessors, SizeArray) {
  Document doc;
  EXPECT_EQ(parse_root(doc, "[]").size(), 0u);
  EXPECT_EQ(parse_root(doc, "[1]").size(), 1u);
  EXPECT_EQ(parse_root(doc, "[1,2,3]").size(), 3u);
}

TEST(ValueAccessors, SizeObject) {
  Document doc;
  EXPECT_EQ(parse_root(doc, "{}").size(), 0u);
  EXPECT_EQ(parse_root(doc, R"({"a":1})").size(), 1u);
  EXPECT_EQ(parse_root(doc, R"({"a":1,"b":2})").size(), 2u);
}

TEST(ValueAccessors, EmptyCheck) {
  Document doc;
  EXPECT_TRUE(parse_root(doc, "[]").empty());
  EXPECT_FALSE(parse_root(doc, "[1]").empty());
  EXPECT_TRUE(parse_root(doc, "{}").empty());
}

// ── Implicit conversion (operator T()) ────────────────────────────────────────

TEST(ValueAccessors, ImplicitInt) {
  Document doc;
  auto root = parse_root(doc, R"({"age": 25})");
  int age = root["age"]; // implicit conversion
  EXPECT_EQ(age, 25);
}

TEST(ValueAccessors, ImplicitString) {
  Document doc;
  auto root = parse_root(doc, R"({"name": "Beast"})");
  std::string name = root["name"]; // implicit conversion
  EXPECT_EQ(name, "Beast");
}

TEST(ValueAccessors, ImplicitDouble) {
  Document doc;
  auto root = parse_root(doc, R"({"ratio": 1.5})");
  double ratio = root["ratio"];
  EXPECT_NEAR(ratio, 1.5, 1e-9);
}

TEST(ValueAccessors, ImplicitBool) {
  Document doc;
  auto root = parse_root(doc, R"({"flag": true})");
  bool flag = root["flag"];
  EXPECT_TRUE(flag);
}

// ── Chained access ────────────────────────────────────────────────────────────

TEST(ValueAccessors, ChainedObjectAccess) {
  Document doc;
  auto root = parse_root(doc, R"({"user": {"id": 7, "name": "Eve"}})");
  EXPECT_EQ(root["user"]["id"].as<int>(), 7);
  EXPECT_EQ(root["user"]["name"].as<std::string>(), "Eve");
}

TEST(ValueAccessors, ChainedArrayAccess) {
  Document doc;
  auto root = parse_root(doc, R"({"ids": [10, 20, 30]})");
  EXPECT_EQ(root["ids"][0].as<int>(), 10);
  EXPECT_EQ(root["ids"][2].as<int>(), 30);
}

TEST(ValueAccessors, DeepNested) {
  Document doc;
  auto root =
      parse_root(doc, R"({"a": {"b": {"c": 42}}})");
  EXPECT_EQ(root["a"]["b"]["c"].as<int>(), 42);
}

// ── Mixed-type arrays ─────────────────────────────────────────────────────────

TEST(ValueAccessors, MixedArray) {
  Document doc;
  auto root = parse_root(doc, R"([1, "two", true, null])");
  EXPECT_EQ(root[0].as<int>(), 1);
  EXPECT_EQ(root[1].as<std::string>(), "two");
  EXPECT_TRUE(root[2].as<bool>());
  EXPECT_TRUE(root[3].is_null());
}

// ── Boolean validity check (operator bool()) ──────────────────────────────────

TEST(ValueAccessors, ValidityCheck) {
  Document doc;
  auto root = parse_root(doc, "42");
  EXPECT_TRUE(static_cast<bool>(root));
  Value empty;
  EXPECT_FALSE(static_cast<bool>(empty));
}

// ══════════════════════════════════════════════════════════════════════════════
// set<T>(): mutation tests
// ══════════════════════════════════════════════════════════════════════════════

// ── set() scalar types ────────────────────────────────────────────────────────

TEST(ValueMutation, SetInt) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  root["x"].set(42);
  EXPECT_EQ(root["x"].as<int>(), 42);
  EXPECT_TRUE(root["x"].is_int());
}

TEST(ValueMutation, SetNegativeInt) {
  Document doc;
  auto root = parse_root(doc, R"({"n": 0})");
  root["n"].set(-99);
  EXPECT_EQ(root["n"].as<int64_t>(), -99);
}

TEST(ValueMutation, SetDouble) {
  Document doc;
  auto root = parse_root(doc, R"({"pi": 0})");
  root["pi"].set(3.14);
  EXPECT_NEAR(root["pi"].as<double>(), 3.14, 1e-9);
  EXPECT_TRUE(root["pi"].is_double());
}

TEST(ValueMutation, SetBoolTrue) {
  Document doc;
  auto root = parse_root(doc, R"({"flag": false})");
  root["flag"].set(true);
  EXPECT_TRUE(root["flag"].as<bool>());
  EXPECT_TRUE(root["flag"].is_bool());
}

TEST(ValueMutation, SetBoolFalse) {
  Document doc;
  auto root = parse_root(doc, R"({"flag": true})");
  root["flag"].set(false);
  EXPECT_FALSE(root["flag"].as<bool>());
}

TEST(ValueMutation, SetNull) {
  Document doc;
  auto root = parse_root(doc, R"({"v": 42})");
  root["v"].set(nullptr);
  EXPECT_TRUE(root["v"].is_null());
}

TEST(ValueMutation, SetString) {
  Document doc;
  auto root = parse_root(doc, R"({"name": "old"})");
  root["name"].set("new");
  EXPECT_EQ(root["name"].as<std::string>(), "new");
  EXPECT_TRUE(root["name"].is_string());
}

TEST(ValueMutation, SetStringView) {
  Document doc;
  auto root = parse_root(doc, R"({"key": "before"})");
  std::string_view sv = "after";
  root["key"].set(sv);
  EXPECT_EQ(root["key"].as<std::string>(), "after");
}

TEST(ValueMutation, SetStdString) {
  Document doc;
  auto root = parse_root(doc, R"({"msg": "hi"})");
  std::string s = "hello world";
  root["msg"].set(s);
  EXPECT_EQ(root["msg"].as<std::string>(), "hello world");
}

// ── set() with type change ─────────────────────────────────────────────────

TEST(ValueMutation, SetTypeChange_IntToString) {
  Document doc;
  auto root = parse_root(doc, R"({"v": 42})");
  root["v"].set("text");
  EXPECT_TRUE(root["v"].is_string());
  EXPECT_FALSE(root["v"].is_int());
  EXPECT_EQ(root["v"].as<std::string>(), "text");
}

TEST(ValueMutation, SetTypeChange_StringToInt) {
  Document doc;
  auto root = parse_root(doc, R"({"v": "old"})");
  root["v"].set(7);
  EXPECT_TRUE(root["v"].is_int());
  EXPECT_EQ(root["v"].as<int>(), 7);
}

// ── set() reflected in dump() ──────────────────────────────────────────────

TEST(ValueMutation, DumpAfterSetInt) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  root["x"].set(99);
  EXPECT_EQ(root.dump(), R"({"x":99})");
}

TEST(ValueMutation, DumpAfterSetString) {
  Document doc;
  auto root = parse_root(doc, R"({"name": "Alice"})");
  root["name"].set("Bob");
  EXPECT_EQ(root.dump(), R"({"name":"Bob"})");
}

TEST(ValueMutation, DumpAfterSetNull) {
  Document doc;
  auto root = parse_root(doc, R"({"v": 1})");
  root["v"].set(nullptr);
  EXPECT_EQ(root.dump(), R"({"v":null})");
}

TEST(ValueMutation, DumpAfterSetBool) {
  Document doc;
  auto root = parse_root(doc, R"({"ok": false})");
  root["ok"].set(true);
  EXPECT_EQ(root.dump(), R"({"ok":true})");
}

TEST(ValueMutation, DumpAfterSetMultiple) {
  Document doc;
  auto root = parse_root(doc, R"({"a": 1, "b": 2, "c": 3})");
  root["a"].set(10);
  root["c"].set(30);
  std::string out = root.dump();
  EXPECT_EQ(out, R"({"a":10,"b":2,"c":30})");
}

TEST(ValueMutation, DumpStringBufferAfterSet) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  root["x"].set(42);
  std::string buf;
  root.dump(buf);
  EXPECT_EQ(buf, R"({"x":42})");
}

// ── unset(): restore original value ──────────────────────────────────────────

TEST(ValueMutation, Unset) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  root["x"].set(99);
  EXPECT_EQ(root["x"].as<int>(), 99);
  root["x"].unset();
  EXPECT_EQ(root["x"].as<int>(), 1); // back to original
}

// ── set() on array element ────────────────────────────────────────────────────

TEST(ValueMutation, SetArrayElement) {
  Document doc;
  auto root = parse_root(doc, "[10, 20, 30]");
  root[1].set(200);
  EXPECT_EQ(root[0].as<int>(), 10);
  EXPECT_EQ(root[1].as<int>(), 200);
  EXPECT_EQ(root[2].as<int>(), 30);
  EXPECT_EQ(root.dump(), "[10,200,30]");
}

// ── set() overwrites previous set() ──────────────────────────────────────────

TEST(ValueMutation, SetOverwrite) {
  Document doc;
  auto root = parse_root(doc, R"({"v": 0})");
  root["v"].set(1);
  root["v"].set(2);
  root["v"].set(3);
  EXPECT_EQ(root["v"].as<int>(), 3);
  EXPECT_EQ(root.dump(), R"({"v":3})");
}

// ── set() on nested value ─────────────────────────────────────────────────────

TEST(ValueMutation, SetNested) {
  Document doc;
  auto root = parse_root(doc, R"({"user": {"score": 0}})");
  root["user"]["score"].set(100);
  EXPECT_EQ(root["user"]["score"].as<int>(), 100);
  EXPECT_EQ(root.dump(), R"({"user":{"score":100}})");
}

// ── try_as<T>() on mutated value ──────────────────────────────────────────────

TEST(ValueMutation, TryAsAfterSet) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 0})");
  root["x"].set(55);
  auto r = root["x"].try_as<int>();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 55);
}

// ── larger mutation string than original ─────────────────────────────────────

TEST(ValueMutation, LargerMutationNoOverflow) {
  Document doc;
  auto root = parse_root(doc, R"({"s": "x"})");
  root["s"].set("a much longer string than the original");
  EXPECT_EQ(root["s"].as<std::string>(), "a much longer string than the original");
  std::string out = root.dump();
  EXPECT_EQ(out, R"({"s":"a much longer string than the original"})");
}

// ══════════════════════════════════════════════════════════════════════════════
// operator= write syntax: root["key"] = value
// ══════════════════════════════════════════════════════════════════════════════

TEST(ValueAssign, AssignInt) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 0})");
  root["x"] = 42;
  EXPECT_EQ(root["x"].as<int>(), 42);
  EXPECT_EQ(root.dump(), R"({"x":42})");
}

TEST(ValueAssign, AssignNegative) {
  Document doc;
  auto root = parse_root(doc, R"({"n": 0})");
  root["n"] = -7;
  EXPECT_EQ(root["n"].as<int>(), -7);
}

TEST(ValueAssign, AssignDouble) {
  Document doc;
  auto root = parse_root(doc, R"({"f": 0})");
  root["f"] = 1.5;
  EXPECT_NEAR(root["f"].as<double>(), 1.5, 1e-9);
}

TEST(ValueAssign, AssignBool) {
  Document doc;
  auto root = parse_root(doc, R"({"b": false})");
  root["b"] = true;
  EXPECT_TRUE(root["b"].as<bool>());
  EXPECT_EQ(root.dump(), R"({"b":true})");
}

TEST(ValueAssign, AssignNull) {
  Document doc;
  auto root = parse_root(doc, R"({"v": 99})");
  root["v"] = nullptr;
  EXPECT_TRUE(root["v"].is_null());
  EXPECT_EQ(root.dump(), R"({"v":null})");
}

TEST(ValueAssign, AssignStringLiteral) {
  Document doc;
  auto root = parse_root(doc, R"({"name": "old"})");
  root["name"] = "new";
  EXPECT_EQ(root["name"].as<std::string>(), "new");
  EXPECT_EQ(root.dump(), R"({"name":"new"})");
}

TEST(ValueAssign, AssignStdString) {
  Document doc;
  auto root = parse_root(doc, R"({"msg": ""})");
  std::string s = "hello";
  root["msg"] = s;
  EXPECT_EQ(root["msg"].as<std::string>(), "hello");
}

TEST(ValueAssign, AssignArrayElement) {
  Document doc;
  auto root = parse_root(doc, "[1, 2, 3]");
  root[1] = 99;
  EXPECT_EQ(root.dump(), "[1,99,3]");
}

TEST(ValueAssign, AssignNested) {
  Document doc;
  auto root = parse_root(doc, R"({"a": {"b": 0}})");
  root["a"]["b"] = 42;
  EXPECT_EQ(root.dump(), R"({"a":{"b":42}})");
}

TEST(ValueAssign, AssignChained) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1, "y": 2})");
  root["x"] = 10;
  root["y"] = 20;
  EXPECT_EQ(root.dump(), R"({"x":10,"y":20})");
}

// ══════════════════════════════════════════════════════════════════════════════
// SafeValue: get() optional chain — never throws
// ══════════════════════════════════════════════════════════════════════════════

using namespace qbuem::json; // access SafeValue type

TEST(SafeValue, GetPresentKey) {
  Document doc;
  auto root = parse_root(doc, R"({"id": 7})");
  auto v = root.get("id");
  EXPECT_TRUE(v.has_value());
  EXPECT_EQ(v.as<int>(), std::optional<int>(7));
}

TEST(SafeValue, GetMissingKey) {
  Document doc;
  auto root = parse_root(doc, R"({"id": 7})");
  auto v = root.get("missing");
  EXPECT_FALSE(v.has_value());
  EXPECT_FALSE(v.as<int>().has_value());
}

TEST(SafeValue, GetChainBothPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"user": {"id": 42}})");
  auto id = root.get("user")["id"].as<int>();
  ASSERT_TRUE(id.has_value());
  EXPECT_EQ(*id, 42);
}

TEST(SafeValue, GetChainFirstMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"user": {"id": 42}})");
  auto id = root.get("nope")["id"].as<int>(); // "nope" doesn't exist
  EXPECT_FALSE(id.has_value()); // chain short-circuits, no throw
}

TEST(SafeValue, GetChainSecondMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"user": {"id": 42}})");
  auto v = root.get("user")["nope"].as<int>(); // "nope" not in user
  EXPECT_FALSE(v.has_value());
}

TEST(SafeValue, GetDeepChain) {
  Document doc;
  auto root = parse_root(doc, R"({"a": {"b": {"c": 99}}})");
  auto v = root.get("a")["b"]["c"].as<int>();
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 99);
}

TEST(SafeValue, GetDeepChainMidMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"a": {"b": {"c": 99}}})");
  auto v = root.get("a")["x"]["c"].as<int>(); // "x" missing
  EXPECT_FALSE(v.has_value());
}

TEST(SafeValue, ValueOrPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"score": 88})");
  int s = root.get("score").value_or(0);
  EXPECT_EQ(s, 88);
}

TEST(SafeValue, ValueOrMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"score": 88})");
  int s = root.get("missing").value_or(-1);
  EXPECT_EQ(s, -1);
}

TEST(SafeValue, ValueOrNestedMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"user": {}})");
  int id = root.get("user")["id"].value_or(0);
  EXPECT_EQ(id, 0); // "id" missing → 0
}

TEST(SafeValue, BoolConversion) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  EXPECT_TRUE(static_cast<bool>(root.get("x")));
  EXPECT_FALSE(static_cast<bool>(root.get("y")));
}

TEST(SafeValue, StarOperator) {
  Document doc;
  auto root = parse_root(doc, R"({"v": 5})");
  auto sv = root.get("v");
  EXPECT_EQ((*sv).as<int>(), 5);
}

TEST(SafeValue, ArrowOperator) {
  Document doc;
  auto root = parse_root(doc, R"({"v": 5})");
  auto sv = root.get("v");
  EXPECT_EQ(sv->as<int>(), 5);
}

TEST(SafeValue, TypeChecks) {
  Document doc;
  auto root = parse_root(doc, R"({"n": 1, "s": "x", "b": true})");
  EXPECT_TRUE(root.get("n").is_int());
  EXPECT_TRUE(root.get("s").is_string());
  EXPECT_TRUE(root.get("b").is_bool());
  EXPECT_FALSE(root.get("missing").is_int());
}

TEST(SafeValue, GetArrayIndex) {
  Document doc;
  auto root = parse_root(doc, R"({"ids": [10, 20, 30]})");
  auto v = root.get("ids")[1].as<int>();
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 20);
}

TEST(SafeValue, GetArrayOutOfRange) {
  Document doc;
  auto root = parse_root(doc, "[1, 2]");
  EXPECT_FALSE(root.get(5).has_value()); // index 5 doesn't exist
}

TEST(SafeValue, GetOnNonObject) {
  Document doc;
  auto root = parse_root(doc, "[1, 2]");
  EXPECT_FALSE(root.get("key").has_value()); // array, not object
}

TEST(SafeValue, DumpPresent) {
  // dump() is now subtree-aware: returns only the selected value
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  EXPECT_EQ(root.get("x").dump(), "1");
}

TEST(SafeValue, DumpAbsent) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  EXPECT_EQ(root.get("missing").dump(), "null");
}

// ── size() and empty() on SafeValue ───────────────────────────────────────────
//
// sv->size() throws bad_optional_access when absent.
// sv.size()  returns 0 safely — the correct pattern.

TEST(SafeValue, SizeArray) {
  Document doc;
  auto root = parse_root(doc, R"({"tags": ["a", "b", "c"]})");
  auto tags = root.get("tags");
  ASSERT_TRUE(tags.has_value());
  EXPECT_EQ(tags.size(), 3u);
}

TEST(SafeValue, SizeObject) {
  Document doc;
  auto root = parse_root(doc, R"({"meta": {"k1": 1, "k2": 2}})");
  EXPECT_EQ(root.get("meta").size(), 2u);
}

TEST(SafeValue, SizeEmptyArray) {
  Document doc;
  auto root = parse_root(doc, R"({"items": []})");
  EXPECT_EQ(root.get("items").size(), 0u);
  EXPECT_TRUE(root.get("items").empty());
}

TEST(SafeValue, SizeAbsent) {
  // absent SafeValue: size() must return 0, not throw
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  auto sv = root.get("missing");
  EXPECT_FALSE(sv.has_value());
  EXPECT_EQ(sv.size(), 0u);   // safe — no exception
  EXPECT_TRUE(sv.empty());    // safe — no exception
}

TEST(SafeValue, SizeNonContainer) {
  // scalar value: size() returns 0 (same as Value::size())
  Document doc;
  auto root = parse_root(doc, R"({"n": 42})");
  EXPECT_EQ(root.get("n").size(), 0u);
}

TEST(SafeValue, IndexVsArrow) {
  // sv[0] and sv.get(0) propagate absence safely.
  // sv->get(0) throws if absent — caller must guard.
  Document doc;
  auto root = parse_root(doc, R"({"tags": ["a", "b"]})");
  auto tags = root.get("tags");

  // safe patterns
  EXPECT_EQ(tags[0].value_or(std::string{""}), "a");
  EXPECT_EQ(tags.get(1).value_or(std::string{""}), "b");

  // absent chain: no throw
  auto none = root.get("missing");
  EXPECT_FALSE(none[0].has_value());
  EXPECT_FALSE(none.get(0).has_value());
}

// ── Monadic operations ────────────────────────────────────────────────────────

TEST(Monadic, AndThenPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"price": 100})");
  // and_then: Value is present, transform succeeds → still SafeValue
  auto result = root.get("price")
      .and_then([](const Value& v) -> SafeValue {
          return v.is_number() ? SafeValue{v} : SafeValue{};
      });
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.as<int>(), std::optional<int>(100));
}

TEST(Monadic, AndThenAbsent) {
  Document doc;
  auto root = parse_root(doc, R"({"price": 100})");
  // and_then: Value is absent, F is never called → SafeValue{}
  bool called = false;
  auto result = root.get("missing")
      .and_then([&](const Value&) -> SafeValue {
          called = true;
          return {};
      });
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(called);
}

TEST(Monadic, AndThenValidationFail) {
  Document doc;
  auto root = parse_root(doc, R"({"tag": "hello"})");
  // and_then: Value is present but validation rejects it → SafeValue{}
  auto result = root.get("tag")
      .and_then([](const Value& v) -> SafeValue {
          return v.is_number() ? SafeValue{v} : SafeValue{};
      });
  EXPECT_FALSE(result.has_value());
}

TEST(Monadic, TransformPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"count": 7})");
  // transform: map Value -> double
  auto result = root.get("count")
      .transform([](const Value& v) { return v.as<int>() * 2; });
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 14);
}

TEST(Monadic, TransformAbsent) {
  Document doc;
  auto root = parse_root(doc, R"({"count": 7})");
  auto result = root.get("missing")
      .transform([](const Value& v) { return v.as<int>(); });
  EXPECT_FALSE(result.has_value());
}

TEST(Monadic, TransformStringExtract) {
  Document doc;
  auto root = parse_root(doc, R"({"name": "beast"})");
  auto result = root.get("name")
      .transform([](const Value& v) { return std::string(v.as<std::string_view>()); });
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beast");
}

TEST(Monadic, OrElsePresent) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 42})");
  bool called = false;
  auto result = root.get("x")
      .or_else([&]() -> SafeValue {
          called = true;
          return {};
      });
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(called);
}

TEST(Monadic, OrElseAbsent) {
  Document doc1, doc2;
  auto root1 = parse_root(doc1, R"({"x": 42})");
  auto root2 = parse_root(doc2, R"({"default": 99})");
  auto result = root1.get("missing")
      .or_else([&]() -> SafeValue { return root2.get("default"); });
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.as<int>(), std::optional<int>(99));
}

TEST(Monadic, PipelineChain) {
  Document doc;
  // Full pipeline: navigate → validate → transform → fallback
  auto root = parse_root(doc, R"({"store": {"item": {"price": 200}}})");
  double price = root.get("store")["item"]["price"]
      .and_then([](const Value& v) -> SafeValue {
          return v.is_number() ? SafeValue{v} : SafeValue{};
      })
      .transform([](const Value& v) { return v.as<double>() * 1.1; })
      .value_or(0.0);
  EXPECT_NEAR(price, 220.0, 1e-9);
}

TEST(Monadic, PipelineChainMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"store": {}})");
  double price = root.get("store")["item"]["price"]
      .and_then([](const Value& v) -> SafeValue {
          return v.is_number() ? SafeValue{v} : SafeValue{};
      })
      .transform([](const Value& v) { return v.as<double>() * 1.1; })
      .value_or(-1.0);
  EXPECT_DOUBLE_EQ(price, -1.0);
}

// ── Concept constraints (compile-time) ────────────────────────────────────────

TEST(Concepts, JsonIntegerConcept) {
  // JsonInteger accepts int, long, short but not bool
  static_assert(qbuem::json::JsonInteger<int>);
  static_assert(qbuem::json::JsonInteger<long>);
  static_assert(qbuem::json::JsonInteger<int64_t>);
  static_assert(!qbuem::json::JsonInteger<bool>);
  static_assert(!qbuem::json::JsonInteger<float>);
  SUCCEED();
}

TEST(Concepts, JsonFloatConcept) {
  static_assert(qbuem::json::JsonFloat<float>);
  static_assert(qbuem::json::JsonFloat<double>);
  static_assert(!qbuem::json::JsonFloat<int>);
  static_assert(!qbuem::json::JsonFloat<bool>);
  SUCCEED();
}

TEST(Concepts, JsonReadableConcept) {
  static_assert(qbuem::json::JsonReadable<bool>);
  static_assert(qbuem::json::JsonReadable<int>);
  static_assert(qbuem::json::JsonReadable<double>);
  static_assert(qbuem::json::JsonReadable<std::string>);
  static_assert(qbuem::json::JsonReadable<std::string_view>);
  SUCCEED();
}

TEST(Concepts, JsonWritableConcept) {
  static_assert(qbuem::json::JsonWritable<bool>);
  static_assert(qbuem::json::JsonWritable<int>);
  static_assert(qbuem::json::JsonWritable<double>);
  static_assert(qbuem::json::JsonWritable<std::string_view>);
  static_assert(qbuem::json::JsonWritable<std::nullptr_t>);
  SUCCEED();
}

// ── Structural mutation ───────────────────────────────────────────────────────

TEST(StructuralMutation, EraseKey) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2,"c":3})");
  root.erase("b");
  EXPECT_EQ(root.dump(), R"({"a":1,"c":3})");
}

TEST(StructuralMutation, EraseCascade) {
  Document doc;
  auto root = parse_root(doc, R"({"user":{"name":"kim","age":30},"tag":"x"})");
  root.erase("user");  // cascade: entire subtree removed
  EXPECT_EQ(root.dump(), R"({"tag":"x"})");
}

TEST(StructuralMutation, EraseAllKeys) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2})");
  root.erase("a");
  root.erase("b");
  EXPECT_EQ(root.dump(), "{}");
}

TEST(StructuralMutation, EraseArrayElement) {
  Document doc;
  auto root = parse_root(doc, "[10,20,30]");
  root.erase(static_cast<size_t>(1));
  EXPECT_EQ(root.dump(), "[10,30]");
}

TEST(StructuralMutation, EraseFirstArrayElement) {
  Document doc;
  auto root = parse_root(doc, "[1,2,3]");
  root.erase(static_cast<size_t>(0));
  EXPECT_EQ(root.dump(), "[2,3]");
}

TEST(StructuralMutation, InsertKey) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1})");
  root.insert("b", 2);
  std::string s = root.dump();
  EXPECT_NE(s.find("\"b\":2"), std::string::npos);
  EXPECT_NE(s.find("\"a\":1"), std::string::npos);
}

TEST(StructuralMutation, InsertStringVal) {
  Document doc;
  auto root = parse_root(doc, R"({"x":1})");
  root.insert("name", std::string_view("beast"));
  std::string s = root.dump();
  EXPECT_NE(s.find("\"name\":\"beast\""), std::string::npos);
}

TEST(StructuralMutation, InsertNestedObject) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1})");
  root.insert_json("nested", R"({"k":99})");
  std::string s = root.dump();
  EXPECT_NE(s.find("\"nested\":{\"k\":99}"), std::string::npos);
}

TEST(StructuralMutation, PushBack) {
  Document doc;
  auto root = parse_root(doc, "[1,2]");
  root.push_back(3);
  EXPECT_EQ(root.dump(), "[1,2,3]");
}

TEST(StructuralMutation, PushBackString) {
  Document doc;
  auto root = parse_root(doc, R"(["a"])");
  root.push_back(std::string_view("b"));
  EXPECT_EQ(root.dump(), R"(["a","b"])");
}

TEST(StructuralMutation, SizeAfterErase) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2,"c":3})");
  EXPECT_EQ(root.size(), 3u);
  root.erase("b");
  EXPECT_EQ(root.size(), 2u);
}

TEST(StructuralMutation, SizeAfterInsert) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1})");
  root.insert("b", 2);
  EXPECT_EQ(root.size(), 2u);
}

TEST(StructuralMutation, SubscriptAfterErase) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2})");
  root.erase("a");
  EXPECT_FALSE(static_cast<bool>(root["a"]));
  EXPECT_EQ(root["b"].as<int>(), 2);
}

TEST(StructuralMutation, EraseAndMutationCombo) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2})");
  root.erase("a");
  root["b"] = 99;
  EXPECT_EQ(root.dump(), R"({"b":99})");
}

// ── Iteration ─────────────────────────────────────────────────────────────────

TEST(Iteration, ItemsBasic) {
  Document doc;
  auto root = parse_root(doc, R"({"x":1,"y":2})");
  std::map<std::string, int> got;
  for (auto [k, v] : root.items())
    got[std::string(k)] = v.as<int>();
  EXPECT_EQ(got["x"], 1);
  EXPECT_EQ(got["y"], 2);
  EXPECT_EQ(got.size(), 2u);
}

TEST(Iteration, ItemsSkipsDeleted) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2,"c":3})");
  root.erase("b");
  std::vector<std::string> keys;
  for (auto [k, v] : root.items())
    keys.push_back(std::string(k));
  ASSERT_EQ(keys.size(), 2u);
  EXPECT_EQ(keys[0], "a");
  EXPECT_EQ(keys[1], "c");
}

TEST(Iteration, ItemsEmpty) {
  Document doc;
  auto root = parse_root(doc, "{}");
  int count = 0;
  for (auto [k, v] : root.items()) { (void)k; (void)v; ++count; }
  EXPECT_EQ(count, 0);
}

TEST(Iteration, ElementsBasic) {
  Document doc;
  auto root = parse_root(doc, "[10,20,30]");
  std::vector<int> got;
  for (auto v : root.elements())
    got.push_back(v.as<int>());
  ASSERT_EQ(got.size(), 3u);
  EXPECT_EQ(got[0], 10);
  EXPECT_EQ(got[1], 20);
  EXPECT_EQ(got[2], 30);
}

TEST(Iteration, ElementsSkipsDeleted) {
  Document doc;
  auto root = parse_root(doc, "[1,2,3,4]");
  root.erase(static_cast<size_t>(1));
  std::vector<int> got;
  for (auto v : root.elements())
    got.push_back(v.as<int>());
  ASSERT_EQ(got.size(), 3u);
  EXPECT_EQ(got[0], 1);
  EXPECT_EQ(got[1], 3);
  EXPECT_EQ(got[2], 4);
}

TEST(Iteration, ElementsNestedObjects) {
  Document doc;
  auto root = parse_root(doc, R"([{"id":1},{"id":2}])");
  std::vector<int> ids;
  for (auto v : root.elements())
    ids.push_back(v["id"].as<int>());
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], 1);
  EXPECT_EQ(ids[1], 2);
}

// ── Subtree dump ──────────────────────────────────────────────────────────────

TEST(SubtreeDump, ObjectSubvalue) {
  Document doc;
  auto root = parse_root(doc, R"({"user":{"name":"kim"},"tag":"x"})");
  EXPECT_EQ(root["user"].dump(), R"({"name":"kim"})");
}

TEST(SubtreeDump, ArraySubvalue) {
  Document doc;
  auto root = parse_root(doc, R"({"arr":[1,2,3]})");
  EXPECT_EQ(root["arr"].dump(), "[1,2,3]");
}

TEST(SubtreeDump, ScalarSubvalue) {
  Document doc;
  auto root = parse_root(doc, R"({"n":42})");
  EXPECT_EQ(root["n"].dump(), "42");
}

TEST(SubtreeDump, ChainedSubvalue) {
  Document doc;
  auto root = parse_root(doc, R"({"a":{"b":{"c":99}}})");
  EXPECT_EQ(root["a"]["b"]["c"].dump(), "99");
}

// ── Pretty-print ──────────────────────────────────────────────────────────────

TEST(PrettyPrint, SimpleObject) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2})");
  std::string p = root.dump(2);
  EXPECT_NE(p.find("  \"a\": 1"), std::string::npos);
  EXPECT_NE(p.find("  \"b\": 2"), std::string::npos);
  EXPECT_EQ(p.front(), '{');
  EXPECT_EQ(p.back(), '}');
}

TEST(PrettyPrint, NestedObject) {
  Document doc;
  auto root = parse_root(doc, R"({"x":{"y":1}})");
  std::string p = root.dump(4);
  EXPECT_NE(p.find("    \"y\": 1"), std::string::npos);
}

TEST(PrettyPrint, SimpleArray) {
  Document doc;
  auto root = parse_root(doc, "[1,2,3]");
  std::string p = root.dump(2);
  EXPECT_NE(p.find("  1"), std::string::npos);
  EXPECT_EQ(p.front(), '[');
  EXPECT_EQ(p.back(), ']');
}

TEST(PrettyPrint, EmptyObject) {
  Document doc;
  auto root = parse_root(doc, "{}");
  EXPECT_EQ(root.dump(2), "{}");
}

TEST(PrettyPrint, EmptyArray) {
  Document doc;
  auto root = parse_root(doc, "[]");
  EXPECT_EQ(root.dump(2), "[]");
}

// ── Auto-chain (non-throwing operator[]) ─────────────────────────────────────

TEST(AutoChain, SafeChainNoThrow) {
  Document doc;
  auto root = parse_root(doc, R"({"a":{"b":{"c":42}}})");
  // Deep chain, all present
  Value v = root["a"]["b"]["c"];
  ASSERT_TRUE(static_cast<bool>(v));
  EXPECT_EQ(v.as<int>(), 42);
}

TEST(AutoChain, SafeChainMissingMid) {
  Document doc;
  auto root = parse_root(doc, R"({"a":{"b":1}})");
  // "x" is missing — should return invalid Value without throwing
  Value v = root["a"]["x"]["c"];
  EXPECT_FALSE(static_cast<bool>(v));
}

TEST(AutoChain, SafeChainMissingRoot) {
  Document doc;
  auto root = parse_root(doc, R"({"z":1})");
  Value v = root["a"]["b"]["c"];
  EXPECT_FALSE(static_cast<bool>(v));
}

TEST(AutoChain, TypeMismatch) {
  Document doc;
  auto root = parse_root(doc, R"({"a":42})");
  // "a" is int, not object — further chaining returns invalid
  Value v = root["a"]["sub"];
  EXPECT_FALSE(static_cast<bool>(v));
}

// ── C++20 ranges / STL algorithm compatibility ────────────────────────────────

TEST(Ranges, FindIfObject) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":99,"c":3})");
  auto it = std::ranges::find_if(root.items(),
      [](auto kv){ return kv.first == "b"; });
  ASSERT_NE(it, root.items().end());
  EXPECT_EQ((*it).second.as<int>(), 99);
}

TEST(Ranges, CountIfArray) {
  Document doc;
  auto root = parse_root(doc, "[1,5,2,8,3,9]");
  long cnt = std::ranges::count_if(root.elements(),
      [](Value v){ return v.as<int>() > 4; });
  EXPECT_EQ(cnt, 3);
}

TEST(Ranges, MaxElement) {
  Document doc;
  auto root = parse_root(doc, "[3,1,4,1,5,9,2,6]");
  auto mx = std::ranges::max_element(root.elements(),
      [](Value a, Value b){ return a.as<int>() < b.as<int>(); });
  ASSERT_NE(mx, root.elements().end());
  EXPECT_EQ((*mx).as<int>(), 9);
}

TEST(Ranges, Accumulate) {
  Document doc;
  auto root = parse_root(doc, "[1,2,3,4,5]");
  int sum = std::accumulate(root.elements().begin(), root.elements().end(), 0,
      [](int acc, Value v){ return acc + v.as<int>(); });
  EXPECT_EQ(sum, 15);
}

TEST(Ranges, TransformToVector) {
  Document doc;
  auto root = parse_root(doc, "[10,20,30]");
  std::vector<int> out;
  std::ranges::transform(root.elements(), std::back_inserter(out),
      [](Value v){ return v.as<int>(); });
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[1], 20);
}

TEST(Ranges, FilterPipeOnArray) {
  Document doc;
  auto root = parse_root(doc, "[1,2,3,4,5,6]");
  int cnt = 0;
  for (auto v : root.elements()) {
    if (v.as<int>() % 2 == 0) ++cnt;
  }
  EXPECT_EQ(cnt, 3);
}

TEST(Ranges, FilterPipeOnObject) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":20,"c":3,"d":40})");
  int cnt = 0;
  for (auto [k, v] : root.items()) {
    if (v.as<int>() > 10) { (void)k; ++cnt; }
  }
  EXPECT_EQ(cnt, 2);
}

TEST(Ranges, Distance) {
  Document doc;
  auto root = parse_root(doc, "[10,20,30,40,50]");
  EXPECT_EQ(std::ranges::distance(root.elements()), 5);
}

TEST(Ranges, BorrowedRangeRvalue) {
  // borrowed_range: std::ranges::find_if on rvalue range returns real iterator
  Document doc;
  auto root = parse_root(doc, "[7,8,9]");
  // This would return std::ranges::dangling without enable_borrowed_range
  auto it = std::ranges::find_if(root.elements(),
      [](Value v){ return v.as<int>() == 8; });
  ASSERT_NE(it, root.elements().end());
  EXPECT_EQ((*it).as<int>(), 8);
}

TEST(Ranges, ForEach) {
  Document doc;
  auto root = parse_root(doc, R"({"x":1,"y":2,"z":3})");
  int sum = 0;
  using ObjPair = std::pair<std::string_view, Value>;
  std::for_each(root.items().begin(), root.items().end(),
      [&](ObjPair kv){ sum += kv.second.as<int>(); });
  EXPECT_EQ(sum, 6);
}

// ── contains() ───────────────────────────────────────────────────────────────

TEST(Contains, PresentKey) {
  Document doc;
  auto root = parse_root(doc, R"({"name":"Alice","age":30})");
  EXPECT_TRUE(root.contains("name"));
  EXPECT_TRUE(root.contains("age"));
}

TEST(Contains, AbsentKey) {
  Document doc;
  auto root = parse_root(doc, R"({"name":"Alice"})");
  EXPECT_FALSE(root.contains("score"));
  EXPECT_FALSE(root.contains(""));
}

TEST(Contains, AfterErase) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2})");
  root.erase("a");
  EXPECT_FALSE(root.contains("a"));
  EXPECT_TRUE(root.contains("b"));
}

TEST(Contains, NonObject) {
  Document doc;
  auto root = parse_root(doc, "[1,2,3]");
  EXPECT_FALSE(root.contains("any"));
}

// ── value(key/idx, default) ───────────────────────────────────────────────────

TEST(ValueDefault, StringKey) {
  Document doc;
  auto root = parse_root(doc, R"({"name":"Alice","age":30})");
  EXPECT_EQ(root.value("name", std::string("unknown")), "Alice");
  EXPECT_EQ(root.value("missing", std::string("unknown")), "unknown");
}

TEST(ValueDefault, IntKey) {
  Document doc;
  auto root = parse_root(doc, R"({"age":30})");
  EXPECT_EQ(root.value("age", 0), 30);
  EXPECT_EQ(root.value("missing", 99), 99);
}

TEST(ValueDefault, StringLiteralFallback) {
  Document doc;
  auto root = parse_root(doc, R"({"name":"Eve"})");
  EXPECT_EQ(root.value("name", "anon"), std::string("Eve"));
  EXPECT_EQ(root.value("missing", "anon"), std::string("anon"));
}

TEST(ValueDefault, ArrayIndex) {
  Document doc;
  auto root = parse_root(doc, "[10,20,30]");
  EXPECT_EQ(root.value(0, -1), 10);
  EXPECT_EQ(root.value(1, -1), 20);
  EXPECT_EQ(root.value(9, -1), -1);
}

TEST(ValueDefault, WrongTypeFallback) {
  Document doc;
  auto root = parse_root(doc, R"({"x":"hello"})");
  EXPECT_EQ(root.value("x", 42), 42);  // string, not int → fallback
}

// ── type_name() ───────────────────────────────────────────────────────────────

TEST(TypeName, Scalars) {
  Document doc;
  auto root = parse_root(doc, R"({"n":null,"b":true,"i":42,"d":3.14,"s":"hi"})");
  EXPECT_EQ(root["n"].type_name(), "null");
  EXPECT_EQ(root["b"].type_name(), "bool");
  EXPECT_EQ(root["i"].type_name(), "int");
  EXPECT_EQ(root["d"].type_name(), "double");
  EXPECT_EQ(root["s"].type_name(), "string");
}

TEST(TypeName, Containers) {
  Document doc;
  auto root = parse_root(doc, R"({"arr":[1,2],"obj":{"k":1}})");
  EXPECT_EQ(root["arr"].type_name(), "array");
  EXPECT_EQ(root["obj"].type_name(), "object");
}

TEST(TypeName, Invalid) {
  Value v;
  EXPECT_EQ(v.type_name(), "invalid");
  Document doc;
  auto root = parse_root(doc, R"({"a":1})");
  EXPECT_EQ(root["missing"].type_name(), "invalid");
}

// ── operator| pipe fallback ───────────────────────────────────────────────────

TEST(PipeFallback, IntPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"age":42})");
  int age = root["age"] | 0;
  EXPECT_EQ(age, 42);
}

TEST(PipeFallback, IntMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"x":1})");
  int age = root["age"] | 99;
  EXPECT_EQ(age, 99);
}

TEST(PipeFallback, StringPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"name":"Eve"})");
  std::string name = root["name"] | "anon";
  EXPECT_EQ(name, "Eve");
}

TEST(PipeFallback, StringMissing) {
  Document doc;
  auto root = parse_root(doc, R"({"x":1})");
  std::string name = root["name"] | "anon";
  EXPECT_EQ(name, "anon");
}

TEST(PipeFallback, DoublePresent) {
  Document doc;
  auto root = parse_root(doc, R"({"pi":3.14})");
  double x = root["pi"] | 0.0;
  EXPECT_DOUBLE_EQ(x, 3.14);
}

TEST(PipeFallback, BoolPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"ok":true})");
  bool b = root["ok"] | false;
  EXPECT_TRUE(b);
}

TEST(PipeFallback, WrongType) {
  Document doc;
  auto root = parse_root(doc, R"({"x":"hello"})");
  int x = root["x"] | 99;
  EXPECT_EQ(x, 99);  // string, not int → fallback
}

TEST(PipeFallback, DeepChain) {
  Document doc;
  auto root = parse_root(doc, R"({"a":{"b":7}})");
  int v = root["a"]["b"] | 0;
  EXPECT_EQ(v, 7);
  int missing = root["a"]["z"] | 42;
  EXPECT_EQ(missing, 42);
}

TEST(PipeFallback, SafeValuePipe) {
  Document doc;
  auto root = parse_root(doc, R"({"user":{"age":25}})");
  int age = root.get("user")["age"] | 0;
  EXPECT_EQ(age, 25);
  int missing = root.get("missing")["age"] | 99;
  EXPECT_EQ(missing, 99);
}

// ── keys() / values() ─────────────────────────────────────────────────────────

TEST(KeysValues, KeysRange) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2,"c":3})");
  std::vector<std::string> keys;
  for (std::string_view k : root.keys())
    keys.emplace_back(k);
  std::sort(keys.begin(), keys.end());
  ASSERT_EQ(keys.size(), 3u);
  EXPECT_EQ(keys[0], "a");
  EXPECT_EQ(keys[1], "b");
  EXPECT_EQ(keys[2], "c");
}

TEST(KeysValues, ValuesRange) {
  Document doc;
  auto root = parse_root(doc, R"({"x":10,"y":20,"z":30})");
  int sum = 0;
  for (Value v : root.values())
    sum += v.as<int>();
  EXPECT_EQ(sum, 60);
}

TEST(KeysValues, EmptyObject) {
  Document doc;
  auto root = parse_root(doc, "{}");
  int cnt = 0;
  for (auto k : root.keys()) { (void)k; ++cnt; }
  EXPECT_EQ(cnt, 0);
}

// ── as_array<T>() / try_as_array<T>() ────────────────────────────────────────

TEST(AsArray, IntArray) {
  Document doc;
  auto root = parse_root(doc, "[1,2,3,4,5]");
  std::vector<int> v(root.as_array<int>().begin(), root.as_array<int>().end());
  ASSERT_EQ(v.size(), 5u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[4], 5);
}

TEST(AsArray, StringArray) {
  Document doc;
  auto root = parse_root(doc, R"(["a","b","c"])");
  std::vector<std::string> v;
  for (std::string s : root.as_array<std::string>())
    v.push_back(s);
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], "a");
}

TEST(AsArray, SumViaAccumulate) {
  Document doc;
  auto root = parse_root(doc, "[10,20,30]");
  auto arr = root.as_array<int>();
  int sum = std::accumulate(arr.begin(), arr.end(), 0);
  EXPECT_EQ(sum, 60);
}

TEST(AsArray, TryAsArrayOptional) {
  Document doc;
  auto root = parse_root(doc, "[1,\"oops\",3]");
  std::vector<std::optional<int>> v;
  for (auto x : root.try_as_array<int>())
    v.push_back(x);
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(*v[0], 1);
  EXPECT_FALSE(v[1].has_value());
  EXPECT_EQ(*v[2], 3);
}

TEST(AsArray, TypeMismatchThrows) {
  Document doc;
  auto root = parse_root(doc, "[1,\"oops\"]");
  EXPECT_THROW({
    for (int x : root.as_array<int>()) { (void)x; }
  }, std::runtime_error);
}

// ── at(path) — Runtime JSON Pointer ──────────────────────────────────────────

TEST(JsonPointer, RootPath) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1})");
  auto v = root.at("");
  EXPECT_TRUE(static_cast<bool>(v));
  EXPECT_EQ(v["a"].as<int>(), 1);
}

TEST(JsonPointer, OneLevel) {
  Document doc;
  auto root = parse_root(doc, R"({"name":"Alice","age":30})");
  EXPECT_EQ(root.at("/name").as<std::string>(), "Alice");
  EXPECT_EQ(root.at("/age").as<int>(), 30);
}

TEST(JsonPointer, Nested) {
  Document doc;
  auto root = parse_root(doc, R"({"user":{"id":7,"score":3.14}})");
  EXPECT_EQ(root.at("/user/id").as<int>(), 7);
  EXPECT_DOUBLE_EQ(root.at("/user/score").as<double>(), 3.14);
}

TEST(JsonPointer, ArrayIndex) {
  Document doc;
  auto root = parse_root(doc, R"({"tags":["go","rust","cpp"]})");
  EXPECT_EQ(root.at("/tags/0").as<std::string>(), "go");
  EXPECT_EQ(root.at("/tags/2").as<std::string>(), "cpp");
}

TEST(JsonPointer, DeepNested) {
  Document doc;
  auto root = parse_root(doc, R"({"a":{"b":{"c":42}}})");
  EXPECT_EQ(root.at("/a/b/c").as<int>(), 42);
}

TEST(JsonPointer, MissingReturnsInvalid) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1})");
  EXPECT_FALSE(static_cast<bool>(root.at("/missing")));
  EXPECT_FALSE(static_cast<bool>(root.at("/a/b")));
}

TEST(JsonPointer, EscapeSequences) {
  Document doc;
  // RFC 6901: ~0 → '~', ~1 → '/'
  auto root = parse_root(doc, R"({"a~b":1,"c/d":2})");
  EXPECT_EQ(root.at("/a~0b").as<int>(), 1);
  EXPECT_EQ(root.at("/c~1d").as<int>(), 2);
}

// ── at<Path>() — Compile-time JSON Pointer ────────────────────────────────────

TEST(JsonPointerCT, OneLevel) {
  Document doc;
  auto root = parse_root(doc, R"({"x":99})");
  EXPECT_EQ(root.at<"/x">().as<int>(), 99);
}

TEST(JsonPointerCT, Nested) {
  Document doc;
  auto root = parse_root(doc, R"({"a":{"b":42}})");
  EXPECT_EQ(root.at<"/a/b">().as<int>(), 42);
}

TEST(JsonPointerCT, Root) {
  Document doc;
  auto root = parse_root(doc, R"({"k":1})");
  auto v = root.at<"">();
  EXPECT_TRUE(static_cast<bool>(v));
}

// ── merge() / merge_patch() ───────────────────────────────────────────────────

// Helper: dump() → store → re-parse (Document stores string_view, not string)
static std::pair<std::string, qbuem::Value> reparse(qbuem::Document &r, qbuem::Value src) {
  std::string json = src.dump();
  auto val = parse(r, json);
  return {std::move(json), val};
}

TEST(Merge, BasicMerge) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2})");
  Document other_doc;
  auto other = parse_root(other_doc, R"({"b":99,"c":3})");
  root.merge(other);
  // merge() results visible via dump(); store string to outlive Document
  Document r; auto [json, res] = reparse(r, root);
  EXPECT_EQ(res["a"].as<int>(), 1);
  EXPECT_EQ(res["b"].as<int>(), 99);  // overwritten
  EXPECT_EQ(res["c"].as<int>(), 3);   // new key
}

TEST(Merge, MergeAddsNewKeys) {
  Document doc;
  auto root = parse_root(doc, R"({"x":1})");
  Document other_doc;
  auto other = parse_root(other_doc, R"({"y":2,"z":3})");
  root.merge(other);
  EXPECT_EQ(root["x"].as<int>(), 1);  // unchanged tape key — visible directly
  Document r; auto [json, res] = reparse(r, root);
  EXPECT_EQ(res["y"].as<int>(), 2);
  EXPECT_EQ(res["z"].as<int>(), 3);
}

TEST(MergePatch, DeleteKey) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1,"b":2,"c":3})");
  root.merge_patch(R"({"b":null})");
  EXPECT_EQ(root["a"].as<int>(), 1);
  EXPECT_FALSE(static_cast<bool>(root["b"]));  // deleted — invisible via operator[]
  EXPECT_EQ(root["c"].as<int>(), 3);
}

TEST(MergePatch, OverwriteValue) {
  Document doc;
  auto root = parse_root(doc, R"({"name":"Alice","age":30})");
  root.merge_patch(R"({"age":31})");
  EXPECT_EQ(root["name"].as<std::string>(), "Alice");  // unchanged tape key
  Document r; auto [json, res] = reparse(r, root);
  EXPECT_EQ(res["age"].as<int>(), 31);
}

TEST(MergePatch, AddNewKey) {
  Document doc;
  auto root = parse_root(doc, R"({"a":1})");
  root.merge_patch(R"({"b":2})");
  EXPECT_EQ(root["a"].as<int>(), 1);  // unchanged tape key
  Document r; auto [json, res] = reparse(r, root);
  EXPECT_EQ(res["b"].as<int>(), 2);
}

TEST(MergePatch, NestedPatch) {
  Document doc;
  auto root = parse_root(doc, R"({"config":{"timeout":5000,"retries":3}})");
  root.merge_patch(R"({"config":{"timeout":10000}})");
  Document r; auto [json, res] = reparse(r, root);
  EXPECT_EQ(res.at("/config/timeout").as<int>(), 10000);
  EXPECT_EQ(res.at("/config/retries").as<int>(), 3);  // unchanged
}

// ── qbuem::read<T>() / qbuem::write<T>() ADL struct binding ──────────────────

struct TestUser {
  std::string name;
  int age = 0;
  bool active = false;
};

inline void from_qbuem_json(const qbuem::Value& v, TestUser& u) {
  u.name   = v["name"]   | std::string{};
  u.age    = v["age"]    | 0;
  u.active = v["active"] | false;
}

inline void to_qbuem_json(qbuem::Value& v, const TestUser& u) {
  v.insert("name",   u.name);
  v.insert("age",    u.age);
  v.insert("active", u.active);
}

TEST(StructBinding, ReadBasic) {
  auto user = qbuem::read<TestUser>(R"({"name":"Alice","age":30,"active":true})");
  EXPECT_EQ(user.name, "Alice");
  EXPECT_EQ(user.age, 30);
  EXPECT_TRUE(user.active);
}

TEST(StructBinding, ReadWithDefaults) {
  auto user = qbuem::read<TestUser>(R"({"name":"Bob"})");
  EXPECT_EQ(user.name, "Bob");
  EXPECT_EQ(user.age, 0);      // default
  EXPECT_FALSE(user.active);   // default
}

TEST(StructBinding, WriteBasic) {
  TestUser u{"Eve", 25, true};
  std::string json = qbuem::write(u);
  // Re-parse and verify
  Document doc;
  auto root = parse(doc, json);
  EXPECT_EQ(root["name"].as<std::string>(), "Eve");
  EXPECT_EQ(root["age"].as<int>(), 25);
  EXPECT_TRUE(root["active"].as<bool>());
}

TEST(StructBinding, RoundTrip) {
  TestUser original{"Charlie", 42, false};
  std::string json = qbuem::write(original);
  auto restored = qbuem::read<TestUser>(json);
  EXPECT_EQ(restored.name, "Charlie");
  EXPECT_EQ(restored.age, 42);
  EXPECT_FALSE(restored.active);
}

// ============================================================================
// Automatic Serialization — Tier 1: built-in STL types
// ============================================================================

// ── is_valid() ────────────────────────────────────────────────────────────────

TEST(IsValid, DefaultValue) {
  qbuem::Value v;
  EXPECT_FALSE(v.is_valid());
}

TEST(IsValid, ParsedValue) {
  Document doc;
  auto root = parse(doc, R"({"x":1})");
  EXPECT_TRUE(root.is_valid());
  EXPECT_TRUE(root["x"].is_valid());
}

TEST(IsValid, MissingKey) {
  Document doc;
  auto root = parse(doc, R"({"x":1})");
  // find() returns optional; operator[] on object may return invalid Value
  auto opt = root.find("y");
  EXPECT_FALSE(opt.has_value());
}

// ── Primitives ────────────────────────────────────────────────────────────────

TEST(AutoSerial, Bool) {
  EXPECT_EQ(qbuem::write(true),  "true");
  EXPECT_EQ(qbuem::write(false), "false");
  EXPECT_EQ(qbuem::read<bool>("true"),  true);
  EXPECT_EQ(qbuem::read<bool>("false"), false);
}

TEST(AutoSerial, Int) {
  EXPECT_EQ(qbuem::write(42),   "42");
  EXPECT_EQ(qbuem::write(-7),   "-7");
  EXPECT_EQ(qbuem::read<int>("42"),  42);
  EXPECT_EQ(qbuem::read<int>("-99"), -99);
}

TEST(AutoSerial, Double) {
  double v = qbuem::read<double>("3.14");
  EXPECT_NEAR(v, 3.14, 1e-9);
  std::string s = qbuem::write(1.5);
  EXPECT_FALSE(s.empty());
  // NaN / Inf → null
  EXPECT_EQ(qbuem::write(std::numeric_limits<double>::infinity()), "null");
  EXPECT_EQ(qbuem::write(std::numeric_limits<double>::quiet_NaN()), "null");
}

TEST(AutoSerial, String) {
  EXPECT_EQ(qbuem::write(std::string("hello")), R"("hello")");
  EXPECT_EQ(qbuem::write(std::string_view("world")), R"("world")");
  EXPECT_EQ(qbuem::read<std::string>(R"("beast")"), "beast");
}

TEST(AutoSerial, StringEscape) {
  // Verify that to_json_str produces correct JSON escape sequences
  std::string s = "a\tb\nc\"d\\e";
  std::string json = qbuem::write(s);
  EXPECT_NE(json.find("\\t"),  std::string::npos);
  EXPECT_NE(json.find("\\n"),  std::string::npos);
  EXPECT_NE(json.find("\\\""), std::string::npos);
  EXPECT_NE(json.find("\\\\"), std::string::npos);
  // Note: qbuem-json uses zero-copy architecture — as<string>() returns raw source
  // bytes without unescaping. Round-trip works for strings with no escapes:
  EXPECT_EQ(qbuem::read<std::string>(R"("hello world")"), "hello world");
}

TEST(AutoSerial, Nullptr) {
  EXPECT_EQ(qbuem::write(nullptr), "null");
}

// ── std::optional ─────────────────────────────────────────────────────────────

TEST(AutoSerial, OptionalPresent) {
  std::optional<int> v = 42;
  EXPECT_EQ(qbuem::write(v), "42");
  auto r = qbuem::read<std::optional<int>>("42");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 42);
}

TEST(AutoSerial, OptionalNull) {
  std::optional<int> v = std::nullopt;
  EXPECT_EQ(qbuem::write(v), "null");
  auto r = qbuem::read<std::optional<int>>("null");
  EXPECT_FALSE(r.has_value());
}

TEST(AutoSerial, OptionalString) {
  auto r = qbuem::read<std::optional<std::string>>(R"("hi")");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "hi");
}

// ── std::vector ───────────────────────────────────────────────────────────────

TEST(AutoSerial, VectorInt) {
  std::vector<int> v = {1, 2, 3};
  EXPECT_EQ(qbuem::write(v), "[1,2,3]");
  auto r = qbuem::read<std::vector<int>>("[10,20,30]");
  ASSERT_EQ(r.size(), 3u);
  EXPECT_EQ(r[1], 20);
}

TEST(AutoSerial, VectorString) {
  std::vector<std::string> v = {"a", "b"};
  std::string json = qbuem::write(v);
  auto r = qbuem::read<std::vector<std::string>>(json);
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "b");
}

TEST(AutoSerial, VectorNested) {
  std::vector<std::vector<int>> v = {{1,2},{3,4}};
  std::string json = qbuem::write(v);
  auto r = qbuem::read<std::vector<std::vector<int>>>(json);
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r[0][1], 2);
  EXPECT_EQ(r[1][0], 3);
}

TEST(AutoSerial, VectorOptional) {
  std::vector<std::optional<int>> v = {1, std::nullopt, 3};
  std::string json = qbuem::write(v);
  EXPECT_EQ(json, "[1,null,3]");
  auto r = qbuem::read<std::vector<std::optional<int>>>(json);
  ASSERT_EQ(r.size(), 3u);
  EXPECT_TRUE(r[0].has_value());
  EXPECT_FALSE(r[1].has_value());
  EXPECT_EQ(*r[2], 3);
}

// ── std::set / std::unordered_set ─────────────────────────────────────────────

TEST(AutoSerial, SetInt) {
  std::set<int> s = {3, 1, 2};
  std::string json = qbuem::write(s);  // sorted: [1,2,3]
  auto r = qbuem::read<std::set<int>>(json);
  EXPECT_EQ(r, s);
}

TEST(AutoSerial, UnorderedSet) {
  std::unordered_set<std::string> s = {"x", "y"};
  std::string json = qbuem::write(s);
  auto r = qbuem::read<std::unordered_set<std::string>>(json);
  EXPECT_EQ(r, s);
}

// ── std::map / std::unordered_map ─────────────────────────────────────────────

TEST(AutoSerial, MapStringInt) {
  std::map<std::string, int> m = {{"a",1}, {"b",2}};
  std::string json = qbuem::write(m);
  auto r = qbuem::read<std::map<std::string, int>>(json);
  EXPECT_EQ(r["a"], 1);
  EXPECT_EQ(r["b"], 2);
}

TEST(AutoSerial, MapStringVector) {
  std::map<std::string, std::vector<int>> m = {{"evens",{2,4,6}},{"odds",{1,3,5}}};
  std::string json = qbuem::write(m);
  auto r = qbuem::read<std::map<std::string, std::vector<int>>>(json);
  ASSERT_EQ(r["evens"].size(), 3u);
  EXPECT_EQ(r["evens"][1], 4);
}

TEST(AutoSerial, UnorderedMapRoundTrip) {
  std::unordered_map<std::string, double> m = {{"pi",3.14},{"e",2.72}};
  std::string json = qbuem::write(m);
  auto r = qbuem::read<std::unordered_map<std::string, double>>(json);
  EXPECT_NEAR(r["pi"], 3.14, 1e-9);
}

// ── std::array ────────────────────────────────────────────────────────────────

TEST(AutoSerial, FixedArray) {
  std::array<int, 4> a = {10, 20, 30, 40};
  EXPECT_EQ(qbuem::write(a), "[10,20,30,40]");
  auto r = qbuem::read<std::array<int, 4>>("[1,2,3,4]");
  EXPECT_EQ(r[2], 3);
}

TEST(AutoSerial, FixedArrayPartialInput) {
  // JSON array shorter than std::array — remaining elements keep default
  auto r = qbuem::read<std::array<int, 3>>("[7,8]");
  EXPECT_EQ(r[0], 7);
  EXPECT_EQ(r[1], 8);
  EXPECT_EQ(r[2], 0);  // default-constructed
}

// ── std::pair / std::tuple ────────────────────────────────────────────────────

TEST(AutoSerial, Pair) {
  std::pair<int, std::string> p = {5, "five"};
  std::string json = qbuem::write(p);  // [5,"five"]
  auto r = qbuem::read<std::pair<int, std::string>>(json);
  EXPECT_EQ(r.first,  5);
  EXPECT_EQ(r.second, "five");
}

TEST(AutoSerial, Tuple) {
  std::tuple<int, std::string, bool> t = {42, "hello", true};
  std::string json = qbuem::write(t);
  auto r = qbuem::read<std::tuple<int, std::string, bool>>(json);
  EXPECT_EQ(std::get<0>(r), 42);
  EXPECT_EQ(std::get<1>(r), "hello");
  EXPECT_TRUE(std::get<2>(r));
}

// ── Direct helpers ────────────────────────────────────────────────────────────

TEST(AutoSerial, FromJsonHelper) {
  Document doc;
  auto root = parse(doc, R"([1,2,3])");
  std::vector<int> v;
  qbuem::from_json(root, v);
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(v[1], 2);
}

TEST(AutoSerial, ToJsonStrHelper) {
  std::vector<bool> v = {true, false, true};
  EXPECT_EQ(qbuem::to_json_str(v), "[true,false,true]");
}

// ============================================================================
// QBUEM_JSON_FIELDS — Tier 2: macro-based struct binding
// ============================================================================

// ── Simple struct ─────────────────────────────────────────────────────────────

struct MacroPoint { int x = 0; int y = 0; };
QBUEM_JSON_FIELDS(MacroPoint, x, y)

TEST(MacroFields, Simple) {
  MacroPoint p = qbuem::read<MacroPoint>(R"({"x":3,"y":7})");
  EXPECT_EQ(p.x, 3);
  EXPECT_EQ(p.y, 7);
}

TEST(MacroFields, SimpleWrite) {
  MacroPoint p{10, 20};
  std::string json = qbuem::write(p);
  auto r = qbuem::read<MacroPoint>(json);
  EXPECT_EQ(r.x, 10);
  EXPECT_EQ(r.y, 20);
}

TEST(MacroFields, MissingFieldsUseDefault) {
  MacroPoint p = qbuem::read<MacroPoint>(R"({"x":5})");
  EXPECT_EQ(p.x, 5);
  EXPECT_EQ(p.y, 0);  // default
}

// ── Nested struct ─────────────────────────────────────────────────────────────

struct MacroAddress { std::string city; std::string country; };
QBUEM_JSON_FIELDS(MacroAddress, city, country)

struct MacroPerson {
  std::string              name;
  int                      age    = 0;
  MacroAddress             addr;
  std::vector<std::string> hobbies;
};
QBUEM_JSON_FIELDS(MacroPerson, name, age, addr, hobbies)

TEST(MacroFields, NestedStruct) {
  constexpr auto JSON = R"({
    "name": "Alice",
    "age": 30,
    "addr": {"city": "Seoul", "country": "KR"},
    "hobbies": ["coding", "reading"]
  })";
  auto p = qbuem::read<MacroPerson>(JSON);
  EXPECT_EQ(p.name,         "Alice");
  EXPECT_EQ(p.age,          30);
  EXPECT_EQ(p.addr.city,    "Seoul");
  EXPECT_EQ(p.addr.country, "KR");
  ASSERT_EQ(p.hobbies.size(), 2u);
  EXPECT_EQ(p.hobbies[0],   "coding");
}

TEST(MacroFields, NestedRoundTrip) {
  MacroPerson original{"Bob", 25, {"Busan","KR"}, {"hiking","gaming","cooking"}};
  std::string json = qbuem::write(original);
  auto restored = qbuem::read<MacroPerson>(json);
  EXPECT_EQ(restored.name,         original.name);
  EXPECT_EQ(restored.age,          original.age);
  EXPECT_EQ(restored.addr.city,    original.addr.city);
  EXPECT_EQ(restored.addr.country, original.addr.country);
  EXPECT_EQ(restored.hobbies,      original.hobbies);
}

// ── Optional fields ───────────────────────────────────────────────────────────

struct MacroWithOpt {
  std::string              name;
  std::optional<int>       score;
  std::optional<MacroPoint> pos;
};
QBUEM_JSON_FIELDS(MacroWithOpt, name, score, pos)

TEST(MacroFields, OptionalAbsent) {
  auto r = qbuem::read<MacroWithOpt>(R"({"name":"test"})");
  EXPECT_EQ(r.name, "test");
  EXPECT_FALSE(r.score.has_value());
  EXPECT_FALSE(r.pos.has_value());
}

TEST(MacroFields, OptionalPresent) {
  auto r = qbuem::read<MacroWithOpt>(R"({"name":"ok","score":99,"pos":{"x":1,"y":2}})");
  ASSERT_TRUE(r.score.has_value());
  EXPECT_EQ(*r.score, 99);
  ASSERT_TRUE(r.pos.has_value());
  EXPECT_EQ(r.pos->x, 1);
  EXPECT_EQ(r.pos->y, 2);
}

TEST(MacroFields, OptionalRoundTrip) {
  MacroWithOpt original{"hello", 77, MacroPoint{3, 4}};
  std::string json = qbuem::write(original);
  auto r = qbuem::read<MacroWithOpt>(json);
  ASSERT_TRUE(r.score.has_value());
  EXPECT_EQ(*r.score, 77);
  ASSERT_TRUE(r.pos.has_value());
  EXPECT_EQ(r.pos->x, 3);
}

// ── All types in one struct ───────────────────────────────────────────────────

struct MacroAllTypes {
  bool                          flag    = false;
  int                           count   = 0;
  double                        ratio   = 0.0;
  std::string                   label;
  std::optional<std::string>    note;
  std::vector<int>              nums;
  std::map<std::string, int>    props;
  std::array<int, 3>            rgb     = {};
};
QBUEM_JSON_FIELDS(MacroAllTypes, flag, count, ratio, label, note, nums, props, rgb)

TEST(MacroFields, AllTypesRoundTrip) {
  MacroAllTypes original;
  original.flag  = true;
  original.count = 42;
  original.ratio = 3.14;
  original.label = "test";
  original.note  = "optional note";
  original.nums  = {10, 20, 30};
  original.props = {{"a", 1}, {"b", 2}};
  original.rgb   = {255, 128, 0};

  std::string json = qbuem::write(original);
  auto r = qbuem::read<MacroAllTypes>(json);

  EXPECT_TRUE(r.flag);
  EXPECT_EQ(r.count, 42);
  EXPECT_NEAR(r.ratio, 3.14, 1e-9);
  EXPECT_EQ(r.label, "test");
  ASSERT_TRUE(r.note.has_value());
  EXPECT_EQ(*r.note, "optional note");
  ASSERT_EQ(r.nums.size(), 3u);
  EXPECT_EQ(r.nums[1], 20);
  EXPECT_EQ(r.props.at("b"), 2);
  EXPECT_EQ(r.rgb[0], 255);
  EXPECT_EQ(r.rgb[2], 0);
}

// ── Deep nesting ──────────────────────────────────────────────────────────────

struct MacroConfig {
  std::string                           version;
  std::map<std::string, std::vector<int>> datasets;
  std::optional<MacroPerson>            owner;
};
QBUEM_JSON_FIELDS(MacroConfig, version, datasets, owner)

TEST(MacroFields, DeepNested) {
  constexpr auto JSON = R"({
    "version": "1.0",
    "datasets": {"train":[1,2,3],"test":[4,5]},
    "owner": {"name":"Carol","age":35,"addr":{"city":"Jeju","country":"KR"},"hobbies":[]}
  })";
  auto cfg = qbuem::read<MacroConfig>(JSON);
  EXPECT_EQ(cfg.version, "1.0");
  EXPECT_EQ(cfg.datasets.at("train").size(), 3u);
  EXPECT_EQ(cfg.datasets.at("test")[1], 5);
  ASSERT_TRUE(cfg.owner.has_value());
  EXPECT_EQ(cfg.owner->name, "Carol");
  EXPECT_EQ(cfg.owner->addr.city, "Jeju");
}

TEST(MacroFields, DeepNestedRoundTrip) {
  MacroConfig original;
  original.version  = "2.0";
  original.datasets = {{"x", {1,2}}, {"y", {3}}};
  original.owner    = MacroPerson{"Dave", 40, {"Incheon","KR"}, {"music"}};

  std::string json = qbuem::write(original);
  auto r = qbuem::read<MacroConfig>(json);

  EXPECT_EQ(r.version, "2.0");
  EXPECT_EQ(r.datasets.at("x")[0], 1);
  ASSERT_TRUE(r.owner.has_value());
  EXPECT_EQ(r.owner->name, "Dave");
  EXPECT_EQ(r.owner->hobbies[0], "music");
}

// ── vector of structs ─────────────────────────────────────────────────────────

TEST(MacroFields, VectorOfStructs) {
  std::vector<MacroPoint> pts = {{1,2},{3,4},{5,6}};
  std::string json = qbuem::write(pts);
  auto r = qbuem::read<std::vector<MacroPoint>>(json);
  ASSERT_EQ(r.size(), 3u);
  EXPECT_EQ(r[1].x, 3);
  EXPECT_EQ(r[2].y, 6);
}

// ── map of structs ────────────────────────────────────────────────────────────

TEST(MacroFields, MapOfStructs) {
  std::map<std::string, MacroPoint> m = {{"origin",{0,0}},{"end",{10,10}}};
  std::string json = qbuem::write(m);
  auto r = qbuem::read<std::map<std::string, MacroPoint>>(json);
  EXPECT_EQ(r.at("end").x, 10);
}
