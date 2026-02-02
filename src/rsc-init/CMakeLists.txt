add_executable(rsc-init main.cpp)

target_include_directories(rsc-init PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(rsc-init PRIVATE CLI11::CLI11)

install(TARGETS rsc-init
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

set(RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates")

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/templates"
    DESTINATION "/rsc"
)