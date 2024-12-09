#include <windows.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "BlockCache.h"
#include "LinearRegression.h"


// Таблица открытых файлов
static std::unordered_map<int, HANDLE> file_table;
static int next_fd = 1; // Следующий доступный пользовательский дескриптор

constexpr size_t CACHE_SIZE = 10; // Максимальное количество страниц в кэше
BlockCache cache(CACHE_SIZE);     // Глобальный кэш

// Открытие файла
int lab2_open(const char* path) {
    HANDLE file_handle = CreateFile(
        path,
        GENERIC_READ | GENERIC_WRITE, // Чтение и запись
        FILE_SHARE_READ | FILE_SHARE_WRITE, // Общий доступ
        nullptr,
        OPEN_EXISTING, // Открыть существующий файл
        FILE_ATTRIBUTE_NORMAL, // Убираем FILE_FLAG_NO_BUFFERING
        nullptr
    );

    if (file_handle == INVALID_HANDLE_VALUE) {
        std::cerr << "Ошибка: невозможно открыть файл " << path
                  << ", код ошибки: " << GetLastError() << std::endl;
        return -1;
    }

    int fd = next_fd++;
    file_table[fd] = file_handle;
    return fd;
}


// Закрытие файла
int lab2_close(int fd) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    if (!CloseHandle(it->second)) {
        std::cerr << "Ошибка: невозможно закрыть файл, код ошибки: " << GetLastError() << std::endl;
        return -1;
    }

    file_table.erase(it);
    return 0;
}


off_t lab2_lseek(int fd, off_t offset, int whence) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    HANDLE file_handle = it->second;

    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;

    LARGE_INTEGER new_position;
    DWORD move_method;

    switch (whence) {
        case SEEK_SET: move_method = FILE_BEGIN; break;
        case SEEK_CUR: move_method = FILE_CURRENT; break;
        case SEEK_END: move_method = FILE_END; break;
        default:
            std::cerr << "Ошибка: некорректное значение whence " << whence << std::endl;
        return -1;
    }

    if (!SetFilePointerEx(file_handle, li_offset, &new_position, move_method)) {
        std::cerr << "Ошибка: невозможно переместить указатель, код ошибки: " << GetLastError() << std::endl;
        return -1;
    }

    return static_cast<off_t>(new_position.QuadPart);
}

// Чтение данных из файла
ssize_t lab2_read(int fd, void* buf, size_t count) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    HANDLE file_handle = it->second;
    off_t offset = lab2_lseek(fd, 0, SEEK_CUR); // Получаем текущую позицию
    size_t bytes_read = 0;

    while (count > 0) {
        off_t page_offset = (offset / BLOCK_SIZE) * BLOCK_SIZE; // Смещение страницы
        size_t page_offset_in_block = offset % BLOCK_SIZE;      // Смещение внутри страницы

        std::cout << "Чтение: offset=" << offset
                  << ", page_offset=" << page_offset
                  << ", page_offset_in_block=" << page_offset_in_block << std::endl;

        // Получаем или загружаем страницу
        auto page = cache.get_page(page_offset);
        if (!page) {
            page = std::make_shared<CachePage>(page_offset);

            // Загружаем данные с диска
            lab2_lseek(fd, page_offset, SEEK_SET);
            DWORD bytes_from_disk = 0;
            if (!ReadFile(file_handle, page->data.data(), BLOCK_SIZE, &bytes_from_disk, nullptr)) {
                std::cerr << "Ошибка чтения с диска." << std::endl;
                return -1;
            }

            cache.add_page(page); // Добавляем страницу в кэш
        }

        // Копируем данные из кэша с учётом смещения
        size_t to_copy = std::min(BLOCK_SIZE - page_offset_in_block, count);
        memcpy(static_cast<char*>(buf) + bytes_read, &page->data[page_offset_in_block], to_copy);

        bytes_read += to_copy;
        count -= to_copy;
        offset += to_copy;
    }

    return bytes_read;
}




