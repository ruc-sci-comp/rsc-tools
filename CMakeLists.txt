cmake_minimum_required(VERSION 4.2)

project(rsc-tools)

find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 23)
add_compile_options(-Wall -Wextra -Wpedantic -Weffc++)

add_subdirectory(src)
