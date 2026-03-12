#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>

using namespace qbuem;

// Bug 1: Segfault after moving Document
TEST(ReproBugs, MoveDocumentSegfault) {
  Document doc;
  Value root = parse(doc, R"({"key": 42})");
  ASSERT_TRUE(root.is_object());

  Document doc2 = std::move(doc);
  // root still points to &doc, but doc is moved (tape.base is null)
  // Actually, root.doc_ is &doc. Let's see if it segfaults.
  // In many cases, it will if the move didn't change the address but cleared
  // internals.
  // root still points to the same DocumentState which is now shared.
  EXPECT_EQ(root["key"].as<int>(), 42);
}

// Bug 2: insert() is ignored by operator[]
TEST(ReproBugs, InsertIgnoredBySubscript) {
  Document doc;
  Value root = parse(doc, "{}");
  root.insert("new_key", 100);

  // This currently fails in the existing implementation
  Value v = root["new_key"];
  EXPECT_TRUE(v.is_valid());
  if (v.is_valid()) {
    EXPECT_EQ(v.as<int>(), 100);
  }
}

// Bug 3: Array size() ignores array_insertions_
TEST(ReproBugs, ArraySizeIgnoresInsertions) {
  Document doc;
  Value root = parse(doc, "[1, 2]");
  root.insert_json(1, "10"); // Insert 10 at index 1 -> [1, 10, 2]

  // This currently returns 2, should be 3
  EXPECT_EQ(root.size(), 3u);
}

// Bug 4: erase() is ignored by as<T>()
TEST(ReproBugs, EraseIgnoredByAs) {
  Document doc;
  Value root = parse(doc, R"({"key": 42})");
  Value v = root["key"];
  root.erase("key");

  EXPECT_FALSE(root.contains("key"));
  // v still points to tape index, so as<int>() might still return 42
  // but the key is logically erased.
  EXPECT_THROW(v.as<int>(), std::runtime_error);
}

// ── BUG-1: qbuem::parse_reuse not exposed in public facade ───────────────────
//
// qbuem::json::parse_reuse existed internally but qbuem::parse_reuse was not
// forwarded in the public qbuem:: namespace.
// Use explicit qualification to avoid ADL ambiguity with qbuem::json::parse_reuse.
TEST(ReproBugs2, ParseReusePublicFacade) {
  qbuem::Document doc;
  // First parse
  qbuem::Value v1 = qbuem::parse_reuse(doc, R"({"a":1})");
  EXPECT_TRUE(v1.is_object());
  EXPECT_EQ(v1["a"].as<int>(), 1);

  // Reuse same document handle for a second parse
  qbuem::Value v2 = qbuem::parse_reuse(doc, R"({"b":2})");
  EXPECT_TRUE(v2.is_object());
  EXPECT_EQ(v2["b"].as<int>(), 2);
}

// ── BUG-2: unsigned int overload ambiguity ───────────────────────────────────
//
// unsigned int literals (1u, 0u) were ambiguous between size_t and int
// overloads for Value::operator[], Value::erase, SafeValue::get.
TEST(ReproBugs2, UnsignedIntSubscriptNoAmbiguity) {
  Document doc;
  Value arr = parse(doc, "[10,20,30]");

  // All of these must compile and return correct values without a cast
  EXPECT_EQ(arr[0u].as<int>(), 10);
  EXPECT_EQ(arr[1u].as<int>(), 20);
  EXPECT_EQ(arr[2u].as<int>(), 30);
}

TEST(ReproBugs2, UnsignedIntEraseNoAmbiguity) {
  Document doc;
  Value arr = parse(doc, "[10,20,30]");

  // erase(unsigned int) must compile and erase the correct element
  arr.erase(1u);
  EXPECT_EQ(arr.dump(), "[10,30]");
  EXPECT_EQ(arr.size(), 2u);
}

TEST(ReproBugs2, SafeValueGetUnsignedIntNoAmbiguity) {
  Document doc;
  Value arr = parse(doc, "[10,20,30]");
  SafeValue sv(arr);

  // SafeValue::get(unsigned int) must compile without cast
  auto v0 = sv.get(0u);
  ASSERT_TRUE(v0.has_value());
  EXPECT_EQ(v0.as<int>().value_or(-1), 10);

  auto v1 = sv[1u];
  ASSERT_TRUE(v1.has_value());
  EXPECT_EQ(v1.as<int>().value_or(-1), 20);
}

