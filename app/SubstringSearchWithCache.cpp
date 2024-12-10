#include <bemapiset.h>

#include "CacheAPI.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <windows.h>

#include "BlockCache.h"

void SubstringSearchWithCache(const std::string& filename, const std::string& substring, int repetitions) {
    const size_t BUFFER_SIZE = BLOCK_SIZE;
    int totalCount = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < repetitions; ++i) {
        int fd = lab2_open(filename.c_str());
        if (fd < 0) {
            std::cerr << "Не удалось открыть файл: " << filename << '\n';
            return;
        }

        off_t file_size = get_file_size(fd);
        if (file_size < 0) {
            lab2_close(fd);
            return;
        }

        std::cout << "Размер файла: " << file_size << " байт\n";

        std::string overflow;
        int count = 0;
        off_t current_offset = 0;

        while (current_offset < file_size) {
            // Выравниваем смещение по границе блока
            off_t aligned_offset = (current_offset / BLOCK_SIZE) * BLOCK_SIZE;

            // Устанавливаем позицию для чтения
            if (lab2_lseek(fd, aligned_offset, SEEK_SET) < 0) {
                std::cerr << "Ошибка позиционирования на offset " << aligned_offset << std::endl;
                break;
            }

            // Подсказка кэшу о следующем доступе
            lab2_advice(fd, aligned_offset, current_offset + BLOCK_SIZE);

            // Читаем блок
            std::vector<char> buffer(BUFFER_SIZE);
            ssize_t bytesRead = lab2_read(fd, buffer.data(), BUFFER_SIZE);

            if (bytesRead < 0) {
                std::cerr << "Ошибка чтения файла\n";
                break;
            }

            if (bytesRead == 0) {
                std::cerr << "Достигнут конец файла\n";
                break;
            }

            std::cout << "Прочитано " << bytesRead << " байт с позиции " << current_offset
                      << " (aligned: " << aligned_offset << ")\n";

            // Формируем текущий блок с учетом overflow
            std::string currentBlock = overflow + std::string(buffer.data(), bytesRead);

            // Обрабатываем overflow для следующего блока
            if (current_offset + bytesRead < file_size) {
                if (currentBlock.length() >= substring.length()) {
                    overflow = currentBlock.substr(currentBlock.length() - substring.length() + 1);
                    currentBlock = currentBlock.substr(0, currentBlock.length() - substring.length() + 1);
                } else {
                    overflow = currentBlock;
                    currentBlock.clear();
                }
            }

            // Поиск подстроки в текущем блоке
            size_t pos = 0;
            while ((pos = currentBlock.find(substring, pos)) != std::string::npos) {
                ++count;
                std::cout << "Найдено вхождение на позиции: " << current_offset + pos << std::endl;
                pos += 1;
            }

            current_offset += bytesRead;
        }

        totalCount += count;
        lab2_close(fd);
        std::cout << "Итерация " << i + 1 << ": найдено " << count << " вхождений\n";
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "Поиск завершён. Подстрока найдена: " << totalCount << " раз.\n";
    std::cout << "Время выполнения: " << duration.count() << " сек\n";
}

void runSubstringSearchWithCache() {
    std::string filename = "large_text_file.txt";
    std::string substring = "example";
    int repetitions = 1; // Начнем с одного повторения для отладки

    std::cout << "Запуск поиска подстроки с использованием кэша:\n";
    std::cout << "Файл: " << filename << "\n";
    std::cout << "Подстрока: \"" << substring << "\"\n";
    std::cout << "Количество повторений: " << repetitions << "\n";

    SubstringSearchWithCache(filename, substring, repetitions);
}