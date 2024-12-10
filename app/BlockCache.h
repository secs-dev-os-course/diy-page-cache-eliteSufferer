#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include <unordered_map>
#include <vector>
#include <memory>
#include <cstddef>

constexpr size_t BLOCK_SIZE = 512; // Размер одной страницы

// Структура для представления страницы в кэше
struct CachePage {
    off_t offset;    // Смещение в файле
    std::vector<char> data; // Данные страницы
    bool dirty;      // Флаг "грязной" страницы (изменена, но не записана на диск)

    CachePage(off_t off) : offset(off), data(BLOCK_SIZE, 0), dirty(false) {}
};

// Класс для управления блочным кэшем
class BlockCache {
private:
    size_t max_pages; // Максимальное количество страниц в кэше
    std::unordered_map<off_t, std::shared_ptr<CachePage>> pages; // Таблица страниц
    std::unordered_map<off_t, size_t> access_hints; // Подсказки о будущем доступе

public:
    explicit BlockCache(size_t max_pages);
    std::shared_ptr<CachePage> get_page(off_t offset);
    void add_page(std::shared_ptr<CachePage> page);
    void flush_pages(int fd);
    void update_access_hint(off_t offset, size_t next_access_time); // Подсказка
    void clear();
    std::unordered_map<off_t, std::shared_ptr<CachePage>>& get_pages(); // Для тестов

private:
    void evict_page(); // Вытеснение страницы
};

#endif // BLOCKCACHE_H