// ── BUG-3: push_back() / push_back_json() leaves size() stale ────────────────
//
// After push_back(), size() returned the original parsed count (frozen)
// because additions_ was not counted in the array size calculation.
TEST(ReproBugs2, PushBackSizeReflectsNewElements) {
  Document doc;
  Value v = parse(doc, R"([])");

  EXPECT_EQ(v.size(), 0u);
  v.push_back(1);
  EXPECT_EQ(v.size(), 1u);
  v.push_back(2.5);
  EXPECT_EQ(v.size(), 2u);
  v.push_back("hello");
  EXPECT_EQ(v.size(), 3u);

  EXPECT_EQ(v.dump(), R"([1,2.5,"hello"])");
}

TEST(ReproBugs2, PushBackOnNonEmptyArraySizeCorrect) {
  Document doc;
  Value v = parse(doc, R"([10, 20])");

  ASSERT_EQ(v.size(), 2u);
  v.push_back(30);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v.dump(), "[10,20,30]");
}

TEST(ReproBugs2, PushBackJsonSizeReflectsNewElements) {
  Document doc;
  Value v = parse(doc, "[1,2,3]");

  v.push_back_json("4");
  EXPECT_EQ(v.size(), 4u);
  EXPECT_EQ(v.dump(), "[1,2,3,4]");
}

// ── BUG-4: insert() leaves items() iteration truncated ───────────────────────
//
// After insert(), items() only iterated original parsed keys.
// Newly inserted keys were invisible in range-based for loop.
TEST(ReproBugs2, ItemsIncludesInsertedKeys) {
  Document doc;
  Value v = parse(doc, R"({"a":1,"b":2})");
  v.insert("c", 3);

  EXPECT_EQ(v.size(), 3u);

  std::set<std::string> keys;
  for (auto [k, val] : v.items())
    keys.insert(std::string(k));

  EXPECT_EQ(keys.size(), 3u);
  EXPECT_TRUE(keys.count("a"));
  EXPECT_TRUE(keys.count("b"));
  EXPECT_TRUE(keys.count("c")); // was missing before fix
}

TEST(ReproBugs2, ItemsIncludesInsertedKeysAfterErase) {
  Document doc;
  Value v = parse(doc, R"({"a":1,"b":2})");
  v.insert("c", 3);
  v.erase("a");

  // dump and size are correct
  EXPECT_EQ(v.size(), 2u);
  EXPECT_NE(v.dump().find("\"b\":2"), std::string::npos);
  EXPECT_NE(v.dump().find("\"c\":3"), std::string::npos);

  std::set<std::string> keys;
  for (auto [k, val] : v.items())
    keys.insert(std::string(k));

  EXPECT_EQ(keys.size(), 2u);
  EXPECT_FALSE(keys.count("a")); // erased
  EXPECT_TRUE(keys.count("b"));
  EXPECT_TRUE(keys.count("c")); // was missing before fix
}

TEST(ReproBugs2, ItemsValuesOfInsertedKeys) {
  Document doc;
  Value v = parse(doc, R"({"x":10})");
  v.insert("y", 20);
  v.insert("z", 30);

  std::map<std::string, int> got;
  for (auto [k, val] : v.items())
    got[std::string(k)] = val.as<int>();

  EXPECT_EQ(got.size(), 3u);
  EXPECT_EQ(got["x"], 10);
  EXPECT_EQ(got["y"], 20);
  EXPECT_EQ(got["z"], 30);
}

// ── OBS-1: unset() reverts to original parsed value, NOT null reset ──────────
//
// unset() removes the mutation overlay so as<T>() and type_name() return the
// original parsed value/type. It does NOT reset the value to null.
TEST(Observations, UnsetRevertsToOriginalValue) {
  Document doc;
  Value root = parse(doc, R"({"n":42,"s":"hello","flag":true})");

  // Mutate, then revert
  root["n"].set(999);
  EXPECT_EQ(root["n"].as<int>(), 999);
  root["n"].unset();
  EXPECT_EQ(root["n"].as<int>(), 42); // original value restored

  root["s"].set("world");
  EXPECT_EQ(root["s"].as<std::string>(), "world");
  root["s"].unset();
  EXPECT_EQ(root["s"].as<std::string>(), "hello"); // original restored

  root["flag"].set(false);
  EXPECT_FALSE(root["flag"].as<bool>());
  root["flag"].unset();
  EXPECT_TRUE(root["flag"].as<bool>()); // original true restored
}

