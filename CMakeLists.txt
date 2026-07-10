cmake_minimum_required(VERSION 3.16)
project(LOCAL_IPC_BROKER LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BROKER_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMMON_INCLUDE_DIR ${BROKER_ROOT}/common/include)
set(COMMON_SRC_DIR ${BROKER_ROOT}/common/src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)

add_library(
    messaging_common
    ${COMMON_SRC_DIR}/MessageCodec.cpp
)

target_include_directories(
    messaging_common
    PUBLIC
    ${COMMON_INCLUDE_DIR}
)

target_link_libraries(
    messaging_common
    PUBLIC
    nlohmann_json::nlohmann_json
)

function(add_ipc_app target_name source_path)
    add_executable(${target_name} ${source_path})
    target_link_libraries(${target_name} PRIVATE messaging_common)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endfunction()

add_ipc_app(broker ${BROKER_ROOT}/broker/main.cpp)
add_ipc_app(capture ${BROKER_ROOT}/capture/main.cpp)
add_ipc_app(analytics ${BROKER_ROOT}/analytics/main.cpp)

target_compile_features(messaging_common PUBLIC cxx_std_20)