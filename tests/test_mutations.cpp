#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

// ── Issue #61: Segfault when assigning via operator[] on a missing key ─────────
//
// Root cause: set() / operator= dereferenced doc_ without checking for null.
// operator[] returns Value{} (doc_=nullptr) on a miss; the subsequent
// assignment then caused a null-pointer dereference (segfault).

// Assigning string to a missing key must not crash and must be a no-op.
TEST(Mutations, AssignToMissingKeyNoSideEffect) {
  Document doc;
  auto root = parse(doc, R"({"a":1})");

  // "missing" key returns an invalid Value (doc_=nullptr)
  Value missing = root["missing"];
  EXPECT_FALSE(missing.is_valid());

  // Any of these must NOT crash (was segfaulting before the fix)
  missing = "some_value";
  missing = 42;
  missing = 3.14;
  missing = true;
  missing = nullptr;

  // The original object must be unchanged
  EXPECT_EQ(root["a"].as<int>(), 1);
  EXPECT_EQ(root.dump(), R"({"a":1})");
}

// Chained subscript on missing key must not crash on assignment.
TEST(Mutations, ChainedMissingKeyAssignNoSideEffect) {
  Document doc;
  auto root = parse(doc, R"({"a":{"b":2}})");

  // Both single and multi-level missing access + assignment must be safe.
  root["x"] = "hello";
  root["a"]["z"] = 99;

  // Original data untouched
  EXPECT_EQ(root["a"]["b"].as<int>(), 2);
  EXPECT_EQ(root.dump(), R"({"a":{"b":2}})");
}

// set() on an invalid Value (doc_=nullptr) must be a no-op.
TEST(Mutations, SetOnInvalidValueIsNoOp) {
  Value invalid;  // default-constructed: doc_=nullptr
  EXPECT_FALSE(invalid.is_valid());

  // None of these should crash
  invalid.set(nullptr);
  invalid.set(true);
  invalid.set(false);
  invalid.set(42);
  invalid.set(3.14);
  invalid.set(std::string_view("hello"));
  invalid.set(std::string("world"));
  invalid.set("literal");
  invalid.unset();  // also guarded
}

// operator= on an invalid Value (doc_=nullptr) must be a no-op.
TEST(Mutations, AssignToDefaultConstructedValueIsNoOp) {
  Value v;
  EXPECT_FALSE(v.is_valid());

  v = nullptr;
  v = true;
  v = 0;
  v = 1.0;
  v = "text";
  v = std::string("str");
  v = std::string_view("sv");

  EXPECT_FALSE(v.is_valid());
}

// Valid mutations on an existing key must still work correctly.
TEST(Mutations, SetOnValidKeyMutatesCorrectly) {
  Document doc;
  auto root = parse(doc, R"({"x":0,"s":"old","flag":false})");

  root["x"].set(99);
  root["s"].set(std::string_view("new"));
  root["flag"].set(true);

  EXPECT_EQ(root["x"].as<int>(), 99);
  EXPECT_EQ(root["s"].as<std::string>(), "new");
  EXPECT_TRUE(root["flag"].as<bool>());
}

// operator[] on a missing array index must not crash on assignment.
TEST(Mutations, AssignToMissingArrayIndexNoSideEffect) {
  Document doc;
  auto root = parse(doc, R"([1,2,3])");

  Value out_of_range = root[99];
  EXPECT_FALSE(out_of_range.is_valid());

  out_of_range = "oob";
  out_of_range = 0;

  // Array unchanged
  EXPECT_EQ(root[0].as<int>(), 1);
  EXPECT_EQ(root[1].as<int>(), 2);
  EXPECT_EQ(root[2].as<int>(), 3);
  EXPECT_EQ(root.dump(), "[1,2,3]");
}

// unset() on an invalid Value must be a no-op.
TEST(Mutations, UnsetOnInvalidValueIsNoOp) {
  Value v;
  EXPECT_FALSE(v.is_valid());
  v.unset();  // must not crash
}

// Verify that a valid mutation followed by unset() restores the original.
TEST(Mutations, UnsetRestoresOriginalValue) {
  Document doc;
  auto root = parse(doc, R"({"n":10})");

  root["n"].set(999);
  EXPECT_EQ(root["n"].as<int>(), 999);

  root["n"].unset();
  EXPECT_EQ(root["n"].as<int>(), 10);
}
