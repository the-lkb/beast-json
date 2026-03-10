#pragma once
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__APPLE__)
#  include <mach/mach.h>
#else
#  include <unistd.h>
#endif

namespace bench {

// Current (not peak) RSS in KB.
// Each library sub-process reads this before and after one cold parse;
// the delta is the heap committed to hold the parsed document.
inline size_t get_current_rss_kb() {
#if defined(__APPLE__)
  struct mach_task_basic_info info;
  mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                (task_info_t)&info, &n) == KERN_SUCCESS)
    return static_cast<size_t>(info.resident_size / 1024);
  return 0;
#else
  std::ifstream f("/proc/self/statm");
  if (!f) return 0;
  long dummy, rss_pages;
  f >> dummy >> rss_pages;
  long page_kb = sysconf(_SC_PAGE_SIZE) / 1024;
  return static_cast<size_t>(rss_pages * page_kb);
#endif
}

// Read entire file into string
inline std::string read_file(const char *path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f.is_open()) {
    throw std::runtime_error(std::string("Failed to open file: ") + path);
  }
  std::streamsize size = f.tellg();
  f.seekg(0, std::ios::beg);
  std::string buffer(size, '\0');
  if (!f.read(&buffer[0], size)) {
    throw std::runtime_error(std::string("Failed to read file: ") + path);
  }
  return buffer;
}

// High precision timer
class Timer {
public:
  using clock = std::chrono::high_resolution_clock;
  using time_point = clock::time_point;

  void start() { start_ = clock::now(); }

  double elapsed_ns() const {
    auto end = clock::now();
    return std::chrono::duration<double, std::nano>(end - start_).count();
  }

  double elapsed_us() const { return elapsed_ns() / 1000.0; }
  double elapsed_ms() const { return elapsed_ns() / 1000000.0; }

private:
  time_point start_;
};

// Benchmark result
struct Result {
  std::string library;
  double parse_time_ns;
  double serialize_time_ns;
  bool correctness_check;
  size_t alloc_kb; // RSS delta during a single cold parse (document live)

  void print() const {
    std::cout << std::left << std::setw(15) << library
              << " | Parse: " << std::right << std::setw(9) << std::fixed
              << std::setprecision(2) << (parse_time_ns / 1000.0) << " \xce\xbcs"
              << " | Serialize: " << std::setw(9) << std::fixed
              << std::setprecision(2) << (serialize_time_ns / 1000.0) << " \xce\xbcs"
              << " | Alloc: " << std::setw(6) << alloc_kb << " KB"
              << " | \xe2\x9c\x93 " << (correctness_check ? "PASS" : "FAIL") << "\n";
  }
};

// Print header
inline void print_header(const std::string &benchmark_name) {
  std::cout << "\n=== " << benchmark_name << " ===\n";
  std::cout << std::string(80, '-') << "\n";
}

// Print table header
inline void print_table_header() {
  std::cout << std::left << std::setw(20) << "Library" << " | Timings\n";
  std::cout << std::string(80, '-') << "\n";
}

} // namespace bench
