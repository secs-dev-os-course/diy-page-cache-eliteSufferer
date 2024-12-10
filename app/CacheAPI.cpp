#define BUILD_DLL
#include "CacheAPI.h"

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

    // Выравниваем размер чтения только если это не последний блок
    size_t aligned_count = count;
    if (count % BLOCK_SIZE != 0) {
        aligned_count = ((count + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    }

    // Создаем временный буфер для выровненного чтения
    std::vector<char> temp_buffer(aligned_count);
    DWORD bytes_read;

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
