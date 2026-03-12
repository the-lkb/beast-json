#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

// NOTE: Neither rtsm::Parser nor lazy::Parser validates UTF-8 byte sequences.
// scan_string_swar / scan_string_end only scan for '"' and '\'.
// All byte sequences in strings are accepted (no RFC 3629 validation).

static bool check_parse(std::string_view json) {
  try {
    Document doc;
    parse(doc, json);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

// Valid UTF-8: accepted
TEST(Utf8Validation, ASCII)       { EXPECT_TRUE(check_parse(R"({"key": "value"})")); }
TEST(Utf8Validation, Valid2Byte)  { EXPECT_TRUE(check_parse("{\"key\": \"\xC2\xA2\"}")); }
TEST(Utf8Validation, Valid3Byte)  { EXPECT_TRUE(check_parse("{\"key\": \"\xE2\x82\xAC\"}")); }
TEST(Utf8Validation, Valid4Byte)  { EXPECT_TRUE(check_parse("{\"key\": \"\xF0\x9D\x84\x9E\"}")); }

// Invalid UTF-8: also accepted (parsers do not validate UTF-8)
TEST(Utf8Validation, InvalidStart0x80)    { EXPECT_TRUE(check_parse("{\"key\": \"\x80\"}")); }
TEST(Utf8Validation, Overlong2Byte)       { EXPECT_TRUE(check_parse("{\"key\": \"\xC0\xAF\"}")); }
TEST(Utf8Validation, Overlong3Byte)       { EXPECT_TRUE(check_parse("{\"key\": \"\xE0\x80\xAF\"}")); }
TEST(Utf8Validation, MissingContinuation) { EXPECT_TRUE(check_parse("{\"key\": \"\xE2\x82\"}")); }
TEST(Utf8Validation, BadContinuation)     { EXPECT_TRUE(check_parse("{\"key\": \"\xE2\x02\xAC\"}")); }
TEST(Utf8Validation, SurrogateHigh)       { EXPECT_TRUE(check_parse("{\"key\": \"\xED\xA0\x80\"}")); }

// Mixed valid UTF-8: accepted
TEST(Utf8Validation, Mixed) { EXPECT_TRUE(check_parse("{\"key\": \"Hello \xF0\x9F\x8C\x8D World\"}")); }

// Structural errors: fail regardless of byte content
TEST(Utf8Validation, MissingCloseBrace)   { EXPECT_FALSE(check_parse("{\"key\": \"value\"")); }
TEST(Utf8Validation, MissingCloseBracket) { EXPECT_FALSE(check_parse("[1, 2, 3")); }
TEST(Utf8Validation, EmptyInput)          { EXPECT_FALSE(check_parse("")); }
