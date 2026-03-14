add_executable(rsc-test main.cpp)

target_include_directories(rsc-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(rsc-test SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/src/external)

find_library(LUA_STATIC_LIB
    NAMES liblua.a
    PATHS /usr/local/lib64 /usr/local/lib
    NO_DEFAULT_PATH
)

target_link_libraries(rsc-test PRIVATE CLI11::CLI11 nlohmann_json::nlohmann_json spdlog::spdlog ${LUA_STATIC_LIB})

install(TARGETS rsc-test
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)