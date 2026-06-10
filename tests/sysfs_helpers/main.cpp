// test_sysfs_helpers — regression for #43.
//
// Verifies that hal::sysfs::readSysfsLine and hal::sysfs::readSysfsInt
// (the new header-only helpers in lib_base.h) behave correctly for
// the cases a HAL module would actually exercise: real on-disk file,
// non-existent path, empty file, non-numeric content.

#include "lib_base.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

int g_failures = 0;

#define EXPECT_EQ(actual, expected, msg) do {                                \
    auto _a = (actual);                                                     \
    auto _e = (expected);                                                   \
    if (_a != _e) {                                                         \
        std::fprintf(stderr,                                                 \
            "FAIL %s:%d  %s  (got %lld, expected %lld)\n",                  \
            __FILE__, __LINE__, msg,                                        \
            static_cast<long long>(_a), static_cast<long long>(_e));        \
        ++g_failures;                                                       \
    } else {                                                                \
        std::fprintf(stderr, "ok   %s\n", msg);                             \
    }                                                                       \
} while (0)

#define EXPECT_STR_EQ(actual, expected, msg) do {                           \
    std::string _a = (actual);                                              \
    std::string _e = (expected);                                            \
    if (_a != _e) {                                                         \
        std::fprintf(stderr,                                                 \
            "FAIL %s:%d  %s  (got \"%s\", expected \"%s\")\n",              \
            __FILE__, __LINE__, msg, _a.c_str(), _e.c_str());               \
        ++g_failures;                                                       \
    } else {                                                                \
        std::fprintf(stderr, "ok   %s\n", msg);                             \
    }                                                                       \
} while (0)

}  // namespace

int main()
{
    // Write a real on-disk file with a known first line.
    const std::string real_path = "/tmp/test_sysfs_helpers_int.txt";
    {
        std::ofstream f(real_path);
        f << "42\n";
    }

    // ---- readSysfsLine ----
    EXPECT_STR_EQ(hal::sysfs::readSysfsLine(real_path), "42",
                  "readSysfsLine on an existing file returns the first line");
    EXPECT_STR_EQ(hal::sysfs::readSysfsLine("/tmp/this_path_does_not_exist"),
                  std::string(),
                  "readSysfsLine on a missing path returns empty string");

    // ---- readSysfsInt ----
    EXPECT_EQ(hal::sysfs::readSysfsInt(real_path), 42LL,
              "readSysfsInt parses a numeric value");
    EXPECT_EQ(hal::sysfs::readSysfsInt("/tmp/this_path_does_not_exist"), -1LL,
              "readSysfsInt on a missing path returns -1");

    // Non-numeric content: also -1.
    const std::string non_numeric_path = "/tmp/test_sysfs_helpers_non_numeric.txt";
    {
        std::ofstream f(non_numeric_path);
        f << "not a number\n";
    }
    EXPECT_EQ(hal::sysfs::readSysfsInt(non_numeric_path), -1LL,
              "readSysfsInt on non-numeric content returns -1");

    // Empty file: also -1.
    const std::string empty_path = "/tmp/test_sysfs_helpers_empty.txt";
    {
        std::ofstream f(empty_path);
        // write nothing
    }
    EXPECT_EQ(hal::sysfs::readSysfsInt(empty_path), -1LL,
              "readSysfsInt on an empty file returns -1");

    // Negative numbers (sysfs exposes -1 as a "disabled" marker for
    // many thermal/clock knobs). The helper should return them as-is.
    const std::string negative_path = "/tmp/test_sysfs_helpers_negative.txt";
    {
        std::ofstream f(negative_path);
        f << "-7\n";
    }
    EXPECT_EQ(hal::sysfs::readSysfsInt(negative_path), -7LL,
              "readSysfsInt preserves a negative numeric value");

    // Cleanup
    std::remove(real_path.c_str());
    std::remove(non_numeric_path.c_str());
    std::remove(empty_path.c_str());
    std::remove(negative_path.c_str());

    if (g_failures != 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "\nall checks passed\n");
    return 0;
}
