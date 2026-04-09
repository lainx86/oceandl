cmake_minimum_required(VERSION 3.24)

project(oceandl VERSION 0.2.8 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(
    MSVC
    AND DEFINED VCPKG_TARGET_TRIPLET
    AND VCPKG_TARGET_TRIPLET MATCHES "-static$"
)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(
    OCEANDL_STRICT_WARNINGS
    "Enable the maintainer warning policy for oceandl targets."
    OFF
)

option(
    OCEANDL_ENABLE_RELEASE_BUNDLE
    "Bundle non-system runtime dependencies into the install tree for release archives."
    OFF
)
set(
    OCEANDL_EXTRA_BUNDLE_DIRS
    ""
    CACHE STRING
    "Additional directories searched when bundling runtime dependencies."
)

find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

function(oceandl_apply_warnings target_name)
    if(NOT OCEANDL_STRICT_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_library(oceandl_lib
    cpp/src/builtin_datasets.cpp
    cpp/src/catalog.cpp
    cpp/src/cli.cpp
    cpp/src/cli_support.cpp
    cpp/src/config.cpp
    cpp/src/download_lock.cpp
    cpp/src/download_command.cpp
    cpp/src/download_resume.cpp
    cpp/src/downloader.cpp
    cpp/src/http_client.cpp
    cpp/src/models.cpp
    cpp/src/providers/base.cpp
    cpp/src/providers/psl.cpp
    cpp/src/providers/registry.cpp
    cpp/src/reporter.cpp
    cpp/src/target_download_executor.cpp
    cpp/src/utils.cpp
    cpp/src/validation.cpp
)

target_include_directories(oceandl_lib PUBLIC cpp/include)
target_link_libraries(oceandl_lib PUBLIC CURL::libcurl fmt::fmt tomlplusplus::tomlplusplus)
if(WIN32)
    target_compile_definitions(oceandl_lib PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static")
        target_compile_definitions(oceandl_lib PUBLIC CURL_STATICLIB)
    endif()
endif()
oceandl_apply_warnings(oceandl_lib)

add_executable(oceandl cpp/src/main.cpp)
target_link_libraries(oceandl PRIVATE oceandl_lib)
oceandl_apply_warnings(oceandl)

install(TARGETS oceandl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
    FILES
        README.md
        LICENSE
        CHANGELOG.md
        CONTRIBUTING.md
        CODE_OF_CONDUCT.md
        SECURITY.md
        RELEASING.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

if(OCEANDL_ENABLE_RELEASE_BUNDLE)
    set(_oceandl_bundle_script "${CMAKE_CURRENT_BINARY_DIR}/oceandl_install_bundle.cmake")
    configure_file(
        cmake/oceandl_install_bundle.cmake.in
        "${_oceandl_bundle_script}"
        @ONLY
    )
    install(SCRIPT "${_oceandl_bundle_script}")
endif()

if(BUILD_TESTING)
    add_executable(oceandl_tests cpp/tests/test_main.cpp)
    target_link_libraries(oceandl_tests PRIVATE oceandl_lib)
    oceandl_apply_warnings(oceandl_tests)

    add_test(NAME oceandl_tests COMMAND oceandl_tests)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(
                NAME oceandl_integration_http_local
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests/hermetic_http_integration.py"
                    --oceandl "$<TARGET_FILE:oceandl>"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/oceandl-http-integration"
            )
            set_tests_properties(
                oceandl_integration_http_local
                PROPERTIES
                    LABELS integration
                    TIMEOUT 60
            )
        endif()
    endif()
endif()
