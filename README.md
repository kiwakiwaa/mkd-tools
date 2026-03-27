# mkd-tools

A C++23 toolkit for reading and exporting 物書堂 dictionary resources.

### Building

Requires a C++23 compiler and CMake.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Usage

```
mkd-tools <command> [options]
mkd-tools <dict_id>            # shorthand for 'export <dict_id>'
```

**Commands:** `list`, `info <dict_id>`, `export <dict_id>`, `help`

On macOS, installed dictionaries are detected automatically.\
You can also point to a dictionary directory with `-d <path>`.
```
mkd-tools export <dict_id>
mkd-tools -d /path/to/dicts export <dict_id>
mkd-tools export <dict_id> --only entries --pretty
```

Use `--only` to limit export to specific resource types (`audio`, `entries`, `graphics`, `fonts`, `keystores`, `headlines`). `--pretty` will format XML entry content.

Output goes to `~/Documents/mkd-export` by default, configurable with `-o`.

### macOS receipt app

A lightweight AppKit app is included at `apps/monokakido-receipt-ui`.  Build it with the Xcode generator:

```
cmake -S . -B build-xcode -G Xcode -DMKD_BUILD_CLI=OFF -DMKD_BUILD_RECEIPT_APP=ON
cmake --build build-xcode --target MKDReceiptApp --config Release
```

The built app bundle will be under:

```
build-xcode/apps/monokakido-receipt-ui/Debug/MKDReceiptApp.app
```


### Acknowledgements
This work benefited greatly from the use of [Hopper Disassembler](https://www.hopperapp.com) and [Frida](https://frida.re). 
Their capabilities made the analysis of the dictionary formats substantially more tractable.

> ### Notice
>
> This software was developed by means of independent reverse engineering. No source code, decompiled output, or other copyrightable material has been incorporated. The implementation is an original work, informed solely by observation and analysis of the binary formats. \
> This tool is intended for use exclusively with legitimately purchased dictionaries. It is neither designed nor offered as a means of obtaining or accessing content without proper licence. The author accepts no responsibility for any use of this software that may infringe upon the rights of any third party. \
> Users are solely responsible for satisfying themselves that their use of this software complies with all applicable laws in their jurisdiction, including without limitation those governing reverse engineering, copyright, and the circumvention of technological protection measures.