// Запись данных в файл
ssize_t lab2_write(int fd, const void* buf, size_t count) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    HANDLE file_handle = it->second;
    off_t offset = lab2_lseek(fd, 0, SEEK_CUR); // Получаем текущую позицию
    size_t bytes_written = 0;

    while (count > 0) {
        off_t page_offset = (offset / BLOCK_SIZE) * BLOCK_SIZE; // Смещение страницы
        size_t page_offset_in_block = offset % BLOCK_SIZE;      // Смещение внутри страницы

        // Получаем или создаём страницу
        auto page = cache.get_page(page_offset);
        if (!page) {
            page = std::make_shared<CachePage>(page_offset);
            cache.add_page(page); // Добавляем страницу в кэш
        }

        // Записываем данные в страницу с учётом смещения
        size_t to_copy = std::min(BLOCK_SIZE - page_offset_in_block, count);
        memcpy(&page->data[page_offset_in_block], static_cast<const char*>(buf) + bytes_written, to_copy);
        page->dirty = true; // Помечаем страницу как "грязную"

        bytes_written += to_copy;
        count -= to_copy;
        offset += to_copy;
    }

    lab2_lseek(fd, offset, SEEK_SET); // Обновляем указатель
    return bytes_written;
}




int lab2_fsync(int fd) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    HANDLE file_handle = it->second;

    // Сбрасываем все "грязные" страницы на диск
    for (auto& [offset, page] : cache.get_pages()) {
        if (page->dirty) {
            lab2_lseek(fd, page->offset, SEEK_SET); // Устанавливаем указатель на нужную страницу
            DWORD bytes_written = 0;
            if (!WriteFile(file_handle, page->data.data(), BLOCK_SIZE, &bytes_written, nullptr)) {
                std::cerr << "Ошибка записи на диск, offset = " << page->offset << std::endl;
                return -1;
            }
            page->dirty = false; // Страница больше не "грязная"
        }
    }

    // Сбрасываем системный буфер
    if (!FlushFileBuffers(file_handle)) {
        std::cerr << "Ошибка: сброс буфера диска не удался, код ошибки: " << GetLastError() << std::endl;
        return -1;
    }

    return 0;
}

int lab2_advice(int fd, off_t offset, size_t next_access_time) {
    cache.update_access_hint(offset, next_access_time);
    return 0;
}


// Тестовая функция
void test_open_close() {
    const char* test_file = "test.txt";

    // Открытие файла
    int fd = lab2_open(test_file);
    if (fd == -1) {
        std::cerr << "Тест провален: не удалось открыть файл." << std::endl;
        return;
    }

    std::cout << "Файл успешно открыт. Дескриптор: " << fd << std::endl;

    // Закрытие файла
    if (lab2_close(fd) == 0) {
        std::cout << "Файл успешно закрыт." << std::endl;
    } else {
        std::cerr << "Тест провален: не удалось закрыть файл." << std::endl;
    }
}

void test_read_write() {
    const char* test_file = "test.txt";

    // Открытие файла
    int fd = lab2_open(test_file);
    if (fd == -1) {
        std::cerr << "Тест провален: не удалось открыть файл." << std::endl;
        return;
    }

    // Запись данных
    const char* write_data = "Hello, world!";
    size_t write_size = strlen(write_data);
    ssize_t bytes_written = lab2_write(fd, write_data, write_size);

    if (bytes_written != write_size) {
        std::cerr << "Тест провален: запись прошла некорректно." << std::endl;
        lab2_close(fd);
        return;
    }

    std::cout << "Запись прошла успешно: записано " << bytes_written << " байт." << std::endl;

    // Перемотка в начало файла для чтения
    SetFilePointer(file_table[fd], 0, nullptr, FILE_BEGIN);

    // Чтение данных
    char read_buffer[64] = {0};
    ssize_t bytes_read = lab2_read(fd, read_buffer, sizeof(read_buffer) - 1);

    if (bytes_read == -1) {
        std::cerr << "Тест провален: чтение прошло с ошибкой." << std::endl;
        lab2_close(fd);
        return;
    }

    std::cout << "Чтение прошло успешно: прочитано " << bytes_read << " байт." << std::endl;
    std::cout << "Данные: " << read_buffer << std::endl;

    // Закрытие файла
    lab2_close(fd);
}

