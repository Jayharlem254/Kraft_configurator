cmake_minimum_required(VERSION 3.15)
project(kraft_backend VERSION 3.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(kraft_backend
    src/server.cpp
    src/db.cpp
    src/engine.cpp
    src/seed.cpp
    src/sqlite3.c
)

if (WIN32)
    target_link_libraries(kraft_backend PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(kraft_backend PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

if (MSVC)
    target_compile_options(kraft_backend PRIVATE /utf-8 /EHsc /O2)
else()
    target_compile_options(kraft_backend PRIVATE -O2 -Wall)
endif()
