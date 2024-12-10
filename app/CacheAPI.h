#ifndef CACHE_API_HPP
#define CACHE_API_HPP

#include <cstddef>
#include <iostream>

off_t get_file_size(int fd);
// Открытие файла по заданному пути
int lab2_open(const char *path);

// Закрытие файла по дескриптору
int lab2_close(int fd);

// Чтение данных из файла
ssize_t lab2_read(int fd, void *buf, size_t count);

// Запись данных в файл
ssize_t lab2_write(int fd, const void *buf, size_t count);

// Управление указателем файла (абсолютные координаты)
off_t lab2_lseek(int fd, off_t offset, int whence);

// Синхронизация данных из кэша с диском
int lab2_fsync(int fd);

// Подсказка кэшу о будущем доступе
int lab2_advice(int fd, off_t offset, size_t next_access_time);

#endif // CACHE_API_HPP
