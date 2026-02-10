cmake_minimum_required(VERSION 4.0)
project(monokakido_extractor)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MACOSX_BUNDLE TRUE)

option(BUILD_TESTING "Build the testing tree" ON)
include(CTest)

include(FetchContent)

# Glaze (json library)
FetchContent_Declare(
        glaze
        GIT_REPOSITORY https://github.com/stephenberry/glaze.git
        GIT_TAG main
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glaze)

# Google Test
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(ZLIB REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${glaze_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/utfcpp)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/indicators)


# Core library
add_library(monokakido_core STATIC
        src/core/platform/fs.mm
        src/core/platform/scoped_security_access.mm
)

target_link_libraries(monokakido_core PRIVATE
        "-framework Foundation"
        "-framework Security"
        "-framework CoreServices"
        "-framework Cocoa"
)


# --- Dictionary library ---
add_library(monokakido_dictionary STATIC
        src/dictionary/catalog.cpp
        src/dictionary/metadata.cpp
        src/dictionary/paths.cpp
        src/dictionary/dictionary.cpp
)


# --- Output library ---
add_library(monokakido_output STATIC
        third_party/pugixml/pugixml.cpp
        src/output/base_exporter.cpp
        src/output/resource_exporter.cpp
)


# --- Resource library ---
add_library(monokakido_resource STATIC
        src/resource/nrsc/nrsc.cpp
        src/resource/nrsc/nrsc_index.cpp
        src/resource/nrsc/nrsc_index_record.cpp
        src/resource/nrsc/nrsc_data.cpp
        src/resource/zlib_decompressor.cpp
        src/resource/resource_loader.cpp
        src/resource/rsc/rsc_index.cpp
        src/resource/rsc/rsc_data.cpp
        src/resource/common.cpp
        src/resource/rsc/rsc_index_record.cpp
        src/resource/rsc/rsc_map.cpp
        src/resource/xml_view.cpp
        src/resource/rsc/rsc_crypto.cpp
        src/resource/rsc/rsc.cpp
        src/resource/font.cpp
        src/utils/kana_convert.cpp
)

target_link_libraries(monokakido_resource PRIVATE
        monokakido_core
)


# --- Executable as macOS bundle ---
add_executable(monokakido_extractor MACOSX_BUNDLE
        src/main.cpp
)

target_link_libraries(monokakido_extractor PRIVATE
        monokakido_core
        monokakido_dictionary
        monokakido_resource
        monokakido_output
        glaze::glaze
        ZLIB::ZLIB
)

set_target_properties(monokakido_extractor PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Monokakido Extractor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.kiwaa.monokakido-extractor"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)


if (BUILD_TESTING)
    enable_testing()
    set(TEST_SOURCES
            test/test_listener.hpp

            test/nrsc/test_nrsc_index.cpp
            test/rsc/test_rsc_data.cpp
            test/rsc/test_rsc_index.cpp
            test/main.cpp
            test/keystore/test_keystore.cpp
    )

    add_executable(monokakido_extractor_tests
            ${TEST_SOURCES}
    )

    target_link_libraries(monokakido_extractor_tests PRIVATE
            monokakido_core
            monokakido_resource
            monokakido_output
            GTest::gtest
            ZLIB::ZLIB
    )

    include(GoogleTest)
    gtest_discover_tests(monokakido_extractor_tests
            DISCOVERY_TIMEOUT 300
            PROPERTIES LABELS "unit"
    )
endif ()