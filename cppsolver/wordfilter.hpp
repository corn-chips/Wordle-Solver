#pragma once

#include <vector>
#include <string>
#include <array>


class WordFilter {

public:
    
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
};