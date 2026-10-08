cmake_minimum_required(VERSION 3.15)
project(fdtest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/prng.cpp
    src/io_threads.cpp
    src/drive_info.cpp
    src/disk_util.cpp
    src/console.cpp
    src/string_table.cpp
)

set(HEADERS
    include/fdtest.h
    src/prng.h
    src/io_threads.h
    src/drive_info.h
    src/disk_util.h
    src/console.h
)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    list(APPEND SOURCES res/fdtest.rc)
else()
    list(APPEND SOURCES src/platform_posix.cpp)
endif()

add_executable(fdtest ${SOURCES} ${HEADERS})

target_include_directories(fdtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(fdtest PRIVATE kernel32 user32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(fdtest PRIVATE Threads::Threads)
endif()

if(MSVC)
    target_compile_options(fdtest PRIVATE /W3)
endif()