void test_lseek_fsync() {
    const char* test_file = "test.txt";

    // Открытие файла
    int fd = lab2_open(test_file);
    if (fd == -1) {
        std::cerr << "Тест провален: не удалось открыть файл." << std::endl;
        return;
    }

    // Запись данных
    const char* write_data = "1234567890";
    size_t write_size = strlen(write_data);
    ssize_t bytes_written = lab2_write(fd, write_data, write_size);

    if (bytes_written != write_size) {
        std::cerr << "Тест провален: запись прошла некорректно." << std::endl;
        lab2_close(fd);
        return;
    }
    std::cout << "Запись прошла успешно: записано " << bytes_written << " байт." << std::endl;

    // Перемещение указателя
    off_t new_position = lab2_lseek(fd, 3, SEEK_SET);
    if (new_position != 3) {
        std::cerr << "Тест провален: ошибка перемещения указателя." << std::endl;
        lab2_close(fd);
        return;
    }
    std::cout << "Указатель успешно перемещен: новая позиция = " << new_position << std::endl;

    // Чтение данных
    char read_buffer[8] = {0};
    ssize_t bytes_read = lab2_read(fd, read_buffer, 5);

    if (bytes_read != 5 || strcmp(read_buffer, "45678") != 0) {
        std::cerr << "Тест провален: данные прочитаны некорректно." << std::endl;
        lab2_close(fd);
        return;
    }
    std::cout << "Чтение прошло успешно: данные = " << read_buffer << std::endl;

    // Синхронизация с диском
    if (lab2_fsync(fd) == 0) {
        std::cout << "Данные успешно синхронизированы с диском." << std::endl;
    } else {
        std::cerr << "Тест провален: ошибка синхронизации." << std::endl;
    }

    // Закрытие файла
    lab2_close(fd);
}

void test_block_cache() {
    BlockCache cache(3); // Кэш на 3 страницы

    // Добавляем страницы в кэш
    auto page1 = std::make_shared<CachePage>(0);
    auto page2 = std::make_shared<CachePage>(4096);
    auto page3 = std::make_shared<CachePage>(8192);

    memcpy(page1->data.data(), "Page1_Data", 10);
    memcpy(page2->data.data(), "Page2_Data", 10);
    memcpy(page3->data.data(), "Page3_Data", 10);

    cache.add_page(page1);
    cache.add_page(page2);
    cache.add_page(page3);

    // Устанавливаем подсказки для алгоритма Optimal
    cache.update_access_hint(0, 100);      // Page1 будет нужна через 100 "единиц времени"
    cache.update_access_hint(4096, 200);  // Page2 будет нужна через 200
    cache.update_access_hint(8192, 50);   // Page3 будет нужна через 50

    std::cout << "Кэш после добавления страниц: ";
    for (const auto& [offset, page] : cache.get_pages()) {
        std::cout << offset << " ";
    }
    std::cout << std::endl;

    // Читаем данные из кэша
    auto fetched_page1 = cache.get_page(0);
    if (fetched_page1) {
        std::cout << "Page1: " << fetched_page1->data.data() << std::endl;
    }

    // Добавляем новую страницу и проверяем вытеснение
    auto page4 = std::make_shared<CachePage>(12288); // Это смещение 4096 * 3
    memcpy(page4->data.data(), "Page4_Data", 10);
    cache.add_page(page4);

    std::cout << "Кэш после добавления Page4: ";
    for (const auto& [offset, page] : cache.get_pages()) {
        std::cout << offset << " ";
    }
    std::cout << std::endl;

    // Проверяем, вытеснилась ли страница с offset=8192
    auto fetched_page3_after_evict = cache.get_page(8192);
    if (!fetched_page3_after_evict) {
        std::cout << "Page3 была вытеснена из кэша (OK)." << std::endl;
    } else {
        std::cerr << "Page3 не была вытеснена (Ошибка)." << std::endl;
    }

    // Проверяем, что Page4 в кэше
    auto fetched_page4 = cache.get_page(12288);
    if (fetched_page4) {
        std::cout << "Page4: " << fetched_page4->data.data() << std::endl;
    }
}




