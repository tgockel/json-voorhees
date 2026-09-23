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
     - Fixed `parse_index::parse` accepting an object whose first member has a key and a `:` but no value, as in
       `{"a":}`. An object's first member is read by the parser's `{` case rather than by its `,` case, and only the
       latter recorded that the structure then owed a value, so the `}` closed cleanly and the index reported success
       over a tape holding a key and nothing after it. The same shape in any later position already failed.
       `jsonv::parse` rejected the document anyway, since building the tree walks onto that `}` where a value should
       be, which is why nothing noticed -- but anything reading a `parse_index` or a `reader` directly was told the
       source was well-formed. `ast_error::close_after_comma` now covers both ways a structure can be closed while it
       still owes a value, so its description reads "structure closed where a value was required" rather than naming
       a comma which need not be involved (#229).
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
     - Fixed `match_string` passing a raw `char` to `std::isxdigit` when validating the four hex digits of a `\u`
       escape, and `compare_icase` doing the same with `std::tolower`. Both classifiers are defined only over
       `unsigned char` values and `EOF`, so a byte at or above `0x80` -- negative in a signed `char` -- was
       undefined behaviour on the way in. Malformed input is now rejected rather than reaching the classifier at
       all (#211).
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
     - The built-in extractors in `formats::defaults` and `formats::coerce` now read the AST node the reader is
       sitting on instead of a `value` materialised for them, which is what makes the claim above true: these are the
       leaves of every extraction, so this is where the middle man stops being allocated. Extracting a `std::string`
       out of JSON text costs one allocation -- the string handed back -- where it cost four, and every other
       built-in costs none. `std::string_view` is now a view of the source rather than a refusal, since a canonical
       string token *is* the string; the source has to outlive the view, which for a `reader` over text means the
       text. A string the source spelt with escape sequences has no decoded form in it to view and is still refused,
       as is one belonging to a tree the pipeline materialised and is about to free. A bad escape -- a `\uD800` with
       no low surrogate, which the parser accepts because it validates an escape's syntax without decoding it -- is
       now a problem in the extraction's list with a path rather than a `parse_error` thrown out of `extract`. This
       resolves the three `TODO(#150)` markers in `ast.cpp`: the extractors own the policy, and the nodes keep the
       mechanism they always had (#229).
     - Integer extraction reports a literal which does not fit the destination instead of wrapping it. It read
       through `ast_node::integer::value()`, which saturates to the bound of `std::int64_t`, and then narrowed that
       result modularly, so `extract<std::uint8_t>` of `999` produced `231` and `extract<std::int8_t>` of `200`
       produced `-56`. The token is now read against the destination type directly, so both are reported failures
       naming the literal and the type it did not fit. A negative literal is likewise out of range for an unsigned
       destination rather than its two's-complement reinterpretation, which is a compatibility break worth calling
       out: `to_json` writes a `std::uint64_t` above `INT64_MAX` as a negative number, because `value` holds integers
       as `std::int64_t`, and `extract<std::uint64_t>` used to reinterpret that back. It now refuses, so such a value
       no longer round-trips. The document said `-1`, every other JSON reader sees `-1`, and reading it back as
       `18446744073709551615` was two mistakes cancelling.
     - `double` and `float` read the number token with the decimal node's parser, which accepts the integer grammar
       as a subset, so an integer literal beyond `std::int64_t` rounds to the nearest `double` rather than to the
       bound the integer accessor saturates to.
     - All of the above is about what the *source text* says. Extracting from an in-memory `value` reads what the
       `value` holds, which for a literal outside `std::int64_t` is already the saturated number `parse` recorded --
       that is #206, and `value::as_integer` and `ast_node::integer::value()` are unchanged here (#229).
     - `extraction_context::problem_path` is public. An extractor which rejects a value for a reason other than its
       node type -- a number outside the range of what it builds, say -- wants the answer `expect` and `current_as`
       already report through, and had no way to ask for it. Relatedly, a mismatch naming several acceptable node
       types no longer repeats a description they share: expecting a string now reports "when expecting string"
       rather than "when expecting one of string, string" (#229).
     - Fixed `extraction_error` built from an empty `problem_list` leaving `problems()` empty, which its own
       documentation says cannot happen. `path()` and `nested_ptr()` each guarded the empty case and returned a
       static empty value; `problems()` has nothing to fall back on and was missed, so a caller iterating it to
       report what went wrong got nothing while a caller reading `what()` got a description. The list is now
       normalised when the error is built, which gives `problems().size() == 1` and a different `what()` for that
       case (#245).
     - Extraction no longer allocates to say where it is. `extraction_context` names its position with a chain of
       `path_scope` guards living on the C++ stack -- a push is two stores, a pop is one -- and materialises a
       `jsonv::path` only when `path()` is called, which happens only when a problem is recorded. The two places
       which name a position on every element of every document now push one of those guards directly:
       `container_adapter` pushes the element's index and the serialization builder's member loop pushes the key the
       document used. Both previously went through `extraction_context::extract_sub`, which takes its subpath by
       value, so every element built a `std::vector<path_element>` and every member built one more -- and, for a key
       too long for the small-string buffer, two copies of that key, since the `path_element` is copied once into
       the call and again on the way into the vector. Extracting an array of `n` objects with `m` members apiece
       performed `n * (m + 1)` heap allocations for the path vectors alone on a wholly successful extraction, and
       `n * (3m + 1)` in total once the member names stopped fitting in a small string -- all to describe a position
       nothing would go on to ask for. It now performs none. The member loop also stops looking itself up twice: it
       held the iterator its own search returned and then asked `value::at_path` to `count` and `at` the same key
       over again, so each member cost three map lookups where one will do. What a failure reports is unchanged,
       down to the path of a member matched through an `alternate_name`, which is still the key the document used
       rather than the declared one. `extract_sub` itself is unchanged and remains available (#228).
   - Platform
     - `JSONV_DEBUG` is now defined for any Debug configuration rather than only on non-Windows targets. It was
       appended to `CMAKE_CXX_FLAGS_DEBUG` inside an `if(WIN32)/else()` whose Windows half was empty, so an MSVC
       Debug build compiled with `JSONV_DEBUG=0` and ran the test suite's timing loops -- a hundred parses of every
       corpus document -- which is the thing both `CONTRIBUTING.md` and the CI workflow say a Debug build does not
       do. It is now a `$<CONFIG:Debug>` compile definition, which is also the only form a multi-config generator
       can read, since `CMAKE_BUILD_TYPE` is empty there at configure time.
     - Link-time optimization is now enabled per configuration rather than globally. `CMAKE_BUILD_TYPE` was defaulted
       to `Release` whenever it was empty, which is always under a multi-config generator, so a Visual Studio build
       chose LTO from a configuration it was not building and compiled the Debug one `/GL` and `/LTCG`.

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
