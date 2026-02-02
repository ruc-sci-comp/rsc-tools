add_executable(rsc-repo main.cpp)

target_include_directories(rsc-repo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(rsc-repo PRIVATE CLI11::CLI11 cpr::cpr nlohmann_json::nlohmann_json spdlog::spdlog)

install(TARGETS rsc-repo
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)