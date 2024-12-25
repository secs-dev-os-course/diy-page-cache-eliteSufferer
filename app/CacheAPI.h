// CacheAPI.h
#ifndef CACHE_API_HPP
#define CACHE_API_HPP

#include <bemapiset.h>
#include <cstddef>
#include <iostream>
#include "Export.h"       // Для DLL_EXPORT
#include "BlockCache.h"   // Для класса BlockCache

#ifdef __cplusplus

struct FileDescriptor {
    HANDLE handle;
    off_t offset;
};
extern "C" {
#endif

    // Объявления функций API
    DLL_EXPORT int lab2_open(const char *path);
    DLL_EXPORT int lab2_close(int fd);
    DLL_EXPORT off_t lab2_lseek(int fd, off_t offset, int whence);
    DLL_EXPORT ssize_t lab2_read(int fd, void *buf, size_t count);
    DLL_EXPORT ssize_t lab2_write(int fd, const void *buf, size_t count);
    DLL_EXPORT int lab2_fsync(int fd);
    DLL_EXPORT int lab2_advice(int fd, off_t offset, size_t next_access_time);
    DLL_EXPORT off_t get_file_size(int fd);

    DLL_EXPORT bool load_file_into_cache(int fd, off_t file_size);


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// Функция для получения ссылки на глобальный кэш
DLL_EXPORT BlockCache& get_cache_instance();
#endif

#endif // CACHE_API_HPP
