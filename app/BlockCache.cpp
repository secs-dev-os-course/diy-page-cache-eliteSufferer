#include "BlockCache.h"
#include <algorithm>
#include <cstring>
#include <iostream>

// Конструктор
BlockCache::BlockCache(size_t max_pages) : max_pages(max_pages) {}

// Получить страницу из кэша
std::shared_ptr<CachePage> BlockCache::get_page(off_t offset) {
    auto it = pages.find(offset);
    if (it != pages.end()) {
        return it->second;
    }
    return nullptr;
}

// Добавить страницу в кэш
void BlockCache::add_page(std::shared_ptr<CachePage> page) {
    if (pages.size() >= max_pages) {
        evict_page();
    }
    pages[page->offset] = page;
}

// Удалить "грязные" страницы (синхронизация)
void BlockCache::flush_pages(int fd) {
    for (auto& [offset, page] : pages) {
        if (page->dirty) {
            // Здесь можно реализовать вызовы lab2_write для записи на диск
            std::cerr << "Warning: flush_pages не завершен.\n";
        }
    }
}

// Обновить подсказку о будущем доступе
void BlockCache::update_access_hint(off_t offset, size_t next_access_time) {
    access_hints[offset] = next_access_time;
}

// Очистить кэш
void BlockCache::clear() {
    pages.clear();
    access_hints.clear();
}

// Вытеснение страницы по алгоритму Optimal
void BlockCache::evict_page() {
    if (pages.empty()) return;

    off_t offset_to_evict = -1;
    size_t min_access_time = SIZE_MAX;

    std::cout << "Кэш перед вытеснением: ";
    for (const auto& [offset, page] : pages) {
        std::cout << offset << " ";
    }
    std::cout << std::endl;

    // Ищем страницу с наименьшим access_time
    for (const auto& [offset, page] : pages) {
        size_t access_time = access_hints.count(offset) ? access_hints[offset] : SIZE_MAX;

        std::cout << "Offset: " << offset << ", Access time: " << access_time << std::endl;

        if (access_time < min_access_time) { // Ищем минимальный access_time
            min_access_time = access_time;
            offset_to_evict = offset;
        }
    }

    // Если не удалось найти подходящую страницу, удаляем первую
    if (offset_to_evict == -1) {
        offset_to_evict = pages.begin()->first;
    }

    std::cout << "Вытесняем страницу с offset: " << offset_to_evict << std::endl;

    // Удаляем страницу из кэша
    pages.erase(offset_to_evict);
    access_hints.erase(offset_to_evict); // Удаляем подсказку для вытеснённой страницы
}



// Вернуть все страницы (для тестов)
std::unordered_map<off_t, std::shared_ptr<CachePage>>& BlockCache::get_pages() {
    return pages;
}
