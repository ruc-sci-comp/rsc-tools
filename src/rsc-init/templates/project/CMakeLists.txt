cmake_minimum_required(VERSION 4.2)

project({{name}})

set(CMAKE_CXX_STANDARD 23)

add_compile_options(-Wall -Wextra -Wpedantic -Weffc++)

add_executable({{name}} src/main.cpp)

target_include_directories({{name}} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

install(TARGETS {{name}}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
