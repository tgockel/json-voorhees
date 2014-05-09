1._ Series
==========

*TODO*

0._ Series
==========

[0.3 (future)](https://bitbucket.org/tgockel/json-voorhees/issues?version=0.3)
------------------------------------------------------------------------------

The main focus of this release is access and modification of the low-level parsing and encoding system.

[0.2](https://bitbucket.org/tgockel/json-voorhees/issues?version=0.2)
---------------------------------------------------------------------

The main focus of this release was the unification of the JSON type into `jsonv::value`.
Another major addition is full support for parsing and emitting strings into a proper encoding.

 - 0.2.1: 2014 May 9
    - Adds support for building with Clang (version 3.3 and beyond)

 - 0.2.0: 2014 May 7
    - Elimination of `array` and `object` types in preference of just `value`
    - Added the ability to specify parsing options
    - Use `JSONV_` as the macro prefix everywhere
    - Full support for decoding JSON numeric encodings (`\uNNNN`) as UTF-8 or CESU-8
    - Various fixes for comparison and assignment

[0.1](https://bitbucket.org/tgockel/json-voorhees/issues?version=0.1)
---------------------------------------------------------------------

The original prototype, which allows for parsing input to the JSON AST, manipulation of said AST and eventually encoding
 it as a string.

 - 0.1.1: 2014 April 30
    - Minor parsing performance improvements by batching the string read for `parse_number`
    - Move to GNU Make as the build system
 - 0.1.0: 2014 April 24
