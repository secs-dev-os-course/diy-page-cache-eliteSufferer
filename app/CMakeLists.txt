cmake_minimum_required(VERSION 3.12)
project(BlockCacheLib VERSION 1.0 LANGUAGES CXX)

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
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(app PRIVATE BlockCache)
