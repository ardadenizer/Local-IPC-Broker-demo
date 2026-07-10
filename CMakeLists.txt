cmake_minimum_required(VERSION 3.16)
project(LOCAL_IPC_BROKER LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(broker     ${CMAKE_SOURCE_DIR}/broker/main.cpp)
add_executable(capture    ${CMAKE_SOURCE_DIR}/capture/main.cpp)
add_executable(analytics  ${CMAKE_SOURCE_DIR}/analytics/main.cpp)

target_include_directories(broker PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_include_directories(capture PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_include_directories(analytics PRIVATE ${CMAKE_SOURCE_DIR}/common)

set_target_properties(broker capture analytics PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)