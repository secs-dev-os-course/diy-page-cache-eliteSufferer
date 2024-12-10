#ifndef CACHE_API_HPP
#define CACHE_API_HPP

#include <cstddef>
#include <iostream>

#ifdef _WIN32
    #ifdef BUILD_DLL
        #define DLL_EXPORT __declspec(dllexport)
    #else
        #define DLL_EXPORT __declspec(dllimport)
    #endif
#else
    #define DLL_EXPORT
#endif

DLL_EXPORT off_t get_file_size(int fd);
DLL_EXPORT int lab2_open(const char *path);
DLL_EXPORT int lab2_close(int fd);
DLL_EXPORT ssize_t lab2_read(int fd, void *buf, size_t count);
DLL_EXPORT ssize_t lab2_write(int fd, const void *buf, size_t count);
DLL_EXPORT off_t lab2_lseek(int fd, off_t offset, int whence);
DLL_EXPORT int lab2_fsync(int fd);
DLL_EXPORT int lab2_advice(int fd, off_t offset, size_t next_access_time);

#endif // CACHE_API_HPP
