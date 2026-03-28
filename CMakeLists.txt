cmake_minimum_required(VERSION 3.24)

project(oceandl VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

add_library(oceandl_lib
    cpp/src/builtin_datasets.cpp
    cpp/src/catalog.cpp
    cpp/src/cli.cpp
    cpp/src/cli_support.cpp
    cpp/src/config.cpp
    cpp/src/download_lock.cpp
    cpp/src/download_resume.cpp
    cpp/src/downloader.cpp
    cpp/src/http_client.cpp
    cpp/src/models.cpp
    cpp/src/providers/base.cpp
    cpp/src/providers/psl.cpp
    cpp/src/providers/registry.cpp
    cpp/src/reporter.cpp
    cpp/src/utils.cpp
    cpp/src/validation.cpp
)

target_include_directories(oceandl_lib PUBLIC cpp/include)
target_link_libraries(oceandl_lib PUBLIC CURL::libcurl fmt::fmt tomlplusplus::tomlplusplus)

add_executable(oceandl cpp/src/main.cpp)
target_link_libraries(oceandl PRIVATE oceandl_lib)

add_executable(oceandl_tests cpp/tests/test_main.cpp)
target_link_libraries(oceandl_tests PRIVATE oceandl_lib)

enable_testing()
add_test(NAME oceandl_tests COMMAND oceandl_tests)
