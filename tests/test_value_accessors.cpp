#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace beast;

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

using namespace beast::json::lazy; // access SafeValue type

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
  static_assert(beast::json::lazy::JsonInteger<int>);
  static_assert(beast::json::lazy::JsonInteger<long>);
  static_assert(beast::json::lazy::JsonInteger<int64_t>);
  static_assert(!beast::json::lazy::JsonInteger<bool>);
  static_assert(!beast::json::lazy::JsonInteger<float>);
  SUCCEED();
}

TEST(Concepts, JsonFloatConcept) {
  static_assert(beast::json::lazy::JsonFloat<float>);
  static_assert(beast::json::lazy::JsonFloat<double>);
  static_assert(!beast::json::lazy::JsonFloat<int>);
  static_assert(!beast::json::lazy::JsonFloat<bool>);
  SUCCEED();
}

TEST(Concepts, JsonReadableConcept) {
  static_assert(beast::json::lazy::JsonReadable<bool>);
  static_assert(beast::json::lazy::JsonReadable<int>);
  static_assert(beast::json::lazy::JsonReadable<double>);
  static_assert(beast::json::lazy::JsonReadable<std::string>);
  static_assert(beast::json::lazy::JsonReadable<std::string_view>);
  SUCCEED();
}

TEST(Concepts, JsonWritableConcept) {
  static_assert(beast::json::lazy::JsonWritable<bool>);
  static_assert(beast::json::lazy::JsonWritable<int>);
  static_assert(beast::json::lazy::JsonWritable<double>);
  static_assert(beast::json::lazy::JsonWritable<std::string_view>);
  static_assert(beast::json::lazy::JsonWritable<std::nullptr_t>);
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
