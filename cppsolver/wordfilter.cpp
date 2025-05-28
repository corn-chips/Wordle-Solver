#include "wordfilter.hpp"
#include "words.h"
#include "findnocommonchars.hpp"

WordFilter::WordFilter(const std::array<char, 5>& correct, const std::array<std::vector<char>, 5>& misplaced, const std::string& wrong) {
    this->correct = correct;
    this->misplaced = std::array<std::vector<char>, 5>{
        misplaced[0], misplaced[1], misplaced[2], misplaced[3], misplaced[4]
    };
    this->wrong = std::vector<char>(wrong.cbegin(), wrong.cend());
}

WordFilter::WordFilter(const std::vector<WordFilter>& filters) {
    this->correct = { ' ', ' ', ' ', ' ', ' ' };
    this->misplaced = {};
    this->wrong = {};

    for (int i = 0; i < filters.size(); i++) {
        for (int j = 0; j < 5; j++) {
            //what???
            if (filters[i].correct[j] != ' ') this->correct[j] = filters[i].correct[j];

            //should reserve maybe
            this->misplaced[j].insert(this->misplaced[j].cend(), filters[i].misplaced[j].cbegin(), filters[i].misplaced[j].cend());
        }
        this->wrong.insert(this->wrong.cend(), filters[i].wrong.cbegin(), filters[i].wrong.cend());
    }

}

WordFilter::WordFilter(const std::string& solution, const std::string& guess) {
    this->correct = { ' ', ' ', ' ', ' ', ' ' };
    this->misplaced = {};
    this->wrong = {};

    for (int i = 0; i < solution.size(); i++) {
        if (solution[i] == guess[i]) {
            this->correct[i] = guess[i];
        }
        else {
            if (solution.find(guess[i]) != std::string::npos) {
                this->misplaced[i].emplace_back(guess[i]);
            }
            else {
                this->wrong.emplace_back(guess[i]);
            }
        }
    }
}

WordFilter::WordFilter(const char* solution, const char* guess) {
    this->correct = { ' ', ' ', ' ', ' ', ' ' };
    this->misplaced = {};
    this->wrong = {};

    for (int i = 0; i < 5; i++) {
        if (solution[i] == guess[i]) {
            this->correct[i] = guess[i];
        }
        else {
            if (std::memchr(solution, guess[i], 5) != nullptr) {
                this->misplaced[i].emplace_back(guess[i]);
            }
            else {
                this->wrong.emplace_back(guess[i]);
            }
        }
    }
}

std::vector<std::string> WordFilter::filterWords(const std::vector<std::string>& wordlist)
{
    std::vector<std::string> filtered{};
    filtered.reserve(100); //micro-optimization

    //Check each word in the given word list
    for (int i = 0; i < wordlist.size(); i++)
    {
        bool possibleWord = true;
        const std::string& word = wordlist[i];

        //Check if all the correct letters are present
        for (int j = 0; j < this->correct.size(); j++)
        {
            //Check if the correct letter is unknown, if so, skip (a space represents unknown)
            if (this->correct[j] == ' ') continue;

            if (this->correct[j] != word[j])
            {
                possibleWord = false;
                break;
            }
        }
        if (!possibleWord) continue;

        //Check if all the misplaced letters are present
        for (int j = 0; j < this->misplaced.size(); j++)
        {
            for (int k = 0; k < this->misplaced[j].size(); k++)
            {
                //Check that the letter is not in the same location
                if (this->misplaced[j][k] == word[j])
                {
                    possibleWord = false;
                    break;
                }

                //Check if the letter is present in the word
                //if (word.find(misplaced[j][k]) == std::string::npos)
                if(std::memchr(word.c_str(), misplaced[j][k], word.size()) == NULL)
                {
                    possibleWord = false;
                    break;
                }
            }

            if (!possibleWord) break;
        }
        if (!possibleWord) continue;

        //Check if the word contains any wrong letters
        //for (int j = 0; j < this->wrong.size(); j++)
        //{
        //    //if (word.find(this->wrong[j]) != std::string::npos)
        //    //if(std::memchr(word.c_str(), this->wrong[j], word.size()) != NULL)
        //    {
        //        possibleWord = false;
        //        break;
        //    }
        //}
        if (!find_no_common_chars_sse42(word.c_str(), word.size(), this->wrong.data(), this->wrong.size())) continue;

        filtered.emplace_back(word);
    }

    return filtered;
}

int WordFilter::filterWordsCount(const std::vector<std::string>& wordlist)
{
    int count = 0;
    //Check each word in the given word list
    for (int i = 0; i < wordlist.size(); i++)
    {
        bool possibleWord = true;
        const std::string& word = wordlist[i];

        //Check if all the correct letters are present
        for (int j = 0; j < this->correct.size(); j++)
        {
            //Check if the correct letter is unknown, if so, skip (a space represents unknown)
            if (this->correct[j] == ' ') continue;

            if (this->correct[j] != word[j])
            {
                possibleWord = false;
                break;
            }
        }
        if (!possibleWord) continue;

        //Check if all the misplaced letters are present
        for (int j = 0; j < this->misplaced.size(); j++)
        {
            for (int k = 0; k < this->misplaced[j].size(); k++)
            {
                //Check that the letter is not in the same location
                if (this->misplaced[j][k] == word[j])
                {
                    possibleWord = false;
                    break;
                }

                //Check if the letter is present in the word
                //if (word.find(misplaced[j][k]) == std::string::npos)
                if (std::memchr(word.c_str(), misplaced[j][k], word.size()) == NULL)
                {
                    possibleWord = false;
                    break;
                }
            }

            if (!possibleWord) break;
        }
        if (!possibleWord) continue;

        //Check if the word contains any wrong letters
        //for (int j = 0; j < this->wrong.size(); j++)
        //{
        //    //if (word.find(this->wrong[j]) != std::string::npos)
        //    //if(std::memchr(word.c_str(), this->wrong[j], word.size()) != NULL)
        //    {
        //        possibleWord = false;
        //        break;
        //    }
        //}
        if (!find_no_common_chars_sse42(word.c_str(), word.size(), this->wrong.data(), this->wrong.size())) continue;

        count++;
    }

    return count;
}

int WordFilter::optimized_filterWordsCount(const unsigned char* wordlist, size_t words, int* scratchmem) {
    int count = 0;

    //create null terminated wrong letter list
    size_t wrongletterlistsize = this->wrong.size();
    char* wrongletterlist = new char[wrongletterlistsize + 1];
    wrongletterlist[wrongletterlistsize] = 0;
    std::memcpy(wrongletterlist, this->wrong.data(), wrongletterlistsize);
    
    //create packed misplace letter representation
    size_t misplaceLettersLists_size = 0;
    char misplaceLettersLists_sizes[5];
    for (int i = 0; i < 5; i++) {
        misplaceLettersLists_size += this->misplaced[i].size();
        misplaceLettersLists_sizes[i] = this->misplaced[i].size();
    }
    char* misplaceLettersLists = nullptr;
    if(misplaceLettersLists_size != 0)
    {
        misplaceLettersLists = new char[misplaceLettersLists_size];
        size_t offsetsum = 0;
        for (int i = 0; i < 5; i++) {
            std::memcpy(&(misplaceLettersLists[offsetsum]), this->misplaced[i].data(), misplaceLettersLists_sizes[i]);
            offsetsum += misplaceLettersLists_sizes[i];
        }
    }
    

    //Check each word in the given word list
    for (int i = 0; i < words; i++)
    {
        const unsigned char* word = &(wordlist[i * 5]);

        //make letter counts
        /*for (int i = 0; i < 26; i++) {
            scratchmem[i] = 0;
        }
        for (int i = 0; i < 5; i++) {
            scratchmem[word[i] - 97] += 1;
        }*/

        //Check if all the correct letters are present
        if (!masked_greenletter_compare(this->correct.data(), word)) continue;

        //Check if all the misplaced letters are present
        //for (int j = 0; j < this->misplaced.size(); j++)
        //{
        //    for (int k = 0; k < this->misplaced[j].size(); k++)
        //    {
        //        //Check that the letter is not in the same location
        //        if (this->misplaced[j][k] == word[j])
        //        {
        //            possibleWord = false;
        //            break;
        //        }
        //
        //        //Check if the letter is present in the word
        //        //if (word.find(misplaced[j][k]) == std::string::npos)
        //        //if (std::memchr(word, misplaced[j][k], 5) == NULL)
        //        if(scratchmem[this->misplaced[j][k] - 97] == 0)
        //        {
        //            possibleWord = false;
        //            break;
        //        }
        //    }
        // if (!possibleWord) break;


        if ((misplaceLettersLists_size != 0) && !check_misplaced_letters(word, misplaceLettersLists, misplaceLettersLists_sizes)) continue;

        if ((wrongletterlistsize > 16 ?
            (!find_no_common_chars_sse42((const char*)word, 5, this->wrong.data(), wrongletterlistsize)) :
            fast_find_no_common_chars_sse42((const char*)word, wrongletterlist)
            )) continue;

        count++;
    }
    delete[] wrongletterlist;
    delete[] misplaceLettersLists;

    return count;
}
