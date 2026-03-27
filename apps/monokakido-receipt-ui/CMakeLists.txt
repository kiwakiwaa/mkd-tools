set(APP_NAME MKDReceiptApp)
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(APP_SOURCES
    ${APP_DIR}/AppMain.swift
    ${APP_DIR}/AppDelegate.swift
    ${APP_DIR}/MKDReceiptBridge.mm
)

add_executable(${APP_NAME} MACOSX_BUNDLE ${APP_SOURCES})

set_source_files_properties(${APP_DIR}/MKDReceiptBridge.h PROPERTIES HEADER_FILE_ONLY TRUE)

target_link_libraries(${APP_NAME} PRIVATE mkd)

target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${APP_NAME} PRIVATE
    "-framework AppKit"
    "-framework Foundation"
)

set_target_properties(${APP_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${APP_DIR}/Info.plist
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.kiwakiwaa.mkd.receipt"
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${APP_DIR}/MKDReceiptBridge.h"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${APP_DIR}/MKDReceipt.entitlements
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
)
