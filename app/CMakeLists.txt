cmake_minimum_required(VERSION 3.12)
project(BlockCacheLib VERSION 1.0 LANGUAGES CXX)

# Убедитесь, что C++ стандарт указан (например, C++11 или выше)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Library
add_library(BlockCache SHARED
        CacheAPI.cpp
        BlockCache.cpp
)

target_include_directories(BlockCache PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(BlockCache PRIVATE BUILD_DLL)

# App
add_executable(app
        SubstringSearchWithCache.cpp
        SubstringSearchWithoutCache.cpp
        SubstringSearch.h
        app.cpp
        Export.h
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(app PRIVATE BlockCache)
