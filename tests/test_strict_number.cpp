#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string_view>

using namespace qbuem;

// NOTE: lazy::Parser uses a lenient digit scanner: any sequence starting
// with '-' or '0'-'9' is accepted as a number. Leading zeros, trailing
// dots, incomplete exponents are all accepted (no RFC 8259 strict validation).
// Only chars not in the action LUT ('+', '.') fail as value-starts.

static bool lazy_ok(std::string_view j) {
  try {
    Document doc;
    parse(doc, j);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

// Valid RFC 8259 numbers: always accepted
TEST(StrictNumber, ValidNumbersAccepted) {
  EXPECT_TRUE(lazy_ok("0"));
  EXPECT_TRUE(lazy_ok("123"));
  EXPECT_TRUE(lazy_ok("-5"));
  EXPECT_TRUE(lazy_ok("3.14"));
  EXPECT_TRUE(lazy_ok("1.5e2"));
  EXPECT_TRUE(lazy_ok("-0.5"));
  EXPECT_TRUE(lazy_ok("1e10"));
  EXPECT_TRUE(lazy_ok("-1.5e-3"));
}

// Lenient scanner: accepts non-RFC number formats
TEST(StrictNumber, LenientScannerBehavior) {
  EXPECT_TRUE(lazy_ok("01"));   // leading zero: accepted
  EXPECT_TRUE(lazy_ok("007"));  // leading zeros: accepted
  EXPECT_TRUE(lazy_ok("1."));   // trailing dot: accepted
  EXPECT_TRUE(lazy_ok("1e"));   // incomplete exponent: accepted
}

// Invalid value-start chars: not in action LUT -> parse error
TEST(StrictNumber, InvalidStartCharsRejected) {
  EXPECT_FALSE(lazy_ok("+123")); // '+' not in number case
  EXPECT_FALSE(lazy_ok(".5"));   // '.' not in number case
}

// Valid negative zero
TEST(StrictNumber, NegativeZeroAccepted) {
  EXPECT_TRUE(lazy_ok("-0"));
}
