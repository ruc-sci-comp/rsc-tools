add_executable(rsc-grade main.cpp)

target_include_directories(rsc-grade PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(rsc-grade SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/src/external)

target_link_libraries(rsc-grade PRIVATE CLI11::CLI11 nlohmann_json::nlohmann_json spdlog::spdlog)

install(TARGETS rsc-grade
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)