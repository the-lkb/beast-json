#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace beast;

// NOTE: Neither rtsm::Parser nor lazy::Parser validates UTF-8 byte sequences.
// scan_string_swar / scan_string_end only scan for '"' and '\'.
// All byte sequences in strings are accepted (no RFC 3629 validation).

class Utf8Validation : public ::testing::TestWithParam<
                           std::tuple<std::string, std::string, bool>> {
protected:
  bool check_parse(std::string_view json) {
    try {
      Document doc;
      parse(doc, json);
      return true;
    } catch (const std::runtime_error &) {
      return false;
    }
  }
};

TEST_P(Utf8Validation, Check) {
  auto [name, json, should_pass] = GetParam();
  EXPECT_EQ(check_parse(json), should_pass) << "Failed case: " << name;
}

INSTANTIATE_TEST_SUITE_P(
    AllCases, Utf8Validation,
    ::testing::Values(
        // Valid UTF-8: accepted
        std::make_tuple("ASCII", R"({"key": "value"})", true),
        std::make_tuple("Valid 2-byte", "{\"key\": \"\xC2\xA2\"}", true),
        std::make_tuple("Valid 3-byte", "{\"key\": \"\xE2\x82\xAC\"}", true),
        std::make_tuple("Valid 4-byte", "{\"key\": \"\xF0\x9D\x84\x9E\"}", true),

        // Invalid UTF-8: also accepted (parsers do not validate UTF-8)
        std::make_tuple("Invalid start 0x80", "{\"key\": \"\x80\"}", true),
        std::make_tuple("Overlong 2-byte", "{\"key\": \"\xC0\xAF\"}", true),
        std::make_tuple("Overlong 3-byte", "{\"key\": \"\xE0\x80\xAF\"}", true),
        std::make_tuple("Missing continuation", "{\"key\": \"\xE2\x82\"}", true),
        std::make_tuple("Bad continuation", "{\"key\": \"\xE2\x02\xAC\"}", true),
        std::make_tuple("Surrogate high", "{\"key\": \"\xED\xA0\x80\"}", true),

        // Mixed valid UTF-8: accepted
        std::make_tuple("Mixed", "{\"key\": \"Hello \xF0\x9F\x8C\x8D World\"}", true),

        // Structural errors: fail regardless of byte content
        std::make_tuple("Missing close brace", "{\"key\": \"value\"", false),
        std::make_tuple("Missing close bracket", "[1, 2, 3", false),
        std::make_tuple("Empty input", "", false)),
    [](const testing::TestParamInfo<Utf8Validation::ParamType> &info) {
      std::string name = std::get<0>(info.param);
      std::replace(name.begin(), name.end(), ' ', '_');
      std::replace(name.begin(), name.end(), '-', '_');
      return name;
    });
