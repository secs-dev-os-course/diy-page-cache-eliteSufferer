//
//
// // // Тестовая функция
// void test_open_close() {
//     const char* test_file = "test.txt";
//
//     // Открытие файла
//     int fd = lab2_open(test_file);
//     if (fd == -1) {
//         std::cerr << "Тест провален: не удалось открыть файл." << std::endl;
//         return;
//     }
//
//     std::cout << "Файл успешно открыт. Дескриптор: " << fd << std::endl;
//
//     // Закрытие файла
//     if (lab2_close(fd) == 0) {
//         std::cout << "Файл успешно закрыт." << std::endl;
//     } else {
//         std::cerr << "Тест провален: не удалось закрыть файл." << std::endl;
//     }
// }
//
// void test_read_write() {
//     const char* test_file = "test.txt";
//
//     // Открытие файла
//     int fd = lab2_open(test_file);
//     if (fd == -1) {
//         std::cerr << "Тест провален: не удалось открыть файл." << std::endl;
//         return;
//     }
//
//     // Запись данных
//     const char* write_data = "Hello, world!";
//     size_t write_size = strlen(write_data);
//     ssize_t bytes_written = lab2_write(fd, write_data, write_size);
//
//     if (bytes_written != write_size) {
//         std::cerr << "Тест провален: запись прошла некорректно." << std::endl;
//         lab2_close(fd);
//         return;
//     }
//
//     std::cout << "Запись прошла успешно: записано " << bytes_written << " байт." << std::endl;
//
//     // Синхронизация данных с диском
//     if (lab2_fsync(fd) == -1) {
//         std::cerr << "Тест провален: синхронизация с диском прошла с ошибкой." << std::endl;
//         lab2_close(fd);
//         return;
//     }
//     std::cout << "Синхронизация с диском прошла успешно." << std::endl;
//
//     // Перемотка в начало файла для чтения
//     if (lab2_lseek(fd, 0, SEEK_SET) == -1) {
//         std::cerr << "Тест провален: ошибка перемотки в начало файла." << std::endl;
//         lab2_close(fd);
//         return;
//     }
//
//     // Чтение данных
//     char read_buffer[64] = {0};
//     ssize_t bytes_read = lab2_read(fd, read_buffer, sizeof(read_buffer) - 1);
//
//     if (bytes_read == -1) {
//         std::cerr << "Тест провален: чтение прошло с ошибкой." << std::endl;
//         lab2_close(fd);
//         return;
//     }
//
//     std::cout << "Чтение прошло успешно: прочитано " << bytes_read << " байт." << std::endl;
//     std::cout << "Прочитанные данные: \"" << read_buffer << "\"" << std::endl;
//
//     // Сравнение записанных и прочитанных данных
//     if (strncmp(write_data, read_buffer, bytes_written) != 0) {
//         std::cerr << "Тест провален: данные не совпадают." << std::endl;
//     } else {
//         std::cout << "Тест пройден: данные совпадают." << std::endl;
//     }
//
//     // Закрытие файла
//     if (lab2_close(fd) == -1) {
//         std::cerr << "Тест провален: ошибка закрытия файла." << std::endl;
//     } else {
//         std::cout << "Файл успешно закрыт." << std::endl;
//     }
// }
//
//
// void test_lseek_fsync() {
//     const char* test_file = "test.txt";
//
//     // Открытие файла
//     int fd = lab2_open(test_file);
//     if (fd == -1) {
//         std::cerr << "Тест провален: не удалось открыть файл." << std::endl;
//         return;
//     }
//
//     // Запись данных
//     const char* write_data = "1234567890";
//     size_t write_size = strlen(write_data);
//     ssize_t bytes_written = lab2_write(fd, write_data, write_size);
//
//     if (bytes_written != write_size) {
//         std::cerr << "Тест провален: запись прошла некорректно." << std::endl;
//         lab2_close(fd);
//         return;
//     }
//     std::cout << "Запись прошла успешно: записано " << bytes_written << " байт." << std::endl;
//
//     // Перемещение указателя
//     off_t new_position = lab2_lseek(fd, 3, SEEK_SET);
//     if (new_position != 3) {
//         std::cerr << "Тест провален: ошибка перемещения указателя." << std::endl;
//         lab2_close(fd);
//         return;
//     }
//     std::cout << "Указатель успешно перемещен: новая позиция = " << new_position << std::endl;
//
//     // Чтение данных
//     char read_buffer[8] = {0};
//     ssize_t bytes_read = lab2_read(fd, read_buffer, 5);
//
//     if (bytes_read != 5 || strncmp(read_buffer, "45678", 5) != 0) {
//         std::cerr << "Тест провален: данные прочитаны некорректно." << std::endl;
//         lab2_close(fd);
//         return;
//     }
//     std::cout << "Чтение прошло успешно: данные = " << read_buffer << std::endl;
//
//     // Синхронизация с диском
//     if (lab2_fsync(fd) == 0) {
//         std::cout << "Данные успешно синхронизированы с диском." << std::endl;
//     } else {
//         std::cerr << "Тест провален: ошибка синхронизации." << std::endl;
//     }
//
//     // Закрытие файла
//     lab2_close(fd);
// }
//
//
// void test_block_cache() {
//     BlockCache cache(3); // Кэш на 3 страницы
//
//     // Добавляем страницы в кэш
//     auto page1 = std::make_shared<CachePage>(0);
//     auto page2 = std::make_shared<CachePage>(4096);
//     auto page3 = std::make_shared<CachePage>(8192);
//
//     memcpy(page1->data.data(), "Page1_Data", 10);
//     memcpy(page2->data.data(), "Page2_Data", 10);
//     memcpy(page3->data.data(), "Page3_Data", 10);
//
//     cache.add_page(page1);
//     cache.add_page(page2);
//     cache.add_page(page3);
//
//     // Устанавливаем подсказки для алгоритма Optimal
//     cache.update_access_hint(0, 100);      // Page1 будет нужна через 100 "единиц времени"
//     cache.update_access_hint(4096, 200);  // Page2 будет нужна через 200
//     cache.update_access_hint(8192, 50);   // Page3 будет нужна через 50
//
//     std::cout << "Кэш после добавления страниц: ";
//     for (const auto& [offset, page] : cache.get_pages()) {
//         std::cout << offset << " ";
//     }
//     std::cout << std::endl;
//
//     // Читаем данные из кэша
//     auto fetched_page1 = cache.get_page(0);
//     if (fetched_page1) {
//         std::cout << "Page1: " << fetched_page1->data.data() << std::endl;
//     }
//
//     // Добавляем новую страницу и проверяем вытеснение
//     auto page4 = std::make_shared<CachePage>(12288); // Это смещение 4096 * 3
//     memcpy(page4->data.data(), "Page4_Data", 10);
//     cache.add_page(page4);
//
//     std::cout << "Кэш после добавления Page4: ";
//     for (const auto& [offset, page] : cache.get_pages()) {
//         std::cout << offset << " ";
//     }
//     std::cout << std::endl;
//
//     // Проверяем, вытеснилась ли страница с offset=8192
//     auto fetched_page3_after_evict = cache.get_page(8192);
//     if (!fetched_page3_after_evict) {
//         std::cout << "Page3 была вытеснена из кэша (OK)." << std::endl;
//     } else {
//         std::cerr << "Page3 не была вытеснена (Ошибка)." << std::endl;
//     }
//
//     // Проверяем, что Page4 в кэше
//     auto fetched_page4 = cache.get_page(12288);
//     if (fetched_page4) {
//         std::cout << "Page4: " << fetched_page4->data.data() << std::endl;
//     }
// }
//
//
// void test_read_after_write() {
//     const char* test_file = "test_cache.txt";
//
//     int fd = lab2_open(test_file);
//     if (fd == -1) {
//         std::cerr << "Ошибка: не удалось открыть файл." << std::endl;
//         return;
//     }
//
//     // Перемещаемся в начало файла
//     lab2_lseek(fd, 0, SEEK_SET);
//
//     // Записываем данные
//     const char* write_data = "Hello from cache!";
//     size_t write_size = strlen(write_data);
//     ssize_t bytes_written = lab2_write(fd, write_data, write_size);
//     if (bytes_written != -1) {
//         std::cout << "Записано через кэш: " << bytes_written << " байт." << std::endl;
//     }
//
//     // Синхронизируем данные с диском
//     if (lab2_fsync(fd) == -1) {
//         std::cerr << "Ошибка синхронизации с диском" << std::endl;
//         lab2_close(fd);
//         return;
//     }
//
//     // Возвращаемся в начало файла
//     lab2_lseek(fd, 0, SEEK_SET);
//
//     // Читаем данные обратно
//     char read_buffer[BLOCK_SIZE] = {0};
//     ssize_t bytes_read = lab2_read(fd, read_buffer, write_size); // Читаем столько же байт, сколько записали
//     if (bytes_read != -1) {
//         std::cout << "Прочитано после записи: " << bytes_read << " байт." << std::endl;
//         std::cout << "Данные: " << read_buffer << std::endl;
//
//         // Проверяем корректность прочитанных данных
//         if (strncmp(write_data, read_buffer, write_size) == 0) {
//             std::cout << "Данные совпадают!" << std::endl;
//         } else {
//             std::cerr << "Ошибка: данные не совпадают!" << std::endl;
//         }
//     }
//
//     lab2_close(fd);
// }
//
// void test_optimal_evict() {
//     BlockCache cache(2); // Кэш на 2 страницы
//
//     // Добавляем страницы
//     auto page1 = std::make_shared<CachePage>(0);
//     auto page2 = std::make_shared<CachePage>(4096);
//     cache.add_page(page1);
//     cache.add_page(page2);
//
//     // Устанавливаем подсказки
//     cache.update_access_hint(0, 100);     // Страница 0 понадобится через 100
//     cache.update_access_hint(4096, 200); // Страница 4096 понадобится через 200
//
//     std::cout << "Кэш после добавления первых двух страниц:" << std::endl;
//     for (const auto& [offset, page] : cache.get_pages()) {
//         std::cout << "Страница с offset: " << offset << std::endl;
//     }
//
//     // Добавляем новую страницу, должна вытесниться страница с самым поздним доступом
//     auto page3 = std::make_shared<CachePage>(8192);
//     cache.add_page(page3);
//
//     std::cout << "Кэш после добавления третьей страницы:" << std::endl;
//     for (const auto& [offset, page] : cache.get_pages()) {
//         std::cout << "Страница с offset: " << offset << std::endl;
//     }
//
//     // Проверяем содержимое кэша
//     auto page1_check = cache.get_page(0);
//     auto page2_check = cache.get_page(4096);
//     auto page3_check = cache.get_page(8192);
//
//     if (page1_check) {
//         std::cout << "Страница с offset 0 осталась в кэше." << std::endl;
//     } else {
//         std::cout << "Страница с offset 0 была вытеснена." << std::endl;
//     }
//
//     if (page2_check) {
//         std::cout << "Страница с offset 4096 осталась в кэше." << std::endl;
//     } else {
//         std::cout << "Страница с offset 4096 была вытеснена." << std::endl;
//     }
//
//     if (page3_check) {
//         std::cout << "Страница с offset 8192 успешно добавлена в кэш." << std::endl;
//     } else {
//         std::cout << "Страница с offset 8192 не добавлена в кэш (Ошибка)." << std::endl;
//     }
// }



// int main() {
//     SetConsoleOutputCP(CP_UTF8);
//     test_open_close();
//     test_read_write();
//     test_block_cache();
//     test_read_after_write();
//     test_optimal_evict();
//
//     return 0;
// }

#include <iostream>
#include <windows.h>

#include "SubstringSearch.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "Select operation:\n";
    std::cout << "1. Run Substring Search without cache\n";
    std::cout << "2. Run Substring Search with cache\n";

    int choice;
    std::cin >> choice;

    if (choice == 1) {
        runSubstringSearchWithoutCache();
    } else if (choice == 2) {
        runSubstringSearchWithCache();
    } else {
        std::cout << "Invalid choice.\n";
    }

    return 0;
}

