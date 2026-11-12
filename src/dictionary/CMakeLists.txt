set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/authoring/resources/product_information.css")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/authoring/resources/product_information.css" MKD_PRODUCT_INFORMATION_CSS)
configure_file(authoring/resources/product_information_style.hpp.in product_information_style.hpp @ONLY)
target_include_directories(mkd PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_sources(mkd PRIVATE
        navigation/navigation.cpp
        navigation/index_format.cpp
        authoring/navigation_writer.cpp
        navigation/appendix_reader.cpp
        authoring/appendix_writer.cpp
        content/inline_text.cpp
        content/content_markup.cpp
        package/resource_resolver.cpp
        content/css_lexer.cpp
        content/analysis.cpp
        content/css_analysis.cpp
        content/headline_analysis.cpp
        entry_order.cpp
        dictionary.cpp
        dictionary_product.cpp
        dictionary_search.cpp
        package/metadata.cpp
        package/content_info.cpp
        package/paths.cpp
        package/resource_loader.cpp
        search/text_normalize.cpp
        authoring/authoring_diagnostics.cpp
        authoring/prepared_product_files.cpp
        authoring/product_build.cpp
        authoring/reference_index.cpp
        authoring/entry_allocator.cpp
        authoring/asset_writer.cpp
        authoring/product_writer.cpp
        authoring/product_information.cpp
        authoring/product_icon.cpp
        authoring/dictionary_writer.cpp
        authoring/entry_writer.cpp
        search/search_definitions.cpp
        authoring/metadata_preparation.cpp
        package/property_list.cpp
        package/metadata_writer.cpp
        content/html_token.cpp
        content/html_lexer.cpp
        content/document_analysis.cpp
        content/document_checks.cpp
        content/article_analysis.cpp
        authoring/dictionary_build.cpp
        authoring/search_index_builder.cpp
)

target_link_libraries(mkd PUBLIC glaze::glaze)

add_subdirectory(search)
