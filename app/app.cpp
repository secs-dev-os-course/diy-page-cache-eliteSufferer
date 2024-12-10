#include <windows.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "BlockCache.h"
#include "SubstringSearch.h"

struct FileDescriptor {
    HANDLE handle;
    off_t offset;
};

// Таблица открытых файлов
static std::unordered_map<int, FileDescriptor> file_table;
static int next_fd = 1; // Следующий доступный пользовательский дескриптор

constexpr size_t CACHE_SIZE = 10; // Максимальное количество страниц в кэше
BlockCache cache(CACHE_SIZE);     // Глобальный кэш

off_t get_file_size(int fd) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    HANDLE file_handle = it->second.handle;
    LARGE_INTEGER size;

    if (!GetFileSizeEx(file_handle, &size)) {
        DWORD error = GetLastError();
        std::cerr << "Ошибка получения размера файла: " << error << std::endl;
        return -1;
    }

    return static_cast<off_t>(size.QuadPart);
}

// Открытие файла
int lab2_open(const char* path) {
    HANDLE file_handle = CreateFile(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,  // Убран FILE_FLAG_OVERLAPPED
        nullptr
    );

    if (file_handle == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Unable to open file " << path << ", error code: " << GetLastError() << std::endl;
        return -1;
    }

    int fd = next_fd++;
    file_table[fd] = {file_handle, 0};
    return fd;
}


// Закрытие файла
int lab2_close(int fd) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Error: Invalid file descriptor " << fd << std::endl;
        return -1;
    }

    if (!CloseHandle(it->second.handle)) {
        std::cerr << "Error: Unable to close file, error code: " << GetLastError() << std::endl;
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

    FileDescriptor& fd_struct = it->second;
    HANDLE file_handle = fd_struct.handle;

    LARGE_INTEGER li_offset;
    LARGE_INTEGER new_position;
    DWORD move_method;

    switch (whence) {
        case SEEK_SET:
            move_method = FILE_BEGIN;
        li_offset.QuadPart = offset;
        break;
        case SEEK_CUR:
            move_method = FILE_CURRENT;
        li_offset.QuadPart = offset;
        break;
        case SEEK_END:
            move_method = FILE_END;
        li_offset.QuadPart = offset;
        break;
        default:
            std::cerr << "Ошибка: некорректное значение whence (" << whence << ")." << std::endl;
        return -1;
    }

    // Проверка выравнивания смещения только для SEEK_SET и SEEK_CUR
    if ((whence == SEEK_SET || whence == SEEK_CUR) && li_offset.QuadPart % BLOCK_SIZE != 0) {
        li_offset.QuadPart = (li_offset.QuadPart / BLOCK_SIZE) * BLOCK_SIZE;
    }

    if (!SetFilePointerEx(file_handle, li_offset, &new_position, move_method)) {
        DWORD error_code = GetLastError();
        std::cerr << "Ошибка: невозможно переместить указатель, код ошибки: " << error_code << std::endl;
        return -1;
    }

    fd_struct.offset = static_cast<off_t>(new_position.QuadPart);
    return fd_struct.offset;
}


