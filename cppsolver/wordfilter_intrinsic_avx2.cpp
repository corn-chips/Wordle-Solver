#include "wordfilter.hpp"
#include "words.h"

int WordFilter::hyperpacked_optimized_filterWordsCount(
    const unsigned char* hyperpacked_wordlist, 
    size_t words
) {
    if (!simd_data_initialized) {
        this->filter_simd_data = SIMD_FilterData(*this);
    }
    // Assume SIMD_FilterData is already prepared (as in previous answer)

    int count = 0;
    const int BATCH_SIZE = 32;
    std::array<__m256i, 5> ymm_word_char_cols;

    size_t num_batches = (words + BATCH_SIZE - 1) / BATCH_SIZE;

    for (size_t batch_idx = 0; batch_idx < num_batches; ++batch_idx) {
        size_t soa_batch_base_offset = batch_idx * 5 * BATCH_SIZE;

        // 1. Load data for the current batch (MUCH FASTER NOW)
        for (int j = 0; j < 5; ++j) {
            ymm_word_char_cols[j] = _mm256_load_si256(
                reinterpret_cast<const __m256i*>(&hyperpacked_wordlist[soa_batch_base_offset + j * BATCH_SIZE])
            );
        }

        __m256i ymm_batch_pass_mask = filter_simd_data.ymm_all_ones;

        // 2. Correct Letters (same logic as before)
        for (int j = 0; j < 5; ++j) {
            __m256i char_match = _mm256_cmpeq_epi8(ymm_word_char_cols[j], filter_simd_data.ymm_correct_chars[j]);
            __m256i pass_this_char_correct = _mm256_or_si256(char_match, filter_simd_data.ymm_correct_is_space_mask[j]);
            ymm_batch_pass_mask = _mm256_and_si256(ymm_batch_pass_mask, pass_this_char_correct);
        }

        // OPTIMIZATION: Early exit if all words in batch failed
        // _mm256_testz_si256(a, b) returns 1 if (a & b) is all zeros.
        // We want to check if ymm_batch_pass_mask itself is all zeros.
        // A common way: test ymm_batch_pass_mask against itself. If it's zero, (mask & mask) is zero.
        // Or, more directly, compare its movemask to 0.
        if (_mm256_movemask_epi8(ymm_batch_pass_mask) == 0) {
            continue; // Skip to the next batch
        }

        // OPTIMIZATION: move up declarations to use goto statements
        unsigned int lanes_mask;
        size_t words_in_this_batch_actual = BATCH_SIZE;

        // 3. Misplaced Letters (same logic as before)
        for (int j = 0; j < 5; ++j) {
            if (filter_simd_data.ymm_misplaced_chars_bcast[j].empty()) continue;

            for (const __m256i& ymm_m_bcast : filter_simd_data.ymm_misplaced_chars_bcast[j]) {
                __m256i ymm_eq_at_j = _mm256_cmpeq_epi8(ymm_word_char_cols[j], ymm_m_bcast);
                __m256i ymm_neq_at_j = _mm256_xor_si256(ymm_eq_at_j, filter_simd_data.ymm_all_ones);
                __m256i ymm_present_in_word = _mm256_setzero_si256();
                for (int k = 0; k < 5; ++k) {
                    __m256i ymm_eq_at_k = _mm256_cmpeq_epi8(ymm_word_char_cols[k], ymm_m_bcast);
                    ymm_present_in_word = _mm256_or_si256(ymm_present_in_word, ymm_eq_at_k);
                }
                __m256i ymm_misplaced_pass_for_m_at_j = _mm256_and_si256(ymm_neq_at_j, ymm_present_in_word);
                ymm_batch_pass_mask = _mm256_and_si256(ymm_batch_pass_mask, ymm_misplaced_pass_for_m_at_j);
            }

            // OPTIMIZATION: Early exit after checking all misplaced for position j
            if (_mm256_movemask_epi8(ymm_batch_pass_mask) == 0) {
                goto next_batch_label; // Using goto to break out of nested loops
            }
        }

        // 4. Wrong Letters (same logic as before)
        for (const __m256i& ymm_w_bcast : filter_simd_data.ymm_wrong_chars_bcast) {
            __m256i ymm_is_w_in_word = _mm256_setzero_si256();
            for (int k = 0; k < 5; ++k) {
                __m256i ymm_eq_at_k = _mm256_cmpeq_epi8(ymm_word_char_cols[k], ymm_w_bcast);
                ymm_is_w_in_word = _mm256_or_si256(ymm_is_w_in_word, ymm_eq_at_k);
            }
            __m256i ymm_not_w_in_word = _mm256_xor_si256(ymm_is_w_in_word, filter_simd_data.ymm_all_ones);
            ymm_batch_pass_mask = _mm256_and_si256(ymm_batch_pass_mask, ymm_not_w_in_word);

            // OPTIMIZATION: Early exit after checking each wrong letter
            if (_mm256_movemask_epi8(ymm_batch_pass_mask) == 0) {
                goto next_batch_label;
            }
        }

        // 5. Count results from the batch
        lanes_mask = _mm256_movemask_epi8(ymm_batch_pass_mask);

        // Correctly handle the last batch, which might be smaller than BATCH_SIZE
        // size_t words_in_this_batch_actual = BATCH_SIZE;
        if (batch_idx == num_batches - 1) { // Last batch
            words_in_this_batch_actual = words - (batch_idx * BATCH_SIZE);
        }

        if (words_in_this_batch_actual < BATCH_SIZE) {
            lanes_mask &= (1U << words_in_this_batch_actual) - 1;
        }
        count += _mm_popcnt_u32(lanes_mask);

        next_batch_label:
        __noop;
    }

    return count;
}