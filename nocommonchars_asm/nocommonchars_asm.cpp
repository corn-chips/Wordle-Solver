#include <iostream>
#include <string>
#include <vector>
#include <chrono> // For potential timing, though not explicitly requested for comparison here

// Declare the assembly function
// It takes: haystack_ptr, haystack_len, needles_ptr, needles_len
// Returns: 1 if haystack does NOT contain any char from needles, 0 otherwise.
extern "C" int find_no_common_chars_sse42(const char* haystack, size_t haystack_len,
    const char* needles, size_t needles_len);

// Naive C++ approach using std::string::find_first_of
// Returns true if haystack does NOT contain any char from needles.
bool naive_find_no_common_chars(const std::string& haystack, const std::string& needles) {
    if (needles.empty()) { // If needles is empty, haystack cannot contain any char from it.
        return true;
    }
    if (haystack.empty()) { // If haystack is empty, it contains no chars (from needles or otherwise).
        return true;
    }
    // find_first_of returns string::npos if no character from 'needles' is found in 'haystack'.
    return haystack.find_first_of(needles) == std::string::npos;
}

// Slightly more verbose naive approach (closer to char-by-char find)
// Returns true if haystack does NOT contain any char from needles.
bool verbose_naive_find_no_common_chars(const std::string& haystack, const std::string& needles) {
    if (needles.empty()) return true;
    if (haystack.empty()) return true;

    for (char needle_char : needles) {
        if (haystack.find(needle_char) != std::string::npos) {
            return false; // Found a common character
        }
    }
    return true; // No common characters found
}


struct TestCase {
    std::string name;
    std::string haystack;
    std::string needles;
    bool expected_no_common; // True if haystack should NOT contain any char from needles
};

int main() {
    //find_no_common_chars_sse42(nullptr, 0, nullptr, 0);

    std::vector<TestCase> test_cases = {
        {"Empty_Haystack", "", "abc", true},
        {"Empty_Needles", "abc", "", true},
        {"Both_Empty", "", "", true},
        {"No_Common_Simple", "abcdef", "xyz", true},
        {"Common_Simple_1", "apple", "orange", false}, // 'a', 'e' are common
        {"Common_Simple_2", "hello world", "low", false}, // 'l', 'o', 'w'
        {"All_Common", "abc", "abc", false},
        {"Needles_Subset_Haystack", "supercalifragilisticexpialidocious", "cali", false},
        {"Haystack_Subset_Needles", "cali", "supercalifragilisticexpialidocious", false},
        {"No_Common_Long_Haystack", std::string(100, 'a') + std::string(100, 'b'), "xyz", true},
        {"Common_Long_Haystack", std::string(100, 'a') + "x" + std::string(100, 'b'), "xyz", false}, // 'x'
        {"No_Common_Long_Needles", "abc", std::string(100, 'x') + std::string(100, 'y'), true},
        {"Common_Long_Needles", "axbyc", std::string(100, 'x') + "z" + std::string(100, 'y'), false}, // 'x'
        {"Boundary_15_15_No_Common", "0123456789abcde", "fghijklmnopqrst", true}, // len 15, len 15
        {"Boundary_15_15_Common", "0123456789abcde", "efghijklmnopq", false},    // len 15, len 15, 'e' common
        {"Boundary_16_16_No_Common", "0123456789abcdef", "ghijklmnopqrstuv", true}, // len 16, len 16
        {"Boundary_16_16_Common", "0123456789abcdef", "fghijklmnopqrstuvw", false},    // len 16, len 16, 'f' common
        {"Boundary_17_17_No_Common", "0123456789abcdefg", "hijklmnopqrstuvwx", true}, // len 17, len 17
        {"Boundary_17_17_Common", "0123456789abcdefg", "ghijklmnopqrstuvwxy", false},    // len 17, len 17, 'g' common
        {"Mixed_Case_Common", "Apple", "pear", false}, // 'p', 'e', 'a', 'r'
        {"Mixed_Case_No_Common", "Apple", "XYZ", true},
        {"Special_Chars_Common", "!@#$", "#%^", false}, // '#'
        {"Special_Chars_No_Common", "!@#$", "%^&", true},
        {"Longer_Needles_Than_Haystack_Common", "short", "a_very_long_string_with_s", false},
        {"Longer_Needles_Than_Haystack_No_Common", "short", "a_very_long_string_no_match", false},
        {"Identical_Strings_Long", std::string(50, 'k'), std::string(50, 'k'), false},
        {"One_Char_Haystack_Match", "k", "abcdefghijk", false},
        {"One_Char_Haystack_No_Match", "z", "abcdefghijk", true},
        {"One_Char_Needles_Match", "abcdefghijk", "k", false},
        {"One_Char_Needles_No_Match", "abcdefghijk", "z", true},
    };

    int tests_passed = 0;
    int tests_failed = 0;

    for (const auto& tc : test_cases) {
        std::cout << "Running test: " << tc.name << std::endl;
        std::cout << "  Haystack: \"" << tc.haystack << "\" (len=" << tc.haystack.length() << ")" << std::endl;
        std::cout << "  Needles:  \"" << tc.needles << "\" (len=" << tc.needles.length() << ")" << std::endl;
        std::cout << "  Expected (no common chars): " << (tc.expected_no_common ? "true" : "false") << std::endl;

        // Test Naive C++ (using find_first_of)
        bool naive_result = naive_find_no_common_chars(tc.haystack, tc.needles);
        std::cout << "  Naive C++ (find_first_of) result: " << (naive_result ? "true" : "false");
        if (naive_result == tc.expected_no_common) {
            std::cout << " [CORRECT]" << std::endl;
        }
        else {
            std::cout << " [INCORRECT]" << std::endl;
        }

        // Test Verbose Naive C++ (char by char find)
        bool verbose_naive_result = verbose_naive_find_no_common_chars(tc.haystack, tc.needles);
        std::cout << "  Verbose Naive C++ (find) result: " << (verbose_naive_result ? "true" : "false");
        if (verbose_naive_result == tc.expected_no_common) {
            std::cout << " [CORRECT]" << std::endl;
        }
        else {
            std::cout << " [INCORRECT]" << std::endl;
        }

        // Test Assembly
        if (tc.name == "No_Common_Simple") {
            int a = 1 + 2;
        }

        int asm_raw_result = find_no_common_chars_sse42(
            tc.haystack.c_str(), tc.haystack.length(),
            tc.needles.c_str(), tc.needles.length()
        );
        bool asm_result = (asm_raw_result == 1); // ASM returns 1 for true (no common)
        std::cout << "  Assembly (SSE4.2) result:       " << (asm_result ? "true" : "false");

        if (asm_result == tc.expected_no_common) {
            std::cout << " [CORRECT]" << std::endl;
            tests_passed++;
        }
        else {
            std::cout << " [INCORRECT]" << std::endl;
            tests_failed++;
        }
        std::cout << "----------------------------------------" << std::endl;
    }

    std::cout << "\nSummary:" << std::endl;
    std::cout << "Tests Passed (vs expected): " << tests_passed << std::endl;
    std::cout << "Tests Failed (vs expected): " << tests_failed << std::endl;

    if (tests_failed == 0) {
        std::cout << "All tests passed successfully against expected values!" << std::endl;
    }
    else {
        std::cout << "Some tests failed!" << std::endl;
    }

    return tests_failed == 0 ? 0 : 1;
}