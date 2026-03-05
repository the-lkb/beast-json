#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <string_view>

using namespace beast;

// NOTE: Neither rtsm::Parser nor lazy::Parser validates control characters
// in strings. scan_string_swar / scan_string_end only stop at '"' and '\'.
// Strings with literal control chars (< 0x20) are accepted by both parsers.

static bool lazy_ok(std::string_view json) {
  try {
    Document doc;
    parse(doc, json);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

// Valid strings (no control chars) are accepted
TEST(ControlChar, ValidStringsAccepted) {
  EXPECT_TRUE(lazy_ok(R"({"a":"valid"})"));
  EXPECT_TRUE(lazy_ok(R"({"a":"hello world"})"));
}

// Control chars in strings: parsers do not validate RFC 8259 requirement
// that unescaped control chars (< 0x20) must be rejected in strings.
// Both parsers accept them silently (scan for '"' and '\' only).
TEST(ControlChar, LiteralNewlineAccepted) {
  std::string json = "{\"key\":\"line1\nline2\"}";
  EXPECT_TRUE(lazy_ok(json));
}

TEST(ControlChar, LiteralTabAccepted) {
  std::string json = "{\"key\":\"tab\tchar\"}";
  EXPECT_TRUE(lazy_ok(json));
}

// Round-trip with JSON escape sequence (not literal control char) is exact.
// Use compact JSON: dump() outputs no whitespace around ':' or ','.
TEST(ControlChar, EscapeSequenceRoundTrip) {
  std::string json = R"({"key":"line1\nline2"})";
  Document doc;
  auto v = parse(doc, json);
  EXPECT_EQ(v.dump(), json);
}
