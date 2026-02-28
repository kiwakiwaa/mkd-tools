cmake_minimum_required(VERSION 3.25)
project(mkd-tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()

option(MKD_BUILD_TESTS "Build tests" OFF)
option(MKD_BUILD_CLI "Build command-line interface" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    add_compile_options(-march=native -O3)

    if(APPLE)
        add_link_options(-Wl,-dead_strip)
    elseif(UNIX)
        add_compile_options(-ffunction-sections -fdata-sections)
        add_link_options(-s -Wl,--gc-sections -Wl,--exclude-libs,ALL)
    endif()
endif()

include(FetchContent)

FetchContent_Declare(glaze
        GIT_REPOSITORY https://github.com/stephenberry/glaze.git
        GIT_TAG main
        GIT_SHALLOW TRUE
)

FetchContent_Declare(zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glaze zlib)

if (NOT TARGET ZLIB::ZLIB)
    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    else()
        message(FATAL_ERROR "zlibstatic target not found")
    endif()
endif()

file(GLOB_RECURSE MKD_SOURCES src/*.cpp)

if(APPLE)
    file(GLOB_RECURSE MKD_OBJCXX_SOURCES src/*.mm)
    list(APPEND MKD_SOURCES ${MKD_OBJCXX_SOURCES})
    set_source_files_properties(${MKD_OBJCXX_SOURCES} PROPERTIES COMPILE_FLAGS "-fno-objc-arc")
else()
    list(FILTER MKD_SOURCES EXCLUDE REGEX "macos/")
endif()

if(APPLE OR UNIX)
    list(FILTER MKD_SOURCES EXCLUDE REGEX ".*rsc_data_fileio\\.cpp$")
    list(FILTER MKD_SOURCES EXCLUDE REGEX ".*parallel\\.cpp$")
else()
    list(FILTER MKD_SOURCES EXCLUDE REGEX ".*rsc_data_mmap\\.cpp$")
endif()

add_library(mkd ${MKD_SOURCES}
        third_party/pugixml/pugixml.cpp
)

target_include_directories(mkd PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(mkd PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/utfcpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/indicators
)

target_link_libraries(mkd PUBLIC glaze::glaze ZLIB::ZLIB)

if (APPLE)
    target_link_libraries(mkd PRIVATE
            "-framework Foundation"
            "-framework Security"
            "-framework CoreServices"
            "-framework Cocoa"
    )
elseif (WIN32)
    target_link_libraries(mkd PRIVATE bcrypt)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(mkd PRIVATE OpenSSL::Crypto)
endif()


# CLI
if (MKD_BUILD_CLI)
    add_subdirectory(cli)
endif()


# Tests
if (MKD_BUILD_TESTS)
    set(CMAKE_POLICY_DEFAULT_CMP0000 OLD)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    FetchContent_Declare(googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES test/*.cpp)
    add_executable(mkd-tests ${TEST_SOURCES})
    target_include_directories(mkd-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml)
    target_link_libraries(mkd-tests PRIVATE mkd GTest::gtest)

    include(GoogleTest)
    gtest_discover_tests(mkd-tests DISCOVERY_TIMEOUT 300)
endif()