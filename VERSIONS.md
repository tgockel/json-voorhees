2._ Series
==========

2.0
---

 - [2.0.0](https://github.com/tgockel/json-voorhees/milestone/12): 2020 March 13
   - Core
     - The side-effect-free public API is now `[[nodiscard]]` (spelled `JSONV_NODISCARD`, which you can define away).
       This covers the predicates and accessors, the constant lookups, `parse`, the `coerce_*` functions and the
       `algorithm.hpp` producers like `merge`, `diff` and `map`. The accessors which create on demand (non-`const`
       `value::path` and `value::operator[]` for an object key) and the mutators are not marked, since discarding those
       is ordinary use. No ABI change (#190).
     - Fixed decimal comparison to use exact ordering instead of a non-transitive epsilon tolerance (#195).
       Signed zeros compare equal; all NaNs compare equal and sort after every non-NaN number.
     - Fixed `value::insert` deep-copying the pair it was handed instead of moving it, which made inserting into an
       object arbitrarily slower than assigning through `operator[]` as the inserted value grew (#152).
     - Fixed `value::insert(hint, node_handle)` ignoring its hint and walking the tree twice.
     - Fixed `object_node_handle` move assignment leaving the source reporting non-empty, so a handle which had
       already given its element away still converted to `true` and returned moved-from contents from `key()` and
       `mapped()`. Self-move assignment is now a no-op instead of dropping the key (#203).
     - Fixed `value::insert(node_handle)` never emptying the handle it consumed, and the overload without a hint
       moving the key and mapped value out even when the key collided -- the handle was left claiming ownership of
       an element it no longer held (#203).
     - Fixed `value::extract` never using `std::map::extract`. The feature probe guarding it asked for a nested
       `node_handle` type, which `std::map` does not have -- it names that type `node_type` -- so the probe was
       always false and every extraction took a fallback which looked the key up a second time despite already
       holding an iterator and copied the key rather than moving it. The probe is gone and `extract` now splices the
       node out directly (#215).
     - `value::insert(first, last)` now hints at the end of the object. An ascending source of unique keys which all
       sort after the object's existing contents -- most importantly an empty object, as in
       `jsonv::object(first, last)` -- now costs amortized constant time per element. Anything else misses the hint
       and keeps the usual logarithmic lookup, including a repeated key: the hint is only taken for a key which sorts
       strictly after the greatest one present, and a repeat does not. As a side effect, it now throws `kind_error`
       on a non-object even when the range is empty, matching `value::insert(std::initializer_list)`.
     - Added `value::emplace`, `value::try_emplace`, and `value::insert_or_assign` for `kind::object`.
     - Changed the backing data type of `kind::array`s to an `std::vector<value>`
     - Fixed `reader::next_structure` aborting the process when called on an exhausted reader. It inspected the
       current node without guarding, and there is no current node once any `next_` function has returned
       `false`, so the resulting exception escaped a `noexcept` frame. It now reports failure by returning
       `false`, matching the rest of the `next_` family (#213).
     - Fixed `reader::next_key` being declared `noexcept` while both its documentation and its implementation
       promised `std::invalid_argument` when called somewhere other than the start of a key. The exception
       escaped a `noexcept` frame, so the documented error was unreachable and ordinary caller error aborted
       the process instead. `next_key` is no longer `noexcept` and the throw is now catchable, as documented.
       Note this changes the function's type, which since C++17 includes the exception specification (#213).
     - Added `reader::next_value`, which steps over the value the reader is on. This is distinct from
       `reader::next_structure`, which leaves the structure the reader is *inside*: on a scalar object
       member the two differ, and using `next_structure` to skip an unwanted member silently consumes the
       rest of the enclosing object. For a `parse_index` source this is a constant-time jump.
     - Added `reader::from_value`, which reads an in-memory `value` directly. The header has always named `value`
       as one of the sources a `reader` accepts, but the implementation behind it was never written: the
       constructor was declared and never defined, so calling it was a link error. It now walks the tree with an
       explicit frame stack, synthesising token text into an arena as it goes, which keeps `extract<T>(const
       value&)` a tree walk instead of a round trip out through the encoder and back through the parser. This is
       a named factory rather than a constructor because `value` converts implicitly from `std::string`, which
       would have made `reader(some_std_string)` ambiguous against the JSON-source overload. Strings and keys
       arrive canonical, since a `value` holds decoded bytes, and a non-finite `kind::decimal` arrives as
       `null` -- both of which match what encoding the same value produces (#224).
     - Corrected `reader::next_value`'s documentation, which said that on a structure it lands on the matching
       close token. It lands on the token *after* it, as the worked example beside the clause and the
       `parse_index` implementation both always did. A caller who believed the prose and added a `next_token`
       to step off the close would have skipped the following key.
     - `reader::expect` and the new `ast_node::expect` report a type mismatch by returning
       `std::expected<void, ast_node_type>` carrying the type actually found, rather than throwing
       `extraction_error` and building a message. Which node types are acceptable is a question about the JSON
       source, not about the correctness of the program asking, so the caller decides whether a mismatch is an
       error -- and trying a type no longer costs a throw. `reader::current_as` returns
       `std::expected<TAstNode, ast_node_type>` in the same way. Expecting an empty list of types still throws
       `std::invalid_argument`, since that is a mistake in the calling code. This also drops `reader`'s only
       dependency on `serialization.hpp` (#223).
     - Fixed `reader::current_as` failing to compile for every node type. It is declared `const` but called
       `reader::expect`, which was not, so instantiating it was a hard error -- on a `const reader` or any other.
       Nothing in the tree instantiated the template, so the mismatch was never diagnosed; there is now a test
       which does (#222).
     - Fixed `reader` failing to compile for any consumer which moved one. Both move operations were declared
       inline `= default` while `reader::impl` is only forward-declared in the public header, so the
       `std::unique_ptr` deleter was instantiated against an incomplete type in the caller's translation unit.
       They are now declared in the header and defaulted in `reader.cpp`, which is what the destructor already
       did. The issue reported move assignment; move construction was broken the same way, since the defaulted
       constructor still needs a destructible member for the exception path (#240).
     - Added `parse_index::iterator::skip_subtree`, which steps over a whole object or array in constant time.
       The index already recorded where each structure ends when it parsed the matching close token, but
       nothing surfaced it, so skipping a value meant walking every node inside it.
     - Fixed a failed parse leaving a structure's recorded end and element count uninitialized. Those slots are
       only written when the matching close token arrives, so both a structure which never closed (`{`,
       `[ 1, 2`) and one which closed but was followed by trailing input (`[]x`) produced an
       `object_begin`/`array_begin` whose `element_count()` read indeterminate memory. Passing that count to
       `extract_tree` could reserve an arbitrary amount of memory.
     - Major refactoring of the parsing from the pull-based `tokenizer` into the flat-structured `parse_index`
     - Removed support for more lax parser settings -- a parsed `parse_index` has been validated
     - Parsing options and errors (`parse_options` and `parse_error`) have been split into parse-specific options
       (things like allowing ECMAScript-style block comments `/* ... */`) and extraction-specific options and errors
       (things like what to do if an object has the same key).
   - Serialization
     - Extraction to C++ objects now occurs directly from `parse_index` instead of going through the `value` middle man,
       saving time and memory
     - `extract_options::on_error` and `max_failures` are now honoured. Both have been documented since the type was
       introduced and neither was ever consumed: `extraction_context` held no `extract_options`, so there was nowhere
       to pass one, and the only reader of the type was `parse_index::extract_tree` for `on_duplicate_key`. The
       context now carries options, and `collect_all` keeps extracting past a problem wherever a composite knows
       where to resume -- an array's next element, an object's next key -- so one bad member no longer hides every
       problem after it. Collecting gathers diagnostics and does not produce partially-extracted objects: an
       extraction which recovered from anything still throws, carrying what it found. A failure with no enclosing
       composite to resume into ends extraction whatever the mode, which is why `extraction_context::recover` is
       asked by the loop rather than decided for it. `max_failures` is the threshold extraction stops at rather than
       a cap on the reported list -- a failure which reports several problems at once is taken whole -- and a limit
       of `0` or `1` makes the first problem the last, which is `fail_immediately` in all but name (#227).
     - Fixed `extraction_error` built from an empty `problem_list` leaving `problems()` empty, which its own
       documentation says cannot happen. `path()` and `nested_ptr()` each guarded the empty case and returned a
       static empty value; `problems()` has nothing to fall back on and was missed, so a caller iterating it to
       report what went wrong got nothing while a caller reading `what()` got a description. The list is now
       normalised when the error is built, which gives `problems().size() == 1` and a different `what()` for that
       case (#245).

1._ Series
==========

1.4
---

 - [1.4.0](https://github.com/tgockel/json-voorhees/releases/tag/v1.4.0): 2019 September 13
   - Core
     - Removes use of variable length arrays in character conversion code
   - Serialization
     - Adds support for required fields
   - Platform
     - Adds support for newer Ubuntu, Fedora, and openSUSE flavors

1.3
---

 - [1.3.0](https://github.com/tgockel/json-voorhees/releases/tag/v1.3.0): 2018 July 13
   - Core
     - Expands testing for strings with the Big List of Naughty Strings
   - Serialization
     - Return old `formats` value when using `formats::set_global`
     - Allow duplicate types when building formats
     - Convenience methods for checking formats references while building a `formats` instance
   - Platform
     - Easy way to build packages for all platforms

1.2
---

 - [1.2.1](https://github.com/tgockel/json-voorhees/releases/tag/v1.2.1): 2018 June 13
   - Workaround an issue in GCC 8.1.0 where unicode points would be incorrectly parsed at `-O3`.

 - [1.2.0](https://github.com/tgockel/json-voorhees/releases/tag/v1.2.0): 2018 April 13
   - Core
     - Parsing performance improvements -- about twice as fast as 1.1
     - Eliminated usage of regular expressions
     - No more reliance on `<codecvt>` or Boost.Locale for UTF conversions
   - Platform
     - Expanded support for Debian, Fedora, OpenSUSE, and Arch Linux
   - Serialization
     - Added support for type-level defaults -- `type_default_on_null` and `type_default_value`
     - Default `formats` now support going to and from `string_view`
     - `extraction_context` now supports extraction based on `std::type_info`

1.1
---

 - [1.1.1](https://github.com/tgockel/json-voorhees/releases/tag/v1.1.1): 2016 January 22
   - Cover odd integer types on OSX

 - [1.1.0](https://github.com/tgockel/json-voorhees/releases/tag/v1.1.0): 2015 November 13
   - Less bad Windows support (and true MSVC 2015)
   - Easier functions for case-insensitive comparisons
   - Faster parsing
   - Faster comparisons
   - Full checking for proper character encodings
   - Support for `std::wstring` access methods on a `value`
   - Serialization DSL expansions
     - Enumeration type support
     - Simple DSL::extend function
     - Support for extracting and encoding polymorphic types

[1.0](https://github.com/tgockel/json-voorhees/issues?q=milestone%3Av1.0)
-------------------------------------------------------------------------

Stabilizing the API and finalizing things for a release.

 - [1.0.0](https://github.com/tgockel/json-voorhees/releases/tag/v1.0.0): 2015 March 13
   - Moves to CMake as the build system
   - Greatly improves the speed of the parser
   - Removes `nullptr` as a type for null in preference of `jsonv::null`
   - Better support for MSVC


0._ Series
==========

[0.5](https://github.com/tgockel/json-voorhees/issues?q=milestone%3Av0.5)
-------------------------------------------------------------------------

The focus of this release is extensible serialization between JSON values and C++ types.

 - [0.5.1](https://github.com/tgockel/json-voorhees/releases/tag/v0.5.1): 2015 February 17
   - Creates `null` to be used in place of `nullptr`
   - Adds the ability to set the default size of `tokenizer`'s buffer
   - Fixes issue in parsing where string divisions on buffer boundaries would yield incorrect results (and potentially
     crash)

 - [0.5.0](https://github.com/tgockel/json-voorhees/releases/tag/v0.5.0): 2015 February 13
   - Creates `formats`, `extractor`, `serializer` and `adapter` classes
   - Creates the `extract` and `to_json` free functions for conversion
   - Creates the Serialization Builder DSL for easily making type adapters
   - Adds support for compiling with GCC and Clang on Windows with [Cygwin](https://www.cygwin.com/)
   - Adds experimental support for Microsoft Visual Studio 14 (CTP 5)
   - Adds the `value::is_X` convenience functions for checking `kind` values.

[0.4](https://github.com/tgockel/json-voorhees/issues?q=milestone%3Av0.4)
-------------------------------------------------------------------------

The focus of this release was the creation of tools to traverse and manipulate the JSON AST.

 - [0.4.1](https://github.com/tgockel/json-voorhees/releases/tag/v0.4.1): 2015 January 19
   - Adds reverse iteration to `array_view` and `object_view`
   - Adds the `map` algorithm
   - Adds the `diff` algorithm
   - Adds `value::count_path`
   - Fixes issue in parsing where large inputs would buffer incorrectly

 - [0.4.0](https://github.com/tgockel/json-voorhees/releases/tag/v0.4.0): 2015 January 13
   - Creates a generic visitor system for `jsonv::value`
   - Creates the `path` system, which is a very simplified version of JSONPath
   - Creates the `merge` and `traverse` families of algorithms
   - Creates the *coerce* library for non-strict conversion
   - Extends `array_view` and `object_view` to have *owning* versions, so calling `value::as_array` and
     `value::as_object` on rvalues works as you would expect (safely)
   - Make `kind::decimal` and `kind::integer` equivalent in almost all cases
   - Various bugfixes

[0.3](https://github.com/tgockel/json-voorhees/issues?q=milestone%3Av0.3)
-------------------------------------------------------------------------

The main focus of this release is access and modification of the low-level parsing and encoding system.

 - [0.3.1](https://github.com/tgockel/json-voorhees/releases/tag/v0.3.1): 2014 September 27
   - Greatly expands the flexibility of `parse_options`
   - Adds all the tests from [JSON_Checker](http://json.org/JSON_checker/)
   - Adds a `parse` that takes a `string_ref`, which unifies the `const char*` and `std::string` overloads
   - Changes the installer to put header files inside of a `jsonv` folder
   - Adds Arch Linux `PKGBUILD` to the `installer` folder
   - Fixes issue with incorrectly escaping `\\\"`
   - Fixes issue with `value::compare(const value&) const` returning non-zero when two decimal kinds were within the
     epsilon value (and `operator==` would have returned `true`)

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
