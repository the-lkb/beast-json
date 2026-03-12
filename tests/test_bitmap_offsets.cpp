#include <qbuem_json/qbuem_json.hpp>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

TEST(Bitmap, TwitterSpecificOffsets) {
  std::ifstream f("twitter.json");
  ASSERT_TRUE(f.is_open()) << "Failed to open twitter.json";
  std::string twitter((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  f.close();

  // Create large JSON same as benchmark
  std::string large;
  large.reserve(twitter.size() * 200 + 202);
  large += "[";
  for (int i = 0; i < 200; ++i) {
    large += twitter;
    if (i < 199)
      large += ",";
  }
  large += "]";

  // Expected first comma location
  size_t expected_comma = 1 + twitter.size(); // After '[' + twitter1

  // Build bitmap
  qbuem::json::BitmapIndex idx;
  qbuem::json::simd::fill_bitmap(large.c_str(), large.size(), idx);

  // Check offset 631124 that C++ reported as comma (suspicious check from
  // legacy test)
  size_t suspicious_offset = 631124;
  if (suspicious_offset < large.size()) {
    size_t block_idx = suspicious_offset / 64;
    size_t bit_pos = suspicious_offset % 64;

    if (block_idx < idx.structural_bits.size()) {
      uint64_t block = idx.structural_bits[block_idx];
      bool is_set = (block & (1ULL << bit_pos)) != 0;

      if (is_set) {
        bool is_structural = (large[suspicious_offset] == ',' ||
                              large[suspicious_offset] == '{' ||
                              large[suspicious_offset] == '}' ||
                              large[suspicious_offset] == '[' ||
                              large[suspicious_offset] == ']');
        EXPECT_TRUE(is_structural)
            << "Bitmap marks non-structural character at " << suspicious_offset;
      }
    }
  }

  // Check expected comma location
  size_t block_idx = expected_comma / 64;
  size_t bit_pos = expected_comma % 64;

  if (block_idx < idx.structural_bits.size()) {
    uint64_t block = idx.structural_bits[block_idx];
    bool is_set = (block & (1ULL << bit_pos)) != 0;
    EXPECT_TRUE(is_set) << "Bitmap MISSING expected comma at "
                        << expected_comma;
  }
}