// Чтение данных из файла
ssize_t lab2_read(int fd, void* buf, size_t count) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    FileDescriptor& fd_struct = it->second;
    HANDLE file_handle = fd_struct.handle;
    off_t offset = fd_struct.offset;

    // Выравниваем размер чтения
    size_t aligned_count = ((count + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

    // Создаем временный буфер для выровненного чтения
    std::vector<char> temp_buffer(aligned_count);
    DWORD bytes_read;

    // Используем синхронное чтение, так как позиция уже установлена через lab2_lseek
    if (!ReadFile(file_handle, temp_buffer.data(), static_cast<DWORD>(aligned_count),
                 &bytes_read, nullptr)) {
        DWORD error = GetLastError();
        if (error != ERROR_HANDLE_EOF) {
            std::cerr << "Ошибка чтения файла: " << error << std::endl;
            return -1;
        }
                 }

    if (bytes_read == 0) {
        return 0;
    }

    // Копируем только запрошенное количество байт
    size_t bytes_to_copy = std::min(count, static_cast<size_t>(bytes_read));
    memcpy(buf, temp_buffer.data(), bytes_to_copy);

    // Обновляем смещение в файле
    fd_struct.offset += bytes_to_copy;

    return bytes_to_copy;
}



// Запись данных в файл
ssize_t lab2_write(int fd, const void* buf, size_t count) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    FileDescriptor& fd_struct = it->second;
    HANDLE file_handle = fd_struct.handle;
    off_t offset = fd_struct.offset;
    size_t bytes_written = 0;


    while (count > 0) {
        // Выровненное смещение начала блока
        off_t block_offset = (offset / BLOCK_SIZE) * BLOCK_SIZE;
        size_t offset_in_block = offset % BLOCK_SIZE;

        // Размер данных для записи в текущий блок
        size_t bytes_to_write = std::min(BLOCK_SIZE - offset_in_block, count);

        // Получаем или создаем страницу в кэше
        auto page = cache.get_page(block_offset);
        if (!page) {
            page = std::make_shared<CachePage>(block_offset);

            // Если пишем не с начала блока или не весь блок,
            // нужно сначала прочитать существующие данные
            if (offset_in_block != 0 || bytes_to_write != BLOCK_SIZE) {
                OVERLAPPED overlapped = {0};
                overlapped.Offset = static_cast<DWORD>(block_offset & 0xFFFFFFFF);
                overlapped.OffsetHigh = static_cast<DWORD>((block_offset >> 32) & 0xFFFFFFFF);

                DWORD bytes_read;
                ReadFile(file_handle, page->data.data(), BLOCK_SIZE, &bytes_read, &overlapped);
            }
        }

        // Копируем данные в нужную позицию в блоке
        memcpy(page->data.data() + offset_in_block,
               static_cast<const char*>(buf) + bytes_written,
               bytes_to_write);

        page->dirty = true;
        cache.add_page(page);

        bytes_written += bytes_to_write;
        count -= bytes_to_write;
        offset += bytes_to_write;
    }

    return bytes_written;
}




int lab2_fsync(int fd) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    FileDescriptor& fd_struct = it->second;
    HANDLE file_handle = fd_struct.handle;

    // Сбрасываем все "грязные" страницы на диск
    for (auto& [offset, page] : cache.get_pages()) {
        if (page->dirty) {
            OVERLAPPED overlapped = {0};
            overlapped.Offset = static_cast<DWORD>(page->offset & 0xFFFFFFFF);
            overlapped.OffsetHigh = static_cast<DWORD>((page->offset >> 32) & 0xFFFFFFFF);

            DWORD bytes_written = 0;
            if (!WriteFile(file_handle, page->data.data(), BLOCK_SIZE, &bytes_written, &overlapped)) {
                std::cerr << "Ошибка записи на диск, offset = " << page->offset << std::endl;
                return -1;
            }
            page->dirty = false; // Помечаем страницу как "чистую"
        }
    }

    // Сбрасываем системный буфер на диск
    if (!FlushFileBuffers(file_handle)) {
        std::cerr << "Ошибка: сброс буфера ОС не удался, код ошибки: " << GetLastError() << std::endl;
        return -1;
    }

    return 0;
}


int lab2_advice(int fd, off_t offset, size_t next_access_time) {
    cache.update_access_hint(offset, next_access_time);
    return 0;
}


