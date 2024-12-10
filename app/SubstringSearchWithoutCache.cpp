#include "SubstringSearch.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>

// Реализация SubstringSearch
void SubstringSearch(const std::string& filename, const std::string& substring, int repetitions) {
    const size_t BUFFER_SIZE = 8192;  // 8KB буфер, можно настроить
    std::vector<char> buffer(BUFFER_SIZE);
    int totalCount = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < repetitions; ++i) {
        std::ifstream file(filename, std::ios::binary);  // Открываем в бинарном режиме
        if (!file) {
            std::cerr << "Couldn't open file: " << filename << '\n';
            return;
        }

        std::string overflow;  // Для хранения части строки между блоками
        int count = 0;

        while (file) {
            // Читаем блок данных
            file.read(buffer.data(), BUFFER_SIZE);
            std::streamsize bytesRead = file.gcount();

            if (bytesRead == 0) break;

            std::string currentBlock = overflow + std::string(buffer.data(), bytesRead);

            if (!file.eof()) {
                if (currentBlock.length() >= substring.length()) {
                    overflow = currentBlock.substr(currentBlock.length() - substring.length() + 1);
                    currentBlock = currentBlock.substr(0, currentBlock.length() - substring.length() + 1);
                }
            }

            // Ищем все вхождения в текущем блоке
            size_t pos = 0;
            while ((pos = currentBlock.find(substring, pos)) != std::string::npos) {
                ++count;
                pos += 1;  // Двигаемся к следующему возможному вхождению
            }
        }

        totalCount += count;
        file.close();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "Search completed. Substring found: " << totalCount << " times.\n";
    std::cout << "Execution time: " << duration.count() << " secs\n";
}

// Функция для запуска SubstringSearch с фиксированными параметрами
void runSubstringSearchWithoutCache() {
    std::string filename = "large_text_file.txt";  // Имя файла для поиска
    std::string substring = "example";        // Подстрока для поиска
    int repetitions = 100;                         // Количество повторений

    std::cout << "Running Substring Search without Cache:\n";
    std::cout << "File: " << filename << "\n";
    std::cout << "Substring: \"" << substring << "\"\n";
    std::cout << "Repetitions: " << repetitions << "\n";

    SubstringSearch(filename, substring, repetitions);
}
