cmake_minimum_required(VERSION 3.25)
project(mkd-tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif ()

option(BUILD_TESTING "Build tests" OFF)
option(MKD_BUILD_CLI "Build command-line interface" ON)

# release optimisations
if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if (ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()
endif ()

# dependencies
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

set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(glaze zlib)

if (NOT TARGET ZLIB::ZLIB)
    if (TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    else ()
        message(FATAL_ERROR "zlibstatic target not found")
    endif ()
endif ()

# sources
file(GLOB_RECURSE MKD_SOURCES src/*.cpp)

if (APPLE)
    list(FILTER MKD_SOURCES EXCLUDE REGEX ".*parallel\\.cpp$")
    file(GLOB_RECURSE MKD_OBJCXX_SOURCES src/*.mm)
    list(APPEND MKD_SOURCES ${MKD_OBJCXX_SOURCES})
    set_source_files_properties(${MKD_OBJCXX_SOURCES} PROPERTIES COMPILE_FLAGS "-fno-objc-arc")
else ()
    list(FILTER MKD_SOURCES EXCLUDE REGEX "macos/")
endif ()

add_library(mkd ${MKD_SOURCES} third_party/pugixml/pugixml.cpp)

target_include_directories(mkd PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(mkd PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/utfcpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/indicators>
)

target_link_libraries(mkd PUBLIC glaze::glaze ZLIB::ZLIB)

# compile/link flags
if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if (MSVC)
        target_compile_definitions(mkd PRIVATE NOMINMAX)
    else ()
        if (CMAKE_CROSSCOMPILING)
            target_compile_options(mkd PRIVATE -O3)
        else ()
            target_compile_options(mkd PRIVATE -march=native -O3)
        endif ()

        if (APPLE)
            target_link_options(mkd PRIVATE -Wl,-dead_strip)
        elseif (UNIX)
            target_compile_options(mkd PRIVATE -ffunction-sections -fdata-sections)
            target_link_options(mkd PRIVATE -s -Wl,--gc-sections -Wl,--exclude-libs,ALL)
        endif ()
    endif ()
endif ()

# platform libraries
if (APPLE)
    target_link_libraries(mkd PRIVATE
            "-framework Foundation"
            "-framework Security"
            "-framework CoreServices"
            "-framework Cocoa"
    )
elseif (WIN32)
    target_link_libraries(mkd PRIVATE bcrypt)
else ()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(mkd PRIVATE OpenSSL::Crypto)
endif ()

# CLI
if (MKD_BUILD_CLI)
    add_subdirectory(cli)
endif ()

# tests
if (BUILD_TESTING)
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
    target_include_directories(mkd-tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml
    )
    target_link_libraries(mkd-tests PRIVATE mkd GTest::gtest)

    include(GoogleTest)
    gtest_discover_tests(mkd-tests DISCOVERY_TIMEOUT 300)
endif ()
