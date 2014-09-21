1._ Series
==========

*TODO*

0._ Series
==========

[0.3](https://github.com/tgockel/json-voorhees/issues?q=milestone%3Av0.3)
-------------------------------------------------------------------------

The main focus of this release is access and modification of the low-level parsing and encoding system.

 - [0.3.0](https://github.com/tgockel/json-voorhees/releases/tag/v0.3.0): 2014 September 21
    - Creates `tokenizer` for easier access to JSON parsing
    - Creates `encode` for customization of JSON output
    - Creates `value::array_view` and `value::object_view` for use in range-based for loops
    - Creates the `make install` recipe (with customization of versioned SOs)
    - Re-write of the parsing system to be stream-based so not everything has to reside in memory at the same time
    - Expose `string_ref` as part of the `encode` and `tokenizer` systems
    - Improved exception-handling in `value`
    - Improved documentation, including automatic Doxygen generation with Travis CI

0.2
---

The main focus of this release was the unification of the JSON type into `jsonv::value`.
Another major addition is full support for parsing and emitting strings into a proper encoding.

 - [0.2.2](https://github.com/tgockel/json-voorhees/releases/tag/v0.2.2): 2014 May 18
    - Adds `std::hash<jsonv::value>`

 - [0.2.1](https://github.com/tgockel/json-voorhees/releases/tag/v0.2.1): 2014 May 9
    - Adds support for building with Clang (version 3.3 and beyond)

 - [0.2.0](https://github.com/tgockel/json-voorhees/releases/tag/v0.2.0): 2014 May 7
    - Elimination of `array` and `object` types in preference of just `value`
    - Added the ability to specify parsing options
    - Use `JSONV_` as the macro prefix everywhere
    - Full support for decoding JSON numeric encodings (`\uNNNN`) as UTF-8 or CESU-8
    - Various fixes for comparison and assignment

0.1
---

The original prototype, which allows for parsing input to the JSON AST, manipulation of said AST and eventually encoding
 it as a string.

 - [0.1.1](https://github.com/tgockel/json-voorhees/releases/tag/v0.1.1): 2014 April 30
    - Minor parsing performance improvements by batching the string read for `parse_number`
    - Move to GNU Make as the build system
 - [0.1.0](https://github.com/tgockel/json-voorhees/releases/tag/v0.1.0): 2014 April 24