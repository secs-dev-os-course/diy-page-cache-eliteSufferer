#include "CacheAPI.h"

#include <windows.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "SubstringSearch.h"

// Таблица открытых файлов
static std::unordered_map<int, FileDescriptor> file_table;
static int next_fd = 1; // Следующий доступный пользовательский дескриптор

constexpr size_t CACHE_SIZE = 10; // Максимальное количество страниц в кэше

bool load_file_into_cache(int fd, off_t file_size) {
    std::cout << "Начало загрузки файла в кэш. Размер файла: " << file_size << " байт." << std::endl;
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return false;
    }
    HANDLE file_handle = it->second.handle;

    off_t current_offset = 0;

    while (current_offset < file_size) {
        std::cout << "Текущий offset: " << current_offset << std::endl;

        // Выравниваем смещение по границе блока
        off_t aligned_offset = (current_offset / BLOCK_SIZE) * BLOCK_SIZE;
        std::cout << "Чтение блока с offset: " << aligned_offset << std::endl;

        // Буфер для чтения блока
        std::vector<char> buffer(BLOCK_SIZE, 0);
        DWORD bytes_read = 0;

        // Читаем блок с диска последовательно
        BOOL read_success = ReadFile(file_handle, buffer.data(), BLOCK_SIZE, &bytes_read, NULL);
        if (!read_success) {
            DWORD error = GetLastError();
            if (error != ERROR_HANDLE_EOF) {
                std::cerr << "Ошибка чтения файла: " << error << std::endl;
                return false;
            }
        }

        if (bytes_read == 0) {
            std::cout << "Достигнут конец файла." << std::endl;
            break; // Конец файла
        }

        // Создаем страницу кэша и добавляем её в кэш
        auto page = std::make_shared<CachePage>(aligned_offset);
        memcpy(page->data.data(), buffer.data(), bytes_read);
        page->dirty = false; // Данные чистые
        get_cache_instance().add_page(page);

        std::cout << "Загружен блок в кэш: offset = " << aligned_offset << ", bytesRead = " << bytes_read << std::endl;
        std::cout << "Текущий размер кэша: " << get_cache_instance().get_pages().size() << " страниц." << std::endl;

        current_offset += bytes_read;
        std::cout << "Обновленный offset: " << current_offset << std::endl;
    }

    std::cout << "Загрузка в кэш завершена. Общий размер кэша: " << get_cache_instance().get_pages().size() << " страниц." << std::endl;
    return true;
}

BlockCache& get_cache_instance() {
    static BlockCache cache(CACHE_SIZE);
    return cache;
}

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
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
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


ssize_t lab2_read(int fd, void* buf, size_t count) {
    auto it = file_table.find(fd);
    if (it == file_table.end()) {
        std::cerr << "Ошибка: некорректный дескриптор файла " << fd << std::endl;
        return -1;
    }

    FileDescriptor& fd_struct = it->second;
    HANDLE file_handle = fd_struct.handle;
    off_t offset = fd_struct.offset;

    size_t bytes_to_read = count;
    size_t bytes_read_total = 0;
    char* buffer_ptr = static_cast<char*>(buf);

    while (bytes_to_read > 0) {
        off_t block_offset = (offset / BLOCK_SIZE) * BLOCK_SIZE;
        size_t offset_in_block = offset % BLOCK_SIZE;
        size_t bytes_available_in_block = BLOCK_SIZE - offset_in_block;
        size_t bytes_to_copy = std::min(bytes_available_in_block, bytes_to_read);

        // Попытка получить страницу из кэша
        auto cache_page = get_cache_instance().get_page(block_offset);
        if (cache_page) {
            // Копирование данных из кэша
            memcpy(buffer_ptr, cache_page->data.data() + offset_in_block, bytes_to_copy);
            std::cout << "Чтение из кэша: offset = " << block_offset << ", bytes_to_copy = " << bytes_to_copy << std::endl;
        } else {
            // Чтение с диска, если страница не в кэше
            std::cout << "Страница не найдена в кэше: offset = " << block_offset << ". Чтение с диска." << std::endl;

            // Буфер для чтения блока
            std::vector<char> temp_buffer(BLOCK_SIZE, 0);
            DWORD bytes_read = 0;

            // Читаем блок с диска
            BOOL read_success = ReadFile(file_handle, temp_buffer.data(), BLOCK_SIZE, &bytes_read, NULL);
            if (!read_success) {
                DWORD error = GetLastError();
                if (error != ERROR_HANDLE_EOF) {
                    std::cerr << "Ошибка чтения файла: " << error << std::endl;
                    return -1;
                }
            }

            if (bytes_read == 0) {
                std::cout << "Достигнут конец файла при чтении." << std::endl;
                break; // Конец файла
            }

            // Копирование данных в буфер
            memcpy(buffer_ptr, temp_buffer.data() + offset_in_block, bytes_to_copy);
            std::cout << "Чтение с диска: offset = " << block_offset << ", bytes_to_copy = " << bytes_to_copy << std::endl;

            // Добавление страницы в кэш
            auto new_page = std::make_shared<CachePage>(block_offset);
            memcpy(new_page->data.data(), temp_buffer.data(), bytes_read);
            new_page->dirty = false;
            get_cache_instance().add_page(new_page);
        }

        buffer_ptr += bytes_to_copy;
        offset += bytes_to_copy;
        bytes_read_total += bytes_to_copy;
        bytes_to_read -= bytes_to_copy;
    }

    fd_struct.offset = offset;
    return bytes_read_total;
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
        auto page = get_cache_instance().get_page(block_offset);
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
        get_cache_instance().add_page(page);

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
    for (auto& [offset, page] : get_cache_instance().get_pages()) {
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
    get_cache_instance().update_access_hint(offset, next_access_time);
    return 0;
}
