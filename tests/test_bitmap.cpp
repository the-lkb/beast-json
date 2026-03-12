#include <qbuem_json/qbuem_json.hpp>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(Bitmap, LargeJsonManualVerify) {
  // Read test JSON
  std::ifstream f("test_large.json", std::ios::binary | std::ios::ate);
  ASSERT_TRUE(f.is_open()) << "Failed to open test_large.json";
  std::streamsize len = f.tellg();
  f.seekg(0, std::ios::beg);

  std::vector<char> buffer(len);
  if (!f.read(buffer.data(), len)) {
    FAIL() << "Failed to read file";
  }

  // Manually count structural chars
  int manual_count = 0;
  for (size_t i = 0; i < (size_t)len; i++) {
    if (strchr("[]{},:", buffer[i])) {
      manual_count++;
    }
  }

  // Build bitmap
  qbuem::json::BitmapIndex idx;
  qbuem::json::simd::fill_bitmap(buffer.data(), len, idx);

  // Count bits in bitmap
  int bitmap_count = 0;
  for (const auto &block : idx.structural_bits) {
    bitmap_count += __builtin_popcountll(block);
  }

  EXPECT_EQ(bitmap_count, manual_count) << "Bitmap count mismatch! Ratio: "
                                        << (double)bitmap_count / manual_count;
}
