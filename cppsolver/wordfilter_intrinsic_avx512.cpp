#include "wordfilter.hpp"
#include "words.h"

int WordFilter::hyperpacked_optimized_filterWordsCount_AVX512(
    const unsigned char* hyperpacked_wordlist,
    size_t words
) {
    // Lazy initialization of AVX-512 specific filter data
    if (!simd_data_avx512_initialized) {
        this->filter_simd_data_avx512 = SIMD_FilterData_AVX512(*this);
        this->simd_data_avx512_initialized = true;
    }

    int count = 0;
    const int BATCH_SIZE_AVX512 = 64; // AVX-512 ZMM register processes 64 bytes
    std::array<__m512i, 5> zmm_word_char_cols;

    size_t num_batches = (words + BATCH_SIZE_AVX512 - 1) / BATCH_SIZE_AVX512;

    // __mmask64 k_all_ones = _mm512_kxnor(0, 0); // All 64 bits set in k-mask
    // Alternative for k_all_ones, if needed explicitly for some ops, though often implicit (e.g. non-masked ops)
    // Or more simply: if you start with a k-mask and AND results into it:
    __mmask64 k_all_pass = (__mmask64)-1LL; // All 64 bits set

    for (size_t batch_idx = 0; batch_idx < num_batches; ++batch_idx) {
        size_t soa_batch_base_offset = batch_idx * 5 * BATCH_SIZE_AVX512;

        // 1. Load data for the current batch
        for (int j = 0; j < 5; ++j) {
            // Use _mm512_loadu_si512 for unaligned loads, or _mm512_load_si512 if aligned
            zmm_word_char_cols[j] = _mm512_loadu_si512(
                reinterpret_cast<const void*>(&hyperpacked_wordlist[soa_batch_base_offset + j * BATCH_SIZE_AVX512])
            );
        }

        __mmask64 k_batch_pass_mask = k_all_pass; // Start with all words in batch passing

        // 2. Correct Letters
        for (int j = 0; j < 5; ++j) {
            // Compare word char with correct char, result is a k-mask
            __mmask64 k_char_match = _mm512_cmpeq_epi8_mask(
                zmm_word_char_cols[j],
                filter_simd_data_avx512.zmm_correct_chars[j]
            );
            // Pass if (chars match) OR (filter's correct char for this position was a space)
            __mmask64 k_pass_this_char_correct = _mm512_kor(
                k_char_match,
                filter_simd_data_avx512.k_correct_is_space[j] // Using precomputed k-mask
            );
            k_batch_pass_mask = _mm512_kand(k_batch_pass_mask, k_pass_this_char_correct);
        }

        // OPTIMIZATION: Early exit if all words in batch failed
        if (k_batch_pass_mask == 0) { // k-mask is 0 if no bits are set
            continue;
        }

        // OPTIMIZATION: move up declarations for goto
        uint64_t lanes_mask_u64; // For popcnt on 64-bit mask
        size_t words_in_this_batch_actual = BATCH_SIZE_AVX512;


        // 3. Misplaced Letters
        for (int j = 0; j < 5; ++j) {
            if (filter_simd_data_avx512.zmm_misplaced_chars_bcast[j].empty()) continue;

            for (const __m512i& zmm_m_bcast : filter_simd_data_avx512.zmm_misplaced_chars_bcast[j]) {
                // Cond1: char m NOT at word[j]
                __mmask64 k_eq_at_j = _mm512_cmpeq_epi8_mask(zmm_word_char_cols[j], zmm_m_bcast);
                __mmask64 k_neq_at_j = _mm512_knot(k_eq_at_j); // k_neq_at_j = _mm512_kxor(k_eq_at_j, k_all_pass);

                // Cond2: char m MUST be present somewhere
                __mmask64 k_present_in_word = 0; // Start with no bits set
                for (int k = 0; k < 5; ++k) {
                    __mmask64 k_eq_at_k = _mm512_cmpeq_epi8_mask(zmm_word_char_cols[k], zmm_m_bcast);
                    k_present_in_word = _mm512_kor(k_present_in_word, k_eq_at_k);
                }

                __mmask64 k_misplaced_pass_for_m_at_j = _mm512_kand(k_neq_at_j, k_present_in_word);
                k_batch_pass_mask = _mm512_kand(k_batch_pass_mask, k_misplaced_pass_for_m_at_j);
            }

            if (k_batch_pass_mask == 0) {
                goto next_batch_label_avx512;
            }
        }

        // 4. Wrong Letters
        if (!filter_simd_data_avx512.zmm_wrong_chars_bcast.empty()) { // Check if there are any wrong letters to process
            for (const __m512i& zmm_w_bcast : filter_simd_data_avx512.zmm_wrong_chars_bcast) {
                __mmask64 k_is_w_in_word = 0; // Start with no bits set
                for (int k = 0; k < 5; ++k) {
                    __mmask64 k_eq_at_k = _mm512_cmpeq_epi8_mask(zmm_word_char_cols[k], zmm_w_bcast);
                    k_is_w_in_word = _mm512_kor(k_is_w_in_word, k_eq_at_k);
                }
                __mmask64 k_not_w_in_word = _mm512_knot(k_is_w_in_word);
                k_batch_pass_mask = _mm512_kand(k_batch_pass_mask, k_not_w_in_word);

                if (k_batch_pass_mask == 0) {
                    goto next_batch_label_avx512;
                }
            }
        }


        // 5. Count results from the batch
        // k_batch_pass_mask is already the 64-bit integer we need for popcount.
        lanes_mask_u64 = k_batch_pass_mask;

        if (batch_idx == num_batches - 1) {
            words_in_this_batch_actual = words - (batch_idx * BATCH_SIZE_AVX512);
        }

        if (words_in_this_batch_actual < BATCH_SIZE_AVX512) {
            // Create a mask to clear upper bits for partial batch
            uint64_t partial_batch_bits_mask = (1ULL << words_in_this_batch_actual) - 1;
            lanes_mask_u64 &= partial_batch_bits_mask;
        }
#ifdef _MSC_VER
        count += __popcnt64(lanes_mask_u64);
#else // GCC/Clang
        count += _popcnt64(lanes_mask_u64); // or __builtin_popcountll(lanes_mask_u64)
#endif


    next_batch_label_avx512:;
        // __noop; // MSVC specific, not needed for GCC/Clang if label is used
    }

    return count;
}