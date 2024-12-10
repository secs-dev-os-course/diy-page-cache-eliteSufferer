#ifndef SUBSTRINGSEARCH_H
#define SUBSTRINGSEARCH_H
#include <string>

void SubstringSearch(const std::string& filename, const std::string& substring, int repetitions);

void runSubstringSearchWithCache();

void runSubstringSearchWithoutCache();
#endif //SUBSTRINGSEARCH_H
