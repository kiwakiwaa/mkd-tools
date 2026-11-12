# Finish, install and update

[![Each icon in the collection opens a dictionary product.](../assets/guide/collection.png)](../assets/guide/collection.png)

Each icon in the collection opens a dictionary product.

Finish the product, then install its folder in Dictionaries on macOS.

## Publish the files

=== "Python"

    ```python
    dictionary.finish()
    product.finish()
    ```

=== "C++"

    ```cpp
    dictionary.finish().value();
    product.finish().value();
    ```

Finish each dictionary before finishing the product. Dictionary completion checks
required content and references. Product completion publishes the staged files
at `destination`. The generated product can then be opened with
`DictionaryProduct::openAtPath(destination)`.

Destruction does not publish. Abandoning a build removes its staging files.
Writers are used serially, and input strings and byte spans are borrowed only for
the call.

## Check and repair a build

Use `preflight()` to check a build while you are still adding content. For
example, if an article links to `article.css` but you forgot to add that file:

=== "Python"

    ```python
    print(dictionary.preflight().describe())

    dictionary.add_asset("article.css", mkd.Buffer.map("article.css"))
    dictionary.finish()
    product.finish()
    ```

=== "C++"

    ```cpp
    auto report = dictionary.preflight().value();
    std::cerr << report.describe() << '\n';

    auto css = MKD::RetainedSpan::map("article.css").value();
    dictionary.addAsset("article.css", css.span()).value();
    dictionary.finish().value();
    product.finish().value();
    ```

Use `product.preflight()` to check product menus and links between dictionaries
as well. Its report also lists any dictionaries you still need to finish.

`finish()` checks again, so you can add missing data and retry it. Start a new
build to replace XML already written, or after an error while adding or changing
content or writing files.

## Encryption and validation

To encrypt article contents, pass `{.encryptContents = true}` as the second
argument to `product.addDictionary(content, options)` in C++, or
`encrypt_contents=True` to `product.add_dictionary(content, ...)` in Python.

The default creation profile is general search. See [profiles](../reference/search_profiles.md)
for kanwa authoring. Full-text keys must also be supplied explicitly.

Completion checks that:

- Every page has a primary headline and the dictionary has headword search keys.
- Search, headline, link and appendix targets resolve to existing pages or anchors.
- Referenced local assets and configured presentation stylesheets exist.
- Any explicit browsing order contains every page exactly once.

## Validate a finished package

Check the finished folder before installation:

```sh
mkd-tools validate build/Birds
```

This checks local resources, entry links and HTML fragments in registered documents
and their reachable HTML/CSS files. It does not modify the package or fetch URLs.

```sh
# Include unregistered loose HTML files.
mkd-tools validate build/Birds --scan-all-documents

# Make warnings fail the check.
mkd-tools validate build/Birds --warnings-as-errors
```

Exit status is 0 for a valid report, 1 for validation failure, and 2 when the
package directory cannot be inspected. Warnings are advisory by default, including
links to separately installed dictionaries that this package cannot verify.

The same check is available from the library:

=== "Python"

    ```python
    report = mkd.validate_package("build/Birds")
    print(report.describe())
    assert report.valid
    ```

=== "C++"

    ```cpp
    #include <MKD/dictionary/package_validation.hpp>

    auto report = MKD::validatePackage("build/Birds").value();
    std::cerr << report.describe() << '\n';
    ```

Validation checks structure and references. Verify rendering, search and media
playback in Dictionaries as well.

## Install and replace

Quit Dictionaries before installation. Run the CLI against the finished product:

```sh
mkd-tools install build/Birds
```

Build a replacement in another directory with the same final folder name, then:

```sh
mkd-tools install build/next/Birds --replace
```

Installation copies the package and adds local activation for that custom product.
An existing activation is preserved. It accepts packages created by mkd and is
intended for custom dictionaries you are allowed to use.

Keep product and content identifiers stable across updates. If you need links or
bookmarks to survive changed article order, preserve the affected IDs when
[importing content](../reference/authoring_options.md#import-explicit-ids).

Verify search, links, navigation, media and both article presentations in the
Dictionaries app.
