cmake_minimum_required(VERSION 3.16)
project(zorin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(zorin
    src/pe_parser.hpp
    src/pe_parser.cpp
    src/generator.hpp
    src/generator.cpp
    src/compiler.hpp
    src/compiler.cpp
    src/main.cpp
)

target_include_directories(zorin PRIVATE src)

if (MSVC)
    target_compile_options(zorin PRIVATE /W4 /O2 /EHsc)
    target_link_libraries(zorin PRIVATE user32 kernel32)
else()
    target_compile_options(zorin PRIVATE -Wall -Wextra -O2)
    target_link_options(zorin PRIVATE -static-libgcc -static-libstdc++)
endif()
