/**
 * test_rfc8259.cpp — RFC 8259 Compliance Tests
 *
 * Tests qbuem::parse_strict() against RFC 8259 (the JSON specification).
 * Test naming follows the JSONTestSuite convention:
 *   y_ → must accept (valid JSON)
 *   n_ → must reject (invalid JSON)
 *   i_ → implementation-defined (we document our choice)
 *
 * RFC 8259 §2: A JSON text is a serialized value (any type at top level).
 */

#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>

using namespace qbuem;

// ── Helpers
// ───────────────────────────────────────────────────────────────────

static bool strict_ok(std::string_view json) {
  try {
    Document doc;
    parse_strict(doc, json);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

static bool strict_fail(std::string_view json) { return !strict_ok(json); }

// ── y_: Must Accept — valid JSON values ──────────────────────────────────────

TEST(RFC8259_Accept, Null) { EXPECT_TRUE(strict_ok("null")); }

TEST(RFC8259_Accept, BoolTrue) { EXPECT_TRUE(strict_ok("true")); }

TEST(RFC8259_Accept, BoolFalse) { EXPECT_TRUE(strict_ok("false")); }

TEST(RFC8259_Accept, EmptyObject) { EXPECT_TRUE(strict_ok("{}")); }

TEST(RFC8259_Accept, EmptyArray) { EXPECT_TRUE(strict_ok("[]")); }

TEST(RFC8259_Accept, SimpleObject) {
  EXPECT_TRUE(strict_ok(R"({"a":1})"));
  EXPECT_TRUE(strict_ok(R"({"name":"Alice","age":30})"));
}

TEST(RFC8259_Accept, SimpleArray) {
  EXPECT_TRUE(strict_ok("[1,2,3]"));
  EXPECT_TRUE(strict_ok(R"(["a","b","c"])"));
}

TEST(RFC8259_Accept, NestedStructures) {
  EXPECT_TRUE(strict_ok(R"({"users":[{"id":1},{"id":2}]})"));
  EXPECT_TRUE(strict_ok("[[[]]]"));
  EXPECT_TRUE(strict_ok(R"([[1,[2,[3]]]])"));
}

// Numbers
TEST(RFC8259_Accept, NumberZero) {
  EXPECT_TRUE(strict_ok("0"));
  EXPECT_TRUE(strict_ok("-0"));
  EXPECT_TRUE(strict_ok("0.0"));
  EXPECT_TRUE(strict_ok("-0.0"));
}

TEST(RFC8259_Accept, NumberInteger) {
  EXPECT_TRUE(strict_ok("1"));
  EXPECT_TRUE(strict_ok("42"));
  EXPECT_TRUE(strict_ok("-5"));
  EXPECT_TRUE(strict_ok("1000000"));
}

TEST(RFC8259_Accept, NumberFloat) {
  EXPECT_TRUE(strict_ok("3.14"));
  EXPECT_TRUE(strict_ok("-3.14"));
  EXPECT_TRUE(strict_ok("0.5"));
  EXPECT_TRUE(strict_ok("1.0"));
}

TEST(RFC8259_Accept, NumberExponent) {
  EXPECT_TRUE(strict_ok("1e10"));
  EXPECT_TRUE(strict_ok("1E10"));
  EXPECT_TRUE(strict_ok("1e+10"));
  EXPECT_TRUE(strict_ok("1e-10"));
  EXPECT_TRUE(strict_ok("1.5e2"));
  EXPECT_TRUE(strict_ok("-2.5E-3"));
  EXPECT_TRUE(strict_ok("1.23e+456"));
}

TEST(RFC8259_Accept, NumberZeroFractional) {
  EXPECT_TRUE(strict_ok("0.5"));
  EXPECT_TRUE(strict_ok("0.123"));
}

// Strings
TEST(RFC8259_Accept, StringEmpty) { EXPECT_TRUE(strict_ok(R"("")")); }

TEST(RFC8259_Accept, StringSimple) {
  EXPECT_TRUE(strict_ok(R"("hello")"));
  EXPECT_TRUE(strict_ok(R"("hello world")"));
}

TEST(RFC8259_Accept, StringEscapeSequences) {
  EXPECT_TRUE(strict_ok(R"("\"")")); // quote
  EXPECT_TRUE(strict_ok(R"("\\")")); // backslash
  EXPECT_TRUE(strict_ok(R"("\/")")); // forward slash
  EXPECT_TRUE(strict_ok(R"("\b")")); // backspace
  EXPECT_TRUE(strict_ok(R"("\f")")); // form feed
  EXPECT_TRUE(strict_ok(R"("\n")")); // newline
  EXPECT_TRUE(strict_ok(R"("\r")")); // carriage return
  EXPECT_TRUE(strict_ok(R"("\t")")); // tab
}

TEST(RFC8259_Accept, StringUnicodeEscape) {
  EXPECT_TRUE(strict_ok(R"("\u0041")")); // A
  EXPECT_TRUE(strict_ok(R"("\u0000")")); // null char (escaped)
  EXPECT_TRUE(strict_ok(R"("\uFFFF")")); // high BMP
  EXPECT_TRUE(strict_ok(R"("\u0048\u0065\u006C\u006C\u006F")")); // Hello
}

TEST(RFC8259_Accept, StringSurrogatePair) {
  // \uD83D\uDE00 = U+1F600 (grinning face emoji) — valid surrogate pair
  EXPECT_TRUE(strict_ok(R"("\uD83D\uDE00")"));
}

TEST(RFC8259_Accept, StringMultibyte) {
  EXPECT_TRUE(strict_ok(R"("한글")"));   // Korean characters (UTF-8)
  EXPECT_TRUE(strict_ok(R"("日本語")")); // Japanese
}

// Whitespace
TEST(RFC8259_Accept, Whitespace) {
  EXPECT_TRUE(strict_ok("  null  "));
  EXPECT_TRUE(strict_ok("\t{\t}\t"));
  EXPECT_TRUE(strict_ok("\n[\n]\n"));
  EXPECT_TRUE(strict_ok("\r\n1\r\n"));
}

// Bare top-level values (RFC 8259 §2 allows any value)
TEST(RFC8259_Accept, TopLevelNumber) {
  EXPECT_TRUE(strict_ok("42"));
  EXPECT_TRUE(strict_ok("3.14"));
}

TEST(RFC8259_Accept, TopLevelString) { EXPECT_TRUE(strict_ok(R"("hello")")); }

TEST(RFC8259_Accept, TopLevelArray) { EXPECT_TRUE(strict_ok("[1,2,3]")); }

// ── n_: Must Reject — invalid JSON ───────────────────────────────────────────

// Trailing commas
TEST(RFC8259_Reject, TrailingCommaArray) {
  EXPECT_TRUE(strict_fail("[1,]"));
  EXPECT_TRUE(strict_fail("[1,2,]"));
  EXPECT_TRUE(strict_fail("[,]"));
}

TEST(RFC8259_Reject, TrailingCommaObject) {
  EXPECT_TRUE(strict_fail(R"({"a":1,})"));
  EXPECT_TRUE(strict_fail(R"({"a":1,"b":2,})"));
}

// Leading zeros in numbers
TEST(RFC8259_Reject, LeadingZeroInteger) {
  EXPECT_TRUE(strict_fail("01"));
  EXPECT_TRUE(strict_fail("007"));
  EXPECT_TRUE(strict_fail("-01"));
  EXPECT_TRUE(strict_fail("00"));
}

TEST(RFC8259_Reject, LeadingZeroFloat) {
  EXPECT_TRUE(strict_fail("01.5"));
  EXPECT_TRUE(strict_fail("-007.5"));
}

// Invalid number format
TEST(RFC8259_Reject, TrailingDecimalPoint) {
  EXPECT_TRUE(strict_fail("1."));
  EXPECT_TRUE(strict_fail("-1."));
  EXPECT_TRUE(strict_fail("0."));
}

TEST(RFC8259_Reject, IncompleteExponent) {
  EXPECT_TRUE(strict_fail("1e"));
  EXPECT_TRUE(strict_fail("1E"));
  EXPECT_TRUE(strict_fail("1e+"));
  EXPECT_TRUE(strict_fail("1e-"));
  EXPECT_TRUE(strict_fail("1.5e"));
}

TEST(RFC8259_Reject, LeadingPlus) {
  EXPECT_TRUE(strict_fail("+1"));
  EXPECT_TRUE(strict_fail("+0"));
}

TEST(RFC8259_Reject, BareDecimal) {
  EXPECT_TRUE(strict_fail(".5"));
  EXPECT_TRUE(strict_fail("-.5"));
}

// String violations
TEST(RFC8259_Reject, UnterminatedString) {
  EXPECT_TRUE(strict_fail(R"("unterminated)"));
  EXPECT_TRUE(strict_fail(R"({"key": "val)"));
}

TEST(RFC8259_Reject, InvalidEscapeSequence) {
  EXPECT_TRUE(strict_fail(R"("\x41")")); // \x not valid
  EXPECT_TRUE(strict_fail(R"("\a")"));   // \a not valid
  EXPECT_TRUE(strict_fail(R"("\z")"));   // \z not valid
  EXPECT_TRUE(strict_fail(R"("\0")"));   // \0 not valid (must use \u0000)
}

TEST(RFC8259_Reject, IncompleteUnicodeEscape) {
  EXPECT_TRUE(strict_fail(R"("\u")"));
  EXPECT_TRUE(strict_fail(R"("\u00")"));
  EXPECT_TRUE(strict_fail(R"("\u004")"));
  EXPECT_TRUE(strict_fail(R"("\uGGGG")")); // non-hex digits
}

TEST(RFC8259_Reject, UnescapedControlCharInString) {
  // NUL byte inside string without escape
  EXPECT_TRUE(strict_fail(std::string_view("\"a\x00b\"", 5)));
  // Tab without escape
  EXPECT_TRUE(strict_fail("\"a\tb\""));
  // Newline without escape
  EXPECT_TRUE(strict_fail("\"a\nb\""));
}

// Structural errors
TEST(RFC8259_Reject, UnterminatedArray) {
  EXPECT_TRUE(strict_fail("["));
  EXPECT_TRUE(strict_fail("[1,2"));
  EXPECT_TRUE(strict_fail("[1,[2,3]"));
}

TEST(RFC8259_Reject, UnterminatedObject) {
  EXPECT_TRUE(strict_fail("{"));
  EXPECT_TRUE(strict_fail(R"({"a":1)"));
}

TEST(RFC8259_Reject, MissingComma) {
  EXPECT_TRUE(strict_fail("[1 2]"));
  EXPECT_TRUE(strict_fail(R"({"a":1 "b":2})"));
}

TEST(RFC8259_Reject, MissingColon) { EXPECT_TRUE(strict_fail(R"({"a" 1})")); }

TEST(RFC8259_Reject, MissingValue) { EXPECT_TRUE(strict_fail(R"({"a":})")); }

TEST(RFC8259_Reject, SingleQuotes) { EXPECT_TRUE(strict_fail("{'a': 'b'}")); }

TEST(RFC8259_Reject, UnquotedKey) { EXPECT_TRUE(strict_fail("{a: 1}")); }

TEST(RFC8259_Reject, UnquotedString) {
  EXPECT_TRUE(strict_fail("hello"));
  EXPECT_TRUE(strict_fail("[hello]"));
}

TEST(RFC8259_Reject, InvalidLiteral) {
  EXPECT_TRUE(strict_fail("tru"));
  EXPECT_TRUE(strict_fail("truee"));
  EXPECT_TRUE(strict_fail("Truth"));
  EXPECT_TRUE(strict_fail("NULL"));
  EXPECT_TRUE(strict_fail("True"));
  EXPECT_TRUE(strict_fail("False"));
}

TEST(RFC8259_Reject, TrailingContent) {
  EXPECT_TRUE(strict_fail("{}extra"));
  EXPECT_TRUE(strict_fail("[1]garbage"));
  EXPECT_TRUE(strict_fail("1 2"));
  EXPECT_TRUE(strict_fail("null{}"));
}

TEST(RFC8259_Reject, EmptyInput) {
  EXPECT_TRUE(strict_fail(""));
  EXPECT_TRUE(strict_fail("   "));
}

TEST(RFC8259_Reject, BareComma) {
  EXPECT_TRUE(strict_fail(","));
  EXPECT_TRUE(strict_fail("[,1]"));
}

// ── i_: Implementation-defined (document our behaviour) ──────────

TEST(RFC8259_ImplDefined, DuplicateKeys_Accepted) {
  // RFC 8259 §4: "The names within an object SHOULD be unique"
  // (SHOULD, not MUST) — qbuem-json accepts duplicates (last-write-wins on tape)
  EXPECT_TRUE(strict_ok(R"({"a":1,"a":2})"));
}

TEST(RFC8259_ImplDefined, DeepNesting_Accepted) {
  // RFC 8259 does not specify a depth limit; we accept up to available stack
  std::string deep = std::string(64, '[') + "1" + std::string(64, ']');
  EXPECT_TRUE(strict_ok(deep));

  // Test maximum practical nesting before general fallback limits out
  std::string very_deep = std::string(512, '[') + "1" + std::string(512, ']');
  EXPECT_TRUE(strict_ok(very_deep));
}

TEST(RFC8259_ImplDefined, DeepObjectNesting_Accepted) {
  std::string deep_obj;
  for (int i = 0; i < 300; i++)
    deep_obj += R"({"a":)";
  deep_obj += "1";
  for (int i = 0; i < 300; i++)
    deep_obj += "}";
  EXPECT_TRUE(strict_ok(deep_obj));
}

TEST(RFC8259_ImplDefined, LargeNumber_Accepted) {
  // Numbers exceeding int64 range are stored as double
  EXPECT_TRUE(strict_ok("99999999999999999999999999999999"));
  EXPECT_TRUE(strict_ok("1e308"));
}

TEST(RFC8259_ImplDefined, SubnormalFloats_Accepted) {
  // Parsing subnormals without blowing up
  EXPECT_TRUE(strict_ok("4.9406564584124654e-324"));
}

TEST(RFC8259_ImplDefined, NullByteInString_MustReject) {
  // strictly RFC: Unescaped control characters MUST be rejected
  EXPECT_TRUE(strict_fail(std::string_view("\"\0\"", 3)));
}

TEST(RFC8259_ImplDefined, UnescapedSurrogateHalves_StringParsing) {
  // Although RFC 8259 states string content is UTF-8, isolated surrogates
  // are often an implementation detail. We ensure they pass basic parse
  // but may render as garbage or trigger undefined behavior if heavily mutated.
  // Standard surrogate escape
  EXPECT_TRUE(strict_ok(R"("\uD83D")"));
}

TEST(RFC8259_ImplDefined, EdgeCaseWhiteSpace) {
  // Lots of boundary whitespaces
  std::string pad(10000, ' ');
  EXPECT_TRUE(strict_ok(pad + "{}" + pad));
}

// ── Roundtrip via parse_strict
// ────────────────────────────────────────────────

TEST(RFC8259_Roundtrip, ObjectRoundtrip) {
  constexpr auto JSON = R"({"name":"Alice","age":30,"active":true})";
  Document doc;
  Value root = parse_strict(doc, JSON);
  EXPECT_EQ(root["name"].as<std::string>(), "Alice");
  EXPECT_EQ(root["age"].as<int>(), 30);
  EXPECT_TRUE(root["active"].as<bool>());
}

TEST(RFC8259_Roundtrip, ArrayRoundtrip) {
  Document doc;
  Value root = parse_strict(doc, "[10,20,30]");
  EXPECT_EQ(root[0].as<int>(), 10);
  EXPECT_EQ(root[1].as<int>(), 20);
  EXPECT_EQ(root[2].as<int>(), 30);
}

TEST(RFC8259_Roundtrip, NumberPrecision) {
  Document doc;
  Value root = parse_strict(doc, "3.141592653589793");
  double v = root.as<double>();
  EXPECT_NEAR(v, 3.141592653589793, 1e-15);
}

// ── rfc8259::validate() standalone API ───────────────────────────────────────

TEST(RFC8259_API, ValidateStandaloneOk) {
  EXPECT_NO_THROW(rfc8259::validate("null"));
  EXPECT_NO_THROW(rfc8259::validate("[1,2,3]"));
  EXPECT_NO_THROW(rfc8259::validate(R"({"a":1})"));
}

TEST(RFC8259_API, ValidateStandaloneFail) {
  EXPECT_THROW(rfc8259::validate("[1,]"), std::runtime_error);
  EXPECT_THROW(rfc8259::validate("01"), std::runtime_error);
}

TEST(RFC8259_API, ErrorMessageContainsOffset) {
  try {
    rfc8259::validate("[1,]");
    FAIL() << "Expected runtime_error";
  } catch (const std::runtime_error &e) {
    std::string msg = e.what();
    EXPECT_NE(msg.find("offset"), std::string::npos)
        << "Error message should contain byte offset: " << msg;
  }
}

// ── RFC 6901: JSON Pointer ───────────────────────────────────────────────────

TEST(RFC6901_Pointer, BasicNavigation) {
  Document doc;
  auto root = parse(doc, R"({"foo":["bar","baz"],"":0,"a/b":1,"m~n":8})");
  EXPECT_EQ(root.at("").dump(), root.dump());
  EXPECT_EQ(root.at("/foo/0").as<std::string>(), "bar");
  EXPECT_EQ(root.at("/").as<int>(), 0);
  EXPECT_EQ(root.at("/a~1b").as<int>(), 1);
  EXPECT_EQ(root.at("/m~0n").as<int>(), 8);
}

TEST(RFC6901_Pointer, LeadingZerosMustReject) {
  Document doc;
  auto root = parse(doc, "[10,20,30]");
  // RFC 6901: leading zeros are not allowed unless the index is exactly "0"
  EXPECT_FALSE(root.at("/01").is_valid());
  EXPECT_TRUE(root.at("/0").is_valid());
}

TEST(RFC6901_Pointer, ArrayEndOfArrayToken) {
  Document doc;
  auto root = parse(doc, "[1,2,3]");
  // "-" is valid pointer but points to non-existent element
  EXPECT_FALSE(root.at("/-").is_valid());
}

// ── RFC 6902: JSON Patch
// ──────────────────────────────────────────────────────

static std::string apply_patch(Value &v, std::string_view patch_json) {
  if (!v.patch(patch_json))
    return "PATCH_FAILED";
  return v.dump();
}

TEST(RFC6902_Patch, AddObjectMember) {
  Document doc;
  auto root = parse(doc, R"({"foo":"bar"})");
  EXPECT_EQ(
      apply_patch(root, R"([{"op":"add", "path":"/baz", "value":"qux"}])"),
      R"({"foo":"bar","baz":"qux"})");
}

TEST(RFC6902_Patch, AddArrayElementPosition) {
  Document doc;
  auto root = parse(doc, R"(["a","c"])");
  // Add "b" at index 1 -> ["a","b","c"]
  EXPECT_EQ(apply_patch(root, R"([{"op":"add", "path":"/1", "value":"b"}])"),
            R"(["a","b","c"])");
}

TEST(RFC6902_Patch, AddArrayEnd) {
  Document doc;
  auto root = parse(doc, "[1,2]");
  EXPECT_EQ(apply_patch(root, R"([{"op":"add", "path":"/-", "value":3}])"),
            "[1,2,3]");
}

TEST(RFC6902_Patch, Remove) {
  Document doc;
  auto root = parse(doc, R"({"a":1,"b":2})");
  EXPECT_EQ(apply_patch(root, R"([{"op":"remove", "path":"/a"}])"),
            R"({"b":2})");
}

TEST(RFC6902_Patch, Replace) {
  Document doc;
  auto root = parse(doc, R"({"a":1})");
  EXPECT_EQ(apply_patch(root, R"([{"op":"replace", "path":"/a", "value":42}])"),
            R"({"a":42})");
}

TEST(RFC6902_Patch, TestOp) {
  Document doc;
  auto root = parse(doc, R"({"baz":"qux"})");
  EXPECT_TRUE(root.patch(R"([{"op":"test", "path":"/baz", "value":"qux"}])"));
  EXPECT_FALSE(
      root.patch(R"([{"op":"test", "path":"/baz", "value":"error"}])"));
}

TEST(RFC6902_Patch, Move) {
  Document doc;
  auto root = parse(doc, R"({"foo":{"bar":"baz"},"qux":{}})");
  // Move /foo/bar to /qux/thud
  EXPECT_EQ(
      apply_patch(root,
                  R"([{"op":"move", "from":"/foo/bar", "path":"/qux/thud"}])"),
      R"({"foo":{},"qux":{"thud":"baz"}})");
}

TEST(RFC6902_Patch, Copy) {
  Document doc;
  auto root = parse(doc, R"({"a":1})");
  EXPECT_EQ(apply_patch(root, R"([{"op":"copy", "from":"/a", "path":"/b"}])"),
            R"({"a":1,"b":1})");
}

TEST(RFC6902_Patch, TransactionalRollback) {
  Document doc;
  auto root = parse(doc, R"({"a":1})");
  // Transactional failure: 2nd op fails, 1st must rollback
  bool ok = root.patch(R"([
    {"op":"replace", "path":"/a", "value":2},
    {"op":"test", "path":"/a", "value":999}
  ])");
  EXPECT_FALSE(ok);
  EXPECT_EQ(root.dump(), R"({"a":1})");
}
