if (NOT APPLE)
    return()
endif ()

if (CMAKE_GENERATOR MATCHES "Unix Makefiles")
    message(WARNING "Skipping AppKit receipt app: Swift is not supported by the '${CMAKE_GENERATOR}' generator. Use -G Xcode to build the app.")
    return()
endif ()

enable_language(Swift)

add_subdirectory(monokakido-receipt-ui)
