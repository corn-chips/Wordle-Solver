#include <iostream>
#include <future>
#include "wordfilter.hpp"
#include "threadpool.h"
#include <chrono>

#include "words.h"

#include "findnocommonchars.hpp"

/**
 * @brief Gets the original indexes of the smallest N numbers in a vector of floats.
 *
 * This function creates pairs of (value, original_index), sorts them based on value
 * in ascending order, and then extracts the indexes of the top N elements.
 *
 * @param vec The input vector of float numbers.
 * @param n The number of smallest elements to find indexes for.
 * @return A vector of integers, representing the original indexes of the smallest N numbers.
 *         The indexes are returned in the order of their corresponding values (smallest first).
 *         Returns an empty vector if n <= 0 or the input vector is empty.
 */
std::vector<int> get_smallest_n_indexes(const std::vector<float>& vec, int n) {
    // Handle edge cases: n is non-positive or vector is empty
    if (n <= 0 || vec.empty()) {
        return {};
    }

    // Ensure n doesn't exceed the vector's size
    n = std::min(n, static_cast<int>(vec.size()));

    // 1. Create a vector of pairs: {value, original_index}
    std::vector<std::pair<float, int>> indexed_values;
    indexed_values.reserve(vec.size()); // Pre-allocate memory for efficiency
    for (int i = 0; i < vec.size(); ++i) {
        indexed_values.push_back({ vec[i], i });
    }

    // 2. Partially sort the vector of pairs.
    //    We only need the top 'n' elements to be in their correct sorted positions.
    //    We sort in ascending order of float value. If values are equal,
    //    we can use the index for stable ordering (e.g., smaller index first).
    std::partial_sort(indexed_values.begin(),
        indexed_values.begin() + n, // Sort only up to this point
        indexed_values.end(),
        [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
            if (a.first != b.first) {
                return a.first < b.first; // <<--- CHANGED: Sort by value in ASCENDING order
            }
            // For tie-breaking (same value), prefer the smaller original index
            return a.second < b.second;
        });

    // 3. Extract the indexes from the first 'n' elements of the partially sorted vector
    std::vector<int> result_indexes;
    result_indexes.reserve(n); // Pre-allocate memory
    for (int i = 0; i < n; ++i) {
        result_indexes.push_back(indexed_values[i].second);
    }

    return result_indexes;
}

//having multiple threads access the same filtered words array might cause cache issues lol
float countAvgRemaining(const std::string& word, const std::vector<std::string>& wordlist) {
    float sum = 0.0f;

    for (int i = 0; i < wordlist.size(); i++) {
        WordFilter tempFilter{ wordlist[i], word };
        sum += (float)tempFilter.filterWordsCount(wordlist);
    }
    return sum / (float)wordlist.size();
}

struct CountAvgRemainingJobParams {
    char word[5];
    float* outputSum;
    size_t wordsInList;

    CountAvgRemainingJobParams() = default;
    CountAvgRemainingJobParams(const char* word, float* out, size_t wordlistSize) : outputSum{out}, wordsInList { wordlistSize } {
        std::memcpy(this->word, word, 5);
    }
};

void countAvgRemaining_pooljob(void* param, void* threadlocalstorage) {
    CountAvgRemainingJobParams& params = *((CountAvgRemainingJobParams*)param);

    float sum = 0.0f;

    for (int i = 0; i < params.wordsInList; i++) {
        WordFilter tempFilter{ (char*)&(((unsigned char*)threadlocalstorage)[i * 5]), params.word };
        sum += (float)tempFilter.optimized_filterWordsCount(((unsigned char*)threadlocalstorage), params.wordsInList, nullptr);
    }

    *params.outputSum = sum / (float)params.wordsInList;
}

void hyperavx_countAvgRemaining_pooljob(void* param, void* threadlocalstorage) {
    CountAvgRemainingJobParams& params = *((CountAvgRemainingJobParams*)param);

    float sum = 0.0f;

    for (int i = 0; i < params.wordsInList; i++) {
        WordFilter tempFilter{ (char*)&(((unsigned char*)threadlocalstorage)[i * 5]), params.word };
        sum += (float)tempFilter.hyperpacked_optimized_filterWordsCount(((unsigned char*)threadlocalstorage), params.wordsInList);
    }

    *params.outputSum = sum / (float)params.wordsInList;
}

void hyperavx512_countAvgRemaining_pooljob(void* param, void* threadlocalstorage) {
    CountAvgRemainingJobParams& params = *((CountAvgRemainingJobParams*)param);

    float sum = 0.0f;

    for (int i = 0; i < params.wordsInList; i++) {
        WordFilter tempFilter{ (char*)&(((unsigned char*)threadlocalstorage)[i * 5]), params.word };
        sum += (float)tempFilter.hyperpacked_optimized_filterWordsCount_AVX512(((unsigned char*)threadlocalstorage), params.wordsInList);
    }

    *params.outputSum = sum / (float)params.wordsInList;
}

int main()
{
    // 
    // NOTE BEFORE USE:
    // NASM and the Visual Studio extension VSNASM is required to compile this project.
    // Build in x64 mode ONLY, assembly uses the Windows x64 ABI
    // AVX and SSE4.2 required
    //

    // TODO: filters do not work when hyperavx mode is on
    // process words in simd parallel
    constexpr bool hyperavx_mode = true;
    constexpr bool avx512_mode = true;
    constexpr int packwidth = avx512_mode ? 64 : 32;

    std::array<char, 5> correct = { ' ', ' ', ' ', ' ', ' ' };
    std::array<std::vector<char>, 5> misplaced = {{
        {}, {}, {}, {}, {}
    }};
    std::string wrong = "";

    std::cout << "Using configuration:\n";
    std::cout << "Correct characters: ";
    for (int i = 0; i < correct.size(); i++) {
        std::cout << ((correct[i] == ' ') ? '_' : correct[i]);
    }
    std::cout << "\n";

    std::cout << "Misplaced characters: \n";
    for (int i = 0; i < misplaced.size(); i++) {
        std::cout << "At idx " << i << ": { ";
        for (int j = 0; j < misplaced[i].size(); j++) {
            std::cout << misplaced[i][j] << ", ";
        }
        std::cout << "}\n";
    }

    std::cout << "Wrong characters: " << wrong << std::endl;

    WordFilter filter { correct, misplaced, wrong };
    std::vector<std::string> filteredWords = filter.filterWords(validWords);

    std::cout << "All possible answers (Total " << filteredWords.size() << ") : \n";
    for (int i = 0; i < filteredWords.size(); i++) {
        std::cout << filteredWords[i] << "\n";
    }
    std::cout << std::endl;

    //stuff
    

    //set search words list
    const std::vector<std::string>& selectedSearchWords = allWords;

    //allocate task list
    std::vector<CountAvgRemainingJobParams> jobParams;
    jobParams.reserve(selectedSearchWords.size()); jobParams.resize(selectedSearchWords.size());

    std::vector<JobRecipe> sumJobs;
    sumJobs.reserve(selectedSearchWords.size()); sumJobs.resize(selectedSearchWords.size());

    std::vector<float> sumsOutputs;
    sumsOutputs.reserve(selectedSearchWords.size()); sumsOutputs.resize(selectedSearchWords.size());
    //fine line between undefined behavior
    //it might be

#if _DEBUG
    ThreadPool pool{ 1 };
#else 
    ThreadPool pool{ 12 };
#endif


    auto start = std::chrono::steady_clock::now();
    if (hyperavx_mode) {
        //build optimally packed wordlist
        // [w0c0], [w1c0], [w2c0], ... [w32c0]
        // [w0c1], [w1c1], [w2c1], ... [w32c1]
        // [w0c2], [w1c2], [w2c2], ... [w32c2]
        // .
        // .
        // .
        // [wNc5], [wNc2], [wNc5], ... [wNc5]

        size_t optimizedFilteredList_bytes = ((filteredWords.size() / packwidth) + 1) * packwidth * 5;
        char* optimizedFilteredList = new char[optimizedFilteredList_bytes];

        //zero the buffer
        std::memset(optimizedFilteredList, 0, optimizedFilteredList_bytes);

        for (int i = 0; i < filteredWords.size(); i += packwidth) {
            for (int j = 0; j < 5; j++) {
                for (int k = 0; k < packwidth; k++) {
                    if ( (i + k) < filteredWords.size() )
                        optimizedFilteredList[(i * 5) + (j * packwidth) + (k)] = filteredWords[i+k].c_str()[j];
                }
            }
        }
        std::cout << "built packed filter list" << std::endl;
        
        pool.allocateThreadLocalStorage(optimizedFilteredList, optimizedFilteredList_bytes, packwidth);
        //can deallocate the packed list now
        delete[] optimizedFilteredList;

        //build jobs
        for (int i = 0; i < selectedSearchWords.size(); i++) {
            jobParams[i] = CountAvgRemainingJobParams(selectedSearchWords[i].c_str(), &(sumsOutputs.data()[i]), filteredWords.size());
            if (avx512_mode)
                sumJobs[i] = JobRecipe(&(jobParams.data()[i]), &hyperavx512_countAvgRemaining_pooljob);
            else
                sumJobs[i] = JobRecipe(&(jobParams.data()[i]), &hyperavx_countAvgRemaining_pooljob);
        }
    }
    else {
        //allocate thread local storage
        size_t optimizedFilteredList_bytes = filteredWords.size() * 5;
        char* optimizedFilteredList = new char[optimizedFilteredList_bytes];
        for (int i = 0; i < filteredWords.size(); i++) {
            std::memcpy(&(optimizedFilteredList[i * 5]), filteredWords[i].c_str(), 5);
        }

        //align to cache line?
        pool.allocateThreadLocalStorage(optimizedFilteredList, optimizedFilteredList_bytes, 64); // 64 for a placeholder
        delete[] optimizedFilteredList;

        //build jobs
        for (int i = 0; i < selectedSearchWords.size(); i++) {
            jobParams[i] = CountAvgRemainingJobParams(selectedSearchWords[i].c_str(), &(sumsOutputs.data()[i]), filteredWords.size());
            sumJobs[i] = JobRecipe(&(jobParams.data()[i]), &countAvgRemaining_pooljob);
        }
    }
    pool.QueueBatchTask(sumJobs.data(), sumJobs.size());
    pool.WaitCompletion();
    auto end = std::chrono::steady_clock::now();
    

    const int numBestResponses = 50;
    std::vector<int> largestSumsIdxs = get_smallest_n_indexes(sumsOutputs, numBestResponses);

    std::cout << "Best " << numBestResponses << " responses: \n";
    for (int i = 0; i < largestSumsIdxs.size(); i++) {
        std::cout << selectedSearchWords[largestSumsIdxs[i]] << "\t" << sumsOutputs[largestSumsIdxs[i]] << "\n";
    }

    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "." << (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() % 1000) << "s\n";
}