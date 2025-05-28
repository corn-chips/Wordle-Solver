#pragma once

//returns true if no needles were found in the haystack
extern "C" int find_no_common_chars_sse42(const char* haystack, size_t haystack_len,
    const char* needles, size_t needles_len);

//5 char haystack vs n size needles of max 16 char
//returns true if needle was found (inverse of the old slower one)
extern "C" int fast_find_no_common_chars_sse42(const char* haystack, const char* needles);

//true if 5 char word passes
extern "C" int masked_greenletter_compare(const char* correct, const unsigned char* word);

//check misplaced letters
//true if passed
extern "C" int check_misplaced_letters(const unsigned char* word, char* misplacedList, char* misplacedListSizes);