TEST(Observations, UnsetTypeNameRetainsOriginalType) {
  Document doc;
  Value root = parse(doc, R"({"b":false,"n":10})");

  // Set bool to bool — after unset, still bool (original type preserved)
  root["b"].set(true);
  EXPECT_EQ(root["b"].type_name(), "bool");
  root["b"].unset();
  // type_name() returns original parsed type, not "null"
  EXPECT_EQ(root["b"].type_name(), "bool");
  EXPECT_FALSE(root["b"].as<bool>()); // original value: false

  // Set int to int — after unset, still int
  root["n"].set(99);
  root["n"].unset();
  EXPECT_EQ(root["n"].type_name(), "int");
  EXPECT_EQ(root["n"].as<int>(), 10);
}

// ── OBS-2: operator[] returns invalid Value on miss, does NOT throw ───────────
//
// The API documentation previously said "throws on miss (like STL at())".
// The actual behavior is non-throwing: a miss returns an invalid Value{}.
// Only calling as<T>() on that invalid Value subsequently throws.
TEST(Observations, SubscriptMissReturnsInvalidNotThrow) {
  Document doc;
  Value root = parse(doc, R"({"a":1})");

  // Object miss — must not throw; must return invalid Value
  Value miss_obj = root["nonexistent"];
  EXPECT_FALSE(miss_obj.is_valid());

  // Array miss — must not throw; must return invalid Value
  Value arr = parse(doc, "[1,2,3]");
  Value miss_arr = arr[99];
  EXPECT_FALSE(miss_arr.is_valid());

  // Chained miss — each level is a no-throw invalid Value
  Value deep = root["x"]["y"]["z"];
  EXPECT_FALSE(deep.is_valid());
}

TEST(Observations, AsOnInvalidValueThrows) {
  Document doc;
  Value root = parse(doc, R"({"a":1})");

  // operator[] itself does not throw, but as<T>() on the result does
  Value miss = root["nonexistent"];
  EXPECT_FALSE(miss.is_valid());
  EXPECT_THROW(miss.as<int>(), std::runtime_error);
  EXPECT_THROW(miss.as<std::string>(), std::runtime_error);
}

// ── OBS-3: as_array<T>() throws on type mismatch; try_as_array<T>() is safe ──
//
// as_array<T>() throws std::runtime_error when an element cannot be converted
// to T. try_as_array<T>() returns std::optional<T> and never throws.
TEST(Observations, AsArrayThrowsOnTypeMismatch) {
  Document doc;
  Value v = parse(doc, R"([1,"two",3])");

  bool threw = false;
  try {
    for (int x : v.as_array<int>()) {
      (void)x; // suppress unused warning
    }
  } catch (const std::runtime_error &) {
    threw = true;
  }
  EXPECT_TRUE(threw); // throws when reaching "two"
}

TEST(Observations, TryAsArrayHandlesMixedTypes) {
  Document doc;
  Value v = parse(doc, R"([1,"two",3])");

  std::vector<int> ints;
  for (auto maybe : v.try_as_array<int>()) {
    if (maybe.has_value())
      ints.push_back(maybe.value());
  }

  // Only integer elements are collected; "two" silently skipped
  ASSERT_EQ(ints.size(), 2u);
  EXPECT_EQ(ints[0], 1);
  EXPECT_EQ(ints[1], 3);
}

TEST(Observations, TryAsArrayNeverThrows) {
  Document doc;
  Value v = parse(doc, R"([true,42,"text",null,3.14])");

  // Mixed type array — try_as_array<int> never throws regardless of types
  EXPECT_NO_THROW({
    for ([[maybe_unused]] auto maybe : v.try_as_array<int>()) {
    }
  });
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