// // Тестовая функция
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

    // Синхронизация данных с диском
    if (lab2_fsync(fd) == -1) {
        std::cerr << "Тест провален: синхронизация с диском прошла с ошибкой." << std::endl;
        lab2_close(fd);
        return;
    }
    std::cout << "Синхронизация с диском прошла успешно." << std::endl;

    // Перемотка в начало файла для чтения
    if (lab2_lseek(fd, 0, SEEK_SET) == -1) {
        std::cerr << "Тест провален: ошибка перемотки в начало файла." << std::endl;
        lab2_close(fd);
        return;
    }

    // Чтение данных
    char read_buffer[64] = {0};
    ssize_t bytes_read = lab2_read(fd, read_buffer, sizeof(read_buffer) - 1);

    if (bytes_read == -1) {
        std::cerr << "Тест провален: чтение прошло с ошибкой." << std::endl;
        lab2_close(fd);
        return;
    }

    std::cout << "Чтение прошло успешно: прочитано " << bytes_read << " байт." << std::endl;
    std::cout << "Прочитанные данные: \"" << read_buffer << "\"" << std::endl;

    // Сравнение записанных и прочитанных данных
    if (strncmp(write_data, read_buffer, bytes_written) != 0) {
        std::cerr << "Тест провален: данные не совпадают." << std::endl;
    } else {
        std::cout << "Тест пройден: данные совпадают." << std::endl;
    }

    // Закрытие файла
    if (lab2_close(fd) == -1) {
        std::cerr << "Тест провален: ошибка закрытия файла." << std::endl;
    } else {
        std::cout << "Файл успешно закрыт." << std::endl;
    }
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

    if (bytes_read != 5 || strncmp(read_buffer, "45678", 5) != 0) {
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


void test_read_after_write() {
    const char* test_file = "test_cache.txt";

    int fd = lab2_open(test_file);
    if (fd == -1) {
        std::cerr << "Ошибка: не удалось открыть файл." << std::endl;
        return;
    }

    // Перемещаемся в начало файла (выровненная позиция)
    lab2_lseek(fd, 0, SEEK_SET);

    // Записываем данные
    const char* write_data = "Hello from cache!";
    size_t write_size = strlen(write_data);
    ssize_t bytes_written = lab2_write(fd, write_data, write_size);
    if (bytes_written != -1) {
        std::cout << "Записано через кэш: " << bytes_written << " байт." << std::endl;
    }

    // Возвращаемся в начало блока (выровненная позиция)
    lab2_lseek(fd, 0, SEEK_SET);

    // Читаем данные обратно
    char read_buffer[BLOCK_SIZE] = {0};  // Используем буфер размером с блок
    ssize_t bytes_read = lab2_read(fd, read_buffer, BLOCK_SIZE);
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

    std::cout << "Кэш после добавления первых двух страниц:" << std::endl;
    for (const auto& [offset, page] : cache.get_pages()) {
        std::cout << "Страница с offset: " << offset << std::endl;
    }

    // Добавляем новую страницу, должна вытесниться страница с самым поздним доступом
    auto page3 = std::make_shared<CachePage>(8192);
    cache.add_page(page3);

    std::cout << "Кэш после добавления третьей страницы:" << std::endl;
    for (const auto& [offset, page] : cache.get_pages()) {
        std::cout << "Страница с offset: " << offset << std::endl;
    }

    // Проверяем содержимое кэша
    auto page1_check = cache.get_page(0);
    auto page2_check = cache.get_page(4096);
    auto page3_check = cache.get_page(8192);

    if (page1_check) {
        std::cout << "Страница с offset 0 осталась в кэше." << std::endl;
    } else {
        std::cout << "Страница с offset 0 была вытеснена." << std::endl;
    }

    if (page2_check) {
        std::cout << "Страница с offset 4096 осталась в кэше." << std::endl;
    } else {
        std::cout << "Страница с offset 4096 была вытеснена." << std::endl;
    }

    if (page3_check) {
        std::cout << "Страница с offset 8192 успешно добавлена в кэш." << std::endl;
    } else {
        std::cout << "Страница с offset 8192 не добавлена в кэш (Ошибка)." << std::endl;
    }
}



// int main() {
//     SetConsoleOutputCP(CP_UTF8);
//     test_open_close();
//     test_read_write();
//     test_block_cache();
//     // test_cache_integration();
//     test_read_after_write();
//     // // debug_cache();
//     test_optimal_evict();
//
//     return 0;
// }

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "Select operation:\n";
    std::cout << "1. Run Substring Search without Cache\n";
    std::cout << "2. Run Substring Search with Cache\n";
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
