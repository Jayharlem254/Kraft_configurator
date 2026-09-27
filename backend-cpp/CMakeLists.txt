cmake_minimum_required(VERSION 3.15)
project(kraft_backend VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(kraft_backend
    src/server.cpp
    src/db.cpp
    src/sqlite3.c
)

if (WIN32)
    target_link_libraries(kraft_backend PRIVATE ws2_32)
else()
    target_link_libraries(kraft_backend PRIVATE pthread dl)
endif()

if (MSVC)
    target_compile_options(kraft_backend PRIVATE /utf-8 /EHsc /O2)
else()
    target_compile_options(kraft_backend PRIVATE -O2 -Wall)
endif()