void test_cache_integration() {
    const char* test_file = "test_cache.txt";

    // Создаём тестовый файл
    FILE* f = fopen(test_file, "w");
    for (int i = 0; i < 1000; ++i) {
        fprintf(f, "Line %d\n", i);
    }
    fclose(f);

    // Открываем файл
    int fd = lab2_open(test_file);
    if (fd == -1) {
        std::cerr << "Ошибка: не удалось открыть файл." << std::endl;
        return;
    }

    // Читаем данные через кэш
    char buffer[128];
    ssize_t bytes_read = lab2_read(fd, buffer, sizeof(buffer));
    if (bytes_read != -1) {
        std::cout << "Прочитано через кэш: " << bytes_read << " байт." << std::endl;
    }

    // Пишем данные через кэш
    const char* write_data = "Hello from cache!";
    ssize_t bytes_written = lab2_write(fd, write_data, strlen(write_data));
    if (bytes_written != -1) {
        std::cout << "Записано через кэш: " << bytes_written << " байт." << std::endl;
    }

    // Синхронизируем данные
    lab2_fsync(fd);

    // Закрываем файл
    lab2_close(fd);
}

void debug_cache() {
    for (const auto& [offset, page] : cache.get_pages()) {
        std::cout << "Кэш содержит страницу с offset: " << offset << ", данные: "
                  << std::string(page->data.data(), BLOCK_SIZE) << std::endl;
    }
}


void test_read_after_write() {
    const char* test_file = "test_cache.txt";

    int fd = lab2_open(test_file);
    if (fd == -1) {
        std::cerr << "Ошибка: не удалось открыть файл." << std::endl;
        return;
    }

    // Перемещаемся в конец файла
    lab2_lseek(fd, 0, SEEK_END);
    off_t write_offset = lab2_lseek(fd, 0, SEEK_CUR);
    std::cout << "Указатель файла перед записью: " << write_offset << std::endl;

    // Записываем данные
    const char* write_data = "Hello from cache!";
    ssize_t bytes_written = lab2_write(fd, write_data, strlen(write_data));
    if (bytes_written != -1) {
        std::cout << "Записано через кэш: " << bytes_written << " байт." << std::endl;
    }

    // Проверяем указатель после записи
    off_t after_write_offset = lab2_lseek(fd, 0, SEEK_CUR);
    std::cout << "Указатель файла после записи: " << after_write_offset << std::endl;

    // Устанавливаем указатель файла точно на начало записанных данных
    lab2_lseek(fd, write_offset, SEEK_SET);

    // Читаем данные обратно
    char read_buffer[128] = {0};
    ssize_t bytes_read = lab2_read(fd, read_buffer, bytes_written);
    if (bytes_read != -1) {
        std::cout << "Прочитано после записи: " << bytes_read << " байт." << std::endl;
        std::cout << "Данные: " << read_buffer << std::endl;
    }

    lab2_close(fd);
}

void test_optimal_evict() {
    BlockCache cache(2); // Кэш на 2 страницы

    // Добавляем страницы
    auto page1 = std::make_shared<CachePage>(0);
    auto page2 = std::make_shared<CachePage>(4096);
    cache.add_page(page1);
    cache.add_page(page2);

    // Устанавливаем подсказки
    cache.update_access_hint(0, 100);     // Страница 0 понадобится через 100
    cache.update_access_hint(4096, 200); // Страница 4096 понадобится через 200

    // Добавляем новую страницу, должна вытесниться страница с самым поздним доступом
    auto page3 = std::make_shared<CachePage>(8192);
    cache.add_page(page3);

    // Проверяем содержимое кэша
    for (const auto& [offset, page] : cache.get_pages()) {
        std::cout << "Кэш содержит страницу с offset: " << offset << std::endl;
    }
}





// Точка входа
int main() {
    SetConsoleOutputCP(CP_UTF8);
    test_open_close();
    test_read_write();
    test_block_cache();
    test_cache_integration();
    test_read_after_write();
    // debug_cache();
    test_optimal_evict();

    return 0;
}

// int main() {
//     SetConsoleOutputCP(CP_UTF8);
//     // Зашитые параметры
//     int dataSize = 100;                  // Размер данных для регрессии
//     int repetitions = 10;                 // Количество повторений в одном прогоне
//     int numRepetitionsPerLoader = 2;       // Количество прогонов алгоритма
//
//     std::cout << "Running Linear Regression with parameters:\n";
//     std::cout << "Data Size: " << dataSize << "\n";
//     std::cout << "Repetitions per Run: " << repetitions << "\n";
//     std::cout << "Number of Runs per Loader: " << numRepetitionsPerLoader << "\n";
//
//     // Запуск линейной регрессии через кэш
//     runLinearRegression(dataSize, repetitions, numRepetitionsPerLoader);
//
//     return 0;
// }
