#pragma once

#include <vector>
#include <string>
#include <array>


class WordFilter {

public:
    
    // Helper: Precompute broadcasted filter data (can be done in constructor or lazily)
    struct SIMD_FilterData {
        // For correct letters
        std::array<__m256i, 5> ymm_correct_chars;
        std::array<__m256i, 5> ymm_correct_is_space_mask; // 0xFF if correct[j] is ' ', 0x00 otherwise

        // For misplaced letters
        // For each position j, a vector of broadcasted misplaced chars
        std::array<std::vector<__m256i>, 5> ymm_misplaced_chars_bcast;

        // For wrong letters
        std::vector<__m256i> ymm_wrong_chars_bcast;

        __m256i ymm_all_ones;

        SIMD_FilterData(const WordFilter& wf) {
            ymm_all_ones = _mm256_set1_epi8(0xFF); // or _mm256_cmpeq_epi8(_mm256_setzero_si256(), _mm256_setzero_si256())

            __m256i space_char_bcast = _mm256_set1_epi8(' ');
            for (int j = 0; j < 5; ++j) {
                ymm_correct_chars[j] = _mm256_set1_epi8(wf.correct[j]);
                ymm_correct_is_space_mask[j] = _mm256_cmpeq_epi8(ymm_correct_chars[j], space_char_bcast);
            }

            for (int j = 0; j < 5; ++j) {
                for (char m_char : wf.misplaced[j]) {
                    ymm_misplaced_chars_bcast[j].push_back(_mm256_set1_epi8(m_char));
                }
            }

            for (char w_char : wf.wrong) {
                ymm_wrong_chars_bcast.push_back(_mm256_set1_epi8(w_char));
            }
        }
        SIMD_FilterData() = default;
    };

    struct SIMD_FilterData_AVX512 {
        std::array<__m512i, 5> zmm_correct_chars;
        std::array<__mmask64, 5> k_correct_is_space; // Stores k-mask directly

        std::array<std::vector<__m512i>, 5> zmm_misplaced_chars_bcast;
        std::vector<__m512i> zmm_wrong_chars_bcast;
        // No need for zmm_all_ones if we use k-mask operations mainly, but good for blend/xor

        SIMD_FilterData_AVX512() = default;

        SIMD_FilterData_AVX512(const WordFilter& wf) {
            __m512i space_char_bcast = _mm512_set1_epi8(' ');
            for (int j = 0; j < 5; ++j) {
                zmm_correct_chars[j] = _mm512_set1_epi8(wf.correct[j]);
                k_correct_is_space[j] = _mm512_cmpeq_epi8_mask(zmm_correct_chars[j], space_char_bcast);
            }

            for (int j = 0; j < 5; ++j) {
                for (char m_char : wf.misplaced[j]) {
                    zmm_misplaced_chars_bcast[j].push_back(_mm512_set1_epi8(m_char));
                }
            }

            for (char w_char : wf.wrong) {
                zmm_wrong_chars_bcast.push_back(_mm512_set1_epi8(w_char));
            }
        }
    };

    std::array<char, 5> correct;
    std::array<std::vector<char>, 5> misplaced;
    std::vector<char> wrong;

    WordFilter() : correct{ ' ', ' ', ' ', ' ', ' ' }, misplaced{} {};

    WordFilter(
        const std::array<char, 5>& correct,
        const std::array<std::vector<char>, 5>& misplaced,
        const std::string& wrong
    );
    explicit WordFilter(
        const std::vector<WordFilter>& filters
    );
    //after guess?
    WordFilter(
        const std::string& solution, const std::string& guess
    );
    WordFilter(
        const char* solution, const char* guess
    );

    std::vector<std::string> filterWords(const std::vector<std::string>& wordlist);
    int filterWordsCount(const std::vector<std::string>& wordlist);
    int optimized_filterWordsCount(const unsigned char* wordlist, size_t words, int* scratchmem);

    bool simd_data_initialized = false;
    SIMD_FilterData filter_simd_data;

    SIMD_FilterData_AVX512 filter_simd_data_avx512; // Member instance
    bool simd_data_avx512_initialized = false;

    int hyperpacked_optimized_filterWordsCount(const unsigned char* hyperpacked_wordlist, size_t words);
    int hyperpacked_optimized_filterWordsCount_AVX512(
        const unsigned char* hyperpacked_wordlist,
        size_t words
    );
};

