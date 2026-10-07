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
     - Fixed `std::hash<value>` giving different hashes to an integer and a decimal which compare equal. An
       unordered container could hold both `2` and `2.0` as separate keys, and looking one up by the other missed;
       arrays and objects holding such numbers failed the same way. Both numeric kinds now hash through
       `as_decimal`, so an integer no longer hashes like `std::hash<std::int64_t>` of it (#198).
     - Fixed comparison between an integer and a decimal converting the integer to `double`, which rounds past 2^53.
       `9007199254740993` compared equal to `9007199254740992.0`, which compared equal to `9007199254740992`, while the
       two integers differed -- so `std::set`, `std::map` and `std::sort` over mixed numbers lost their ordering
       guarantees. The two now compare by exact numeric value through the new `compare_traits::compare_integer_decimal`.
       A traits type passed to `compare` which does not derive from `compare_traits` must provide it, and a custom
       `compare_decimals` no longer sees mixed pairs (#199).
     - Fixed `value::insert` deep-copying the pair it was handed instead of moving it, which made inserting into an
       object arbitrarily slower than assigning through `operator[]` as the inserted value grew (#152).
     - Fixed `value::insert(hint, node_handle)` ignoring its hint and walking the tree twice.
     - Fixed `object_node_handle` move assignment leaving the source reporting non-empty, so a handle which had
       already given its element away still converted to `true` and returned moved-from contents from `key()` and
       `mapped()`. Self-move assignment is now a no-op instead of dropping the key (#203).
     - Fixed `value::insert(node_handle)` never emptying the handle it consumed, and the overload without a hint
       moving the key and mapped value out even when the key collided -- the handle was left claiming ownership of
       an element it no longer held (#202, #203).
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
       explicit frame stack, synthesising token text into an arena as it goes, which keeps `deserialize<T>(const
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
       `deserialization_error` and building a message. Which node types are acceptable is a question about the JSON
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
     - Added `reader::validate`, which throws `parse_error` if the reader is over JSON text which did not parse --
       the question `parse_index::validate` answers, asked of a reader. Parsing never throws: a malformed document
       produces a tape which stops at an `error` node, and a reader walks it as far as it goes, so there was no way
       to ask one what was wrong with the rest. A reader over a `value` has nothing to parse and never throws. Also
       added `reader::owns_source`, which is `true` for a reader made from a `std::string` rvalue or by
       `from_value(value&&)` -- the two which keep their source alive only as long as the reader (#232).
     - Added `reader::current_type`, which is `current().type()` without the node. A reader over a `value` has to
       synthesise token text before it can hand out an `ast_node` for a number, a string or a key, and a loop asking
       only whether it has reached the `]` has no use for it. `reader::expect` now asks this way, as do the built-in
       adapters and the serialization builder (#238).
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
     - Fixed a document which ends while an array or object is still open being reported as a mismatched close.
       `[1, 2` failed with "mismatched closing character", although it holds no closing character at all, while `{`
       failed with "input ended unexpectedly", so which of the two a truncated document got depended on where it was
       cut. A deserialization from one carried the same message. The parser closed the document before looking at what
       was still open. It now reports `ast_error::unexpected_eof` at the end of the input, and the tape stops at the
       `error` node rather than carrying a `document_end` for a document which never ended (#262).
     - Fixed `parse_index::parse` accepting a value which follows another with no `,` between them. `[1 2]` parsed as
       a two-element array whose `element_count()` was 1, since that counts commas. At the top level, `5,` parsed as
       `5`, and `5 6` parsed into a tape `jsonv::parse` could not build a tree from, so it threw
       `std::invalid_argument` rather than `parse_error`. Inside an array or object this now fails with the new
       `ast_error::expected_comma`, and at the top level with `ast_error::expected_eof`, as `[1] 2` already did.
       `{"a": 1 "b": 2}`, which failed with "unexpected token" at the `:` after `"b"`, now reports the missing `,` at
       the `"b"` itself (#261).
     - Fixed `parse_index::parse` under-allocating its buffer for an `initial_buffer_capacity` near `SIZE_MAX`. The
       size in bytes wrapped, so a small allocation claimed the whole requested capacity and the first write ran off
       its end; for some values the allocation was smaller than the buffer's own header, and writing that corrupted
       the heap before parsing began. A capacity whose size cannot be represented now throws `std::length_error`, as
       `std::vector::reserve` does past `max_size()`. One which can be represented but not allocated still throws
       `std::bad_alloc` (#221).
     - Major refactoring of the parsing from the pull-based `tokenizer` into the flat-structured `parse_index`
     - Removed support for more lax parser settings -- a parsed `parse_index` has been validated
     - Removed `parse_options::complete_parse`. It let 1.x read one document after another off a `tokenizer`, but the
       2.0 parser never read it: every parse takes the whole of its input as one document, and anything after the
       value fails with `ast_error::expected_eof` (#261).
     - Parsing options and errors (`parse_options` and `parse_error`) have been split into parse-specific options
       (things like allowing ECMAScript-style block comments `/* ... */`) and deserialization-specific options and
       errors (things like what to do if an object has the same key).
     - `parse_options` refuses comments by default. RFC 8259 has no comments, but a default-constructed
       `parse_options` accepted `/* ... */` anywhere whitespace may go, so `parse`, `parse_index::parse`, a `reader`
       over text and `deserialize<T>` from text all read a dialect of JSON unless told otherwise. They now read the
       standard, and `parse_options().comments(true)` is how to read configuration files and other input which
       uses comments. This is a behavioural break for anyone relying on the old default: text with a comment in it
       which used to parse now fails to. `create_strict()` refused comments already and is unchanged (#186).
     - Fixed `parse_options::comments` refusing a comment in three places whitespace may go: before an object's key,
       between a key and its `:`, and after the top-level object or array. `{/*c*/"a":1}` failed with "expected a
       string" and `{} /*c*/` with "extra characters in input", while a comment beside a value parsed. Only the
       parser's main loop skipped comments, and it reads a key, its `:` and whatever follows the document on its own.
       A malformed comment in one of those places now reports "invalid comment block" (#280).
     - Fixed `match_string` passing a raw `char` to `std::isxdigit` when validating the four hex digits of a `\u`
       escape, and `compare_icase` doing the same with `std::tolower`. Both classifiers are defined only over
       `unsigned char` values and `EOF`, so a byte at or above `0x80` -- negative in a signed `char` -- was
       undefined behaviour on the way in. Malformed input is now rejected rather than reaching the classifier at
       all (#211).
     - Fixed the parser accepting ill-formed UTF-8 in strings: the 5- and 6-byte forms RFC 3629 removed, overlong
       encodings such as `\xc0\x80` for U+0000, UTF-16 surrogates written as raw bytes, and codepoints above
       U+10FFFF. `match_string` checked only that a lead byte was followed by the right number of continuation
       bytes, so `"\xed\xa0\x80"` parsed while `"\ud800"` -- the same codepoint, escaped -- did not. Nor did
       `utf8_strict` help, since its only extra check is for unprintable ASCII. The parser, the string decoder
       and the encoder now share one definition of well-formed UTF-8, so the encoder no longer rewrites an
       ill-formed string into a different one (`\xc0\x80` as `\u0000`) or into one the parser refuses
       (`\xed\xa0\x80` as `\ud800`); it replaces each byte with a numeric escape, as it already did for other
       malformed input. The wide-string conversions agree with it: `as_wstring` now refuses an overlong encoding,
       and a `std::wstring` holding an unpaired low surrogate throws `std::range_error` when made into a `value` or
       used as a key, rather than being stored as a surrogate's UTF-8 bytes (#207).
     - Fixed the `std::wstring` conversions truncating each code unit to 16 bits before checking it where `wchar_t` is
       32 bits, as on Linux and macOS. A single unit of `0x1f600` was stored as U+F600, `0x10000` as U+0000, and a high
       surrogate followed by `0x1de00` came out as a valid U+1F600, since the second unit was cut down to a low
       surrogate before the pair was checked. A `std::wstring` is UTF-16 on every platform -- `as_wstring` already
       writes surrogate pairs into a 32-bit `wchar_t` -- so a unit which is negative or above `0xffff` now throws
       `std::range_error` when made into a `value` or used as a key (#271).
     - Fixed `ostream_encoder::ensure_ascii` being declared but never defined, so calling it failed to link and the
       UTF-8 passthrough it controls was out of reach. With it off, well-formed UTF-8 is now written out as it is,
       and `ostream_pretty_encoder` follows it too. Control characters are still escaped: the passthrough used to
       treat a lone ASCII byte as well-formed UTF-8 and write U+0000 through U+001F out raw, which is not JSON
       (#163, #273).
     - Fixed an integer literal beyond the range of 64 bits parsing as the bound it was clamped to. A magnitude above
       `UINT64_MAX` saturated to that value and one below `INT64_MIN` to that one, so `18446744073709551616` and
       `12345678901234567890123` both parsed to `-1` -- indistinguishable from `18446744073709551615` -- with nothing
       to say the number had been lost. `parse` now reads such a literal as the nearest `double`, which is what the
       `double` deserializer already made of it, so it is a `kind::decimal`: `as_integer` refuses it and `as_decimal`
       has it. `ast_node::integer::value()` has no `double` to give and throws `std::invalid_argument` instead, and
       so does `parse` for a literal no `double` holds either, as it already did for `1e400`. A literal from 2^63
       through `UINT64_MAX` still keeps its bits as a negative `std::int64_t`. `coerce_integer` of a string holding
       one of these now clamps as it does for the same number written as a decimal. A string holding a number with no
       finite `double` at all -- that long an integer, or `1e400` -- is one `can_coerce` answers `false` for and
       `coerce_integer` and `coerce_decimal` refuse with `kind_error`, rather than letting out the
       `std::invalid_argument` `parse` throws for it (#206).
     - `coerce_integer` and `coerce_decimal` now read a string as a number directly rather than handing it to `parse`,
       so `parse_options` no longer decides what a string may say. It has to hold a single RFC 8259 number, optionally
       surrounded by JSON whitespace, and nothing else, so a comment is refused however `parse_options` is set. An
       integer string from 2^63 through `UINT64_MAX` is now treated as a number too large for `std::int64_t`, like any
       other: `coerce_integer` clamps it to the maximum and `coerce_decimal` returns its nearest `double`. Both used to
       see the negative `std::int64_t` the parser keeps its bits in, so `"18446744073709551615"` coerced to `-1` (#193).
     - The encoder writes `/` as it is rather than as `\/`. RFC 8259 allows the escape, but only a quote, a backslash
       and the control characters must be escaped; the encoder only wrote `\/` because it shared one table of escapes
       with the decoder. The decoder still reads `\/`. Encoded text containing a `/` changes, but it means the same
       thing to any JSON parser.
     - Fixed `to_string(path)` writing paths `path::create` could not read back, or read back as a different path. A key
       in brackets was written without escaping, so `a"b` came out as unparseable `["a"b"]` and `a\b` as `["a\b"]`,
       which reads back as `a`, a backspace and `b`. Keys are now written as JSON strings, with well-formed UTF-8 left
       as it is. The choice between `.key` and `["key"]` now follows the parser's identifier grammar rather than
       `std::isalnum`, so a key starting with a digit (`["123"]`, which was `.123`) gets brackets and one with `_` or
       `$` (`._id`, which was `["_id"]`) no longer needs them. That also stops passing a possibly negative `char` to
       `std::isalnum`. An array index is written without the stream's digit grouping, which could make `[1000]` into
       `[1,000]`, and `path::create` refuses an index too large for `std::size_t` with `std::invalid_argument` rather
       than saturating it. `path::create(to_string(p)) == p` now holds for every path whose keys are well-formed UTF-8.
       Also fixed the escape lookup behind the encoder and decoder reading one entry past the end of its table for a
       character which sorts after every escape, such as any lowercase letter being encoded or the `u` of a `\u`
       escape being decoded. What it read was never used, so no output changes (#194).
     - `ostream_pretty_encoder` no longer flushes its stream at the end of every line. It wrote line breaks with
       `std::endl`, so pretty output to a file or a socket paid for a write per line. The output itself is unchanged;
       flush the stream yourself if you need it to have arrived (#188).
     - `jsonv::optional<T>` is still `std::optional<T>` for every `T` that takes, and is now also defined for a
       reference. `std::optional<T&>` is C++26 (P2988), and neither MSVC's standard library nor Apple's has it yet, so
       `jsonv::optional<T&>` is a stand-in with its interface: one pointer, trivially copyable, rebound rather than
       assigned through, and refusing to bind to a temporary. `reader::current_value` returns one rather than a
       `const value*`, empty where there is no `value` to lend -- on a reader over text, or on a key or a closing token.
       Once every supported toolchain has `std::optional<T&>`, `jsonv::optional` is `std::optional` throughout.
       `jsonv::nullopt` is gone; `std::nullopt` works for both.
     - Added `writer`, the push-side mirror of `reader`: a cursor which writes a JSON token sequence into any
       `encoder` -- `object_begin`, `key`, `integer`, `array_end` and the rest, named after `ast_node_type` -- or into
       a `std::ostream` through a compact `ostream_encoder` it owns. The writer owns the grammar and the punctuation.
       A key outside an object, a value where a key is due, a mismatched end or any call on a moved-from writer throws
       before anything reaches the sink, and the delimiters between elements and between members are the writer's to
       write, so an `encoder` only ever sees a sequence of hook calls which spells a valid document. `current_path()`
       names the slot the next token fills and comes off the writer's own frame stack, so unlike `reader::current_path`
       over text it is cheap. `writer::write(const value&)` and `encoder::encode` share one walk, so an `encoder`
       subclass, the pretty printer included, sees exactly the sequence of hook calls it saw before (#319).
     - Added `value_encoder`, an `encoder` which builds a `value` from the tokens it is given: the sink for a document
       produced token by token through a `writer` but wanted as a tree, and the mirror of `reader::from_value`.
       `std::move(sink).take()` hands the document out and leaves the encoder ready for another; it throws
       `std::logic_error` while a structure is still open or when nothing has been written, since a JSON document is
       never empty. A key which repeats within an object keeps the value written last, as `parse` does by default, and
       a second root written before `take` replaces the first (#320).
     - A `value` built from a `std::string_view`, a `const char*` or a wide string now copies the text once rather
       than twice. Those constructors built a temporary `std::string` and then copied it again into the node, which
       is the path every string `parse` produces takes (#336).
   - Serialization
     - Deserialization to C++ objects now occurs directly from `parse_index` instead of going through the `value` middle
       man, saving time and memory. The `benchmark/deserialize/` rows in `jsonv-tests` measure it: on
       `citm_catalog.json`, deserializing a `std::vector` of DSL-described records from the text takes a little over
       half as long as `parse` followed by `deserialize` from the result, and a type which reads two of each record's
       nine members takes under a quarter as long, since the members it skips are never built (#234).
     - `deserializer::deserialize` reads from a `reader` instead of a `value`. It is now
       `deserialize(deserialization_context&, reader&, void*) const` and returns `std::expected<void, ast_node_type>`:
       on success the reader is left one past the value which was read, and on failure nothing has been built, what went
       wrong is recorded on the context, and the `ast_node_type` carried is the type actually found when a mismatch was
       the trouble. This is the interface everything else in this section is built on, and a source break for anything
       implementing `deserializer` directly -- `get_type` is also `noexcept` now, so an override has to be too.
       `adapter_for<T>::create` and `deserializer_for<T>::create` change the same way, taking
       `(deserialization_context&, reader&)` and returning `std::expected<T, ast_node_type>`. An adapter written against
       the old `create(const deserialization_context&, const value&)` keeps its body by deriving from the new
       `value_adapter_for<T>` instead and dropping the `const` from its context: that `create` is handed the value read
       into a `value`, and so keeps paying for the tree the reader exists to avoid (#226).
     - `make_deserializer`, `make_adapter` and `deserializer_construction` take the reader forms alongside the old ones.
       A function may be called as `(deserialization_context&, reader&)` or `(reader&)` as well as
       `(deserialization_context&, const value&)` or `(const value&)`, and may return the deserialized type or a
       `std::expected` of it; a deserializing constructor may take `(reader&, deserialization_context&)` or `(reader&)`
       as well as the `value` forms. The `value` forms are handed the subtree read into a `value` by the new
       `read_value` -- or, when the reader was made by `reader::from_value`, the caller's own `value`, lent by the new
       `reader::current_value` -- so they keep compiling and keep paying for the tree they always built. What a function
       deserializes is deduced from what it returns, so `make_deserializer` and `make_adapter` are a single overload
       each (#226).
     - `deserialization_context` is mutable and no longer copyable: problems are recorded on it, through `problem`, and
       recording one changes it. Everything which deserializes through one therefore takes it by non-`const` reference
       -- a `deserializer`, a deserializing constructor, and the callbacks able to deserialize: `polymorphic_adapter`'s
       match predicates and the serialization builder's `pre_deserialize`, `post_deserialize`, `on_unknown_members`,
       `default_value` and `type_default_value`. A callable which declares its context `const` still binds, and only has
       to change if it deserializes through it. `deserialization_context::deserialize<T>(reader&)` reports failure by
       returning a `std::expected` rather than by throwing, while `deserialize<T>(const value&)` still throws. `expect`
       and `current_as` check the node a reader is on and record a mismatch with the "Read node of type X when expecting
       Y" message which `reader::expect` no longer builds (#226).
     - `serialization.hpp` is split into `serialization/formats.hpp`, `context.hpp`, `deserialize.hpp`, `serializer.hpp`
       and `adapter.hpp`. It still includes all of them, so no `#include` has to change. In the move, `context_base`
       became `context` -- the base of `deserialization_context` and `serialization_context` -- and `context::version`
       returns `const std::optional<jsonv::version>&`, with both contexts taking a `std::optional<jsonv::version>` which
       defaults to `std::nullopt`. A default-constructed `jsonv::version` is `0.0`, and every caller had to know that it
       meant "unspecified" rather than "version zero"; `std::nullopt` now says the first and `jsonv::version()` the
       second. That is a behaviour change for a context created with `jsonv::version()` on purpose, which the
       serialization builder's `since`, `until`, `after` and `before` now compare against `0.0` rather than treating as
       unversioned (#225).
     - Deserializing from an in-memory `value` no longer formats every number into text. A reader over a `value`
       synthesised a token for each scalar as soon as anything asked what it was on, which `container_adapter` and the
       builder's member loop did for every element, and the deserializers then either ignored the text -- a `double` was
       already read from the `value` -- or parsed it straight back. They now ask `reader::current_type` and read the
       lent `value`, so the token is built only for a deserializer which asks for the node itself. Deserializing the
       coordinates of `canada.json` from a parsed `value` takes about a sixth less time, which puts it below
       deserializing them from the text, and `parse` followed by `deserialize` gains the same few milliseconds (#238).
     - `deserialize_options::on_error` and `max_failures` are now honoured. Both have been documented since the type was
       introduced and neither was ever consumed: `deserialization_context` held no `deserialize_options`, so there was
       nowhere to pass one, and the only reader of the type was `parse_index::extract_tree` for `on_duplicate_key`. The
       context now carries options, and `collect_all` keeps deserializing past a problem wherever a composite knows
       where to resume -- an array's next element, an object's next key -- so one bad member no longer hides every
       problem after it. Collecting gathers diagnostics and does not produce partially-deserialized objects: a
       deserialization which recovered from anything still throws, carrying what it found. A failure with no enclosing
       composite to resume into ends deserialization whatever the mode, which is why `deserialization_context::recover`
       is asked by the loop rather than decided for it. `max_failures` is the threshold deserialization stops at rather
       than a cap on the reported list -- a failure which reports several problems at once is taken whole -- and a limit
       of `0` or `1` makes the first problem the last, which is `fail_immediately` in all but name (#227).
     - Fixed `collect_all` losing everything after a structure which failed to read out of text. Materialising a
       structure -- for the `value` deserializer, for an adapter on the `value` bridge, or for `coerce` to turn into a
       string -- walks the cursor into it, and a scalar inside which could not be read, such as a number no `double`
       holds or an escape which does not decode, left the cursor there. The array around it then took the
       structure's own close for its own, so every element after it went unread and unreported. `read_value` now
       walks the rest of a structure which fails part-way through, leaving the cursor one past it as success does,
       and the new `read_value(deserialization_context&, reader&)` also tells the context that the value is behind the
       cursor, so a composite recovering from it resumes at the next sibling. The built-in deserializers use it.
     - The built-in deserializers in `formats::defaults` and `formats::coerce` now read the AST node the reader is
       sitting on instead of a `value` materialised for them, which is what makes the claim above true: these are the
       leaves of every deserialization, so this is where the middle man stops being allocated. Deserializing a
       `std::string` out of JSON text costs one allocation -- the string handed back -- where it cost four, and every
       other built-in costs none. `std::string_view` is now a view of the source rather than a refusal, since a
       canonical string token *is* the string; the source has to outlive the view, which for a `reader` over text means
       the text. A string the source spelt with escape sequences has no decoded form in it to view and is still refused,
       as is one belonging to a tree the pipeline materialised and is about to free. A bad escape -- a `\uD800` with no
       low surrogate, which the parser accepts because it validates an escape's syntax without decoding it -- is now a
       problem in the deserialization's list with a path rather than a `parse_error` thrown out of `deserialize`. This
       resolves the three `TODO(#150)` markers in `ast.cpp`: the deserializers own the policy, and the nodes keep the
       mechanism they always had (#229).
     - Integer deserialization reports a literal which does not fit the destination instead of wrapping it. It read
       through `ast_node::integer::value()`, which saturated to the bound of `std::int64_t`, and then narrowed that
       result modularly, so `deserialize<std::uint8_t>` of `999` produced `231` and `deserialize<std::int8_t>` of `200`
       produced `-56`. The token is now read against the destination type directly, so both are reported failures naming
       the literal and the type it did not fit. A negative literal is likewise out of range for an unsigned destination
       rather than its two's-complement reinterpretation, which is a compatibility break worth calling out: `to_json`
       writes a `std::uint64_t` above `INT64_MAX` as a negative number, because `value` holds integers as
       `std::int64_t`, and `deserialize<std::uint64_t>` used to reinterpret that back. It now refuses, so such a value
       no longer round-trips. The document said `-1`, every other JSON reader sees `-1`, and reading it back as
       `18446744073709551615` was two mistakes cancelling.
     - `double` and `float` read the number token with the decimal node's parser, which accepts the integer grammar
       as a subset, so an integer literal beyond `std::int64_t` rounds to the nearest `double` rather than being
       wrapped or refused as it is by the integer accessor.
     - All of the above is about what the *source text* says. Deserializing from an in-memory `value` reads what the
       `value` holds, which for a literal from 2^63 through `UINT64_MAX` is the wrapped, negative number `parse`
       recorded (#229).
     - `deserialization_context::problem_path` is public. A deserializer which rejects a value for a reason other than
       its node type -- a number outside the range of what it builds, say -- wants the answer `expect` and `current_as`
       already report through, and had no way to ask for it. Relatedly, a mismatch naming several acceptable node types
       no longer repeats a description they share: expecting a string now reports "when expecting string" rather than
       "when expecting one of string, string" (#229).
     - Fixed `deserialization_error` built from an empty `problem_list` leaving `problems()` empty, which its own
       documentation says cannot happen. `path()` and `nested_ptr()` each guarded the empty case and returned a
       static empty value; `problems()` has nothing to fall back on and was missed, so a caller iterating it to
       report what went wrong got nothing while a caller reading `what()` got a description. The list is now
       normalised when the error is built, which gives `problems().size() == 1` and a different `what()` for that
       case (#245).
     - Deserialization no longer allocates to say where it is. `deserialization_context` names its position with a chain
       of `path_scope` guards living on the C++ stack -- a push is two stores, a pop is one -- and materialises a
       `jsonv::path` only when `path()` is called, which happens only when a problem is recorded. The two places which
       name a position on every element of every document now push one of those guards directly: `container_adapter`
       pushes the element's index and the serialization builder's member loop pushes the key the document used. Both
       previously went through `extraction_context::extract_sub`, which takes its subpath by value, so every element
       built a `std::vector<path_element>` and every member built one more -- and, for a key too long for the
       small-string buffer, two copies of that key, since the `path_element` is copied once into the call and again on
       the way into the vector. Deserializing an array of `n` objects with `m` members apiece performed `n * (m + 1)`
       heap allocations for the path vectors alone on a wholly successful deserialization, and `n * (3m + 1)` in total
       once the member names stopped fitting in a small string -- all to describe a position nothing would go on to ask
       for. It now performs none. The member loop also stops looking itself up twice: it held the iterator its own
       search returned and then asked `value::at_path` to `count` and `at` the same key over again, so each member cost
       three map lookups where one will do. What a failure reports is unchanged, down to the path of a member matched
       through an `alias`, which is still the key the document used rather than the declared one. `extract_sub`
       itself is removed, by #232 (#228).
     - `container_adapter`, `optional_adapter` and `wrapper_adapter` read the reader directly instead of a `value`
       materialised for them. A `std::vector<my_type>` built the whole array as a `value` and then deserialized each
       element out of it, so a large array was built twice; it is now walked once. The array's opening token carries how
       many elements follow it, so a container which can be told its size is told it rather than doubling its way there
       -- the count is bounded by what is on the tape even when the parse which produced it failed part-way through,
       which is what makes it safe to hand to `reserve`. This is a source break for anyone deriving from one of the
       three and overriding `create(deserialization_context&, const value&)`: the base is now `adapter_for` and the hook
       takes a `reader`. `polymorphic_adapter` followed in #236 and `enum_adapter` in #237 (#230).
     - `std::vector<std::string_view>` deserialized from JSON text is a view of the source rather than a refusal. It was
       refused for a structural reason rather than a semantic one: the container materialised the whole array and every
       element then saw `deserialization_context::source_is_temporary`, because a view of that temporary would name
       storage freed as the deserialization unwound. With nothing materialised each element views the document exactly
       as a lone `std::string_view` does, and is valid for exactly as long as that source is. A string the source spelt
       with escape sequences is still refused, and a value-backed source still borrows the caller's storage rather than
       the reader's arena (#230).
     - `std::optional<T>` reads what the `value` holds rather than what encoding it would write. A value-backed
       reader has no token for a non-finite `kind::decimal` and renders one as `null`, so deciding "none" from the
       node type alone turned a `std::optional<double>` holding a NaN into an empty one. Where there is a `value` to
       ask, its `kind` decides -- the same line the numeric deserializers already drew for the same reason (#230).
     - Added `deserialization_context::skip_failed_value` and `deserialization_context::note_value_consumed`, which are
       what a composite honouring `deserialize_options::on_error::collect_all` should step over a failed element with.
       `reader::next_value` alone is not enough and the recovery example on `deserialization_context::recover` said
       otherwise: an adapter on the `value` bridge reading a structure out of JSON *text* has already walked the cursor
       past it, because materialising it is what does the walking, so a loop stepping again skipped the following
       sibling entirely -- dropping it from the result, dropping every problem it had to report, and renumbering
       everything after it. Collecting over an array of three objects where the first and third were bad reported the
       first and the *second*, and never read the third. Whatever fails with the value behind it rather than in front of
       it now says so through `note_value_consumed`, keyed to the reader it consumed it from, and the note lives and
       dies with one call to `deserialize`. The bridge says it for itself; so does a container which read its own
       closing token, and a wrapper or optional whose construction rejects a value the deserialization below it already
       stepped over. The note also settles *where* the failure was, since the cursor no longer says: the enclosing
       structure, which is still true of the value that failed, where the next sibling is both false and actively
       misleading. That is the same approximation the bridge already made, and for the same reason -- naming the
       position exactly would mean building a path before every successful deserialization, which on a text source
       rescans from the start of the document. This was unreachable until a composite walked the reader per element,
       which is what made it visible (#230).
     - The same note covers every way an adapter can fail once it has stepped the cursor, which is more ways than it
       first appears: a wrapper or optional whose constructor refuses the value handed to it, an optional-like type
       which refuses to default-construct on `null`, the moves at the end of `adapter_for::deserialize` and
       `deserializer_for::deserialize` which place the created object into the caller's storage, and both of the moves a
       registered callable's result makes on its way out -- the one which normalises it into the `std::expected` the
       pipeline speaks, and, for a callable written against `value`, the return which happens once the bridge has
       committed and can no longer report it. All of these are the caller's own types and all of them can throw after
       the value is behind the cursor. A throw out of the callable itself is deliberately not treated this way: it
       may have failed before consuming anything, so the call is made outside the guard.
     - `container_adapter` finishes walking its array before letting an exception out of the element loop. Inserting
       into the container is the caller's code -- a `std::set` comparator or a move constructor may throw -- and a
       failure there used to leave the cursor stranded between two elements. A loop above it resumed at that token
       and read the inner `]` as its own end, so deserializing a `std::vector<std::set<T>>` under `collect_all` where
       two of the sets were bad reported one of them and stopped. The walk steps over whole child values rather than
       leaving "the current structure", because a child which is itself an array or object has to be crossed rather
       than entered -- leaving one of those lands back inside the container being built. A failure after the closing
       token has already been read has nothing left to walk and skips this entirely, which is what keeps a throwing
       move of the finished container from consuming the sibling after it (#230).
     - The serialization builder DSL reads the reader directly instead of a `value` materialised for it, which is
       what takes the last composite most users actually deserialize through off the bridge. It is not a change of
       signature so much as a change of who drives the loop: each member used to be handed the whole parent object
       and look itself up in it by name, walking its `alias`es until one hit, which is random access into
       something a forward cursor cannot offer. The walk now goes the other way -- over the document's keys, each
       dispatched to the member which claims it -- so a type described by the DSL is read in one pass instead of
       being built as a tree and then read out of that tree. A key which claims no member is stepped over whole,
       which on a tape-backed reader costs one move however large the subtree under it is (#231).
     - Deserialization of a DSL-described type runs in **document order** rather than member-declaration order. Which
       member reports a problem first changes with it, and so does the order of any side effects a caller's mutator
       has. Declaration order still decides two things: which name wins when a member and an `alias` of
       another both appear, and the order the members no key claimed are reported or defaulted in, since that pass
       happens after the walk (#231).
     - Three type-level hooks lose their `const value&` parameter, which is the price of the above and a source break
       for anyone using them: `pre_deserialize` is now `void(deserialization_context&)`, the `on_unknown_members`
       handler is `void(deserialization_context&, std::set<std::string>)`, and a member's `default_value` factory is
       `TMember(deserialization_context&)`. `deny_unknown_members` follows the second of those and never read its
       `value` anyway. A forward cursor cannot hand a callback the object it is part-way through reading,
       and a missing key is only known to be missing once every key which was there has gone by. What the parameter gave
       them -- most sharply a default computed from a sibling member -- they ask the context for instead, through
       `deserialization_context::source_value` (#235). `type_default_value` is unaffected: it took only the context
       already (#231).
     - `deserialization_context::encoded_source` quotes the object a DSL-described type is being deserialized from, so a
       `post_deserialize` which refuses the object can say which one it was. It is there for the hooks which run once
       the walk reaches the object's `}` -- `on_unknown_members` and every member's `default_value` as well as
       `post_deserialize` -- and is empty anywhere else, including inside anything one of those hooks goes on to
       deserialize through its context. Read from JSON text it is a view of exactly what was written; read from a
       `value` it is that value's compact encoding, made the first time a hook asks, so a deserialization which never
       asks pays nothing for it from either. It is text to quote; `deserialization_context::source_value` is the tree
       to query (#134).
     - `deserialization_context::source_value` gives the hooks of a DSL-described type the object they are about, as a
       `value` to read members out of: `pre_deserialize` can refuse a document by a version member, a `default_value`
       can compute from a sibling, and an `on_unknown_members` handler can read the values of the keys it is handed. It
       returns a `jsonv::optional<const value&>`, which is empty anywhere else -- a member's `check` or setter during
       the walk, a `type_default_value` standing in for a `null`, anything a hook goes on to deserialize through its
       context, and anything outside DSL deserialization. `pre_deserialize` is shown the value the reader is on, which
       is not necessarily an object. Read from a `value`, it is the caller's own tree. Read from text, the first hook to
       ask has the object read into a `value` through a second cursor -- from where the reader is, before the walk, or
       from a position on the tape kept from the object's `{`, after it -- and the hooks after it are shown the same
       one. Keeping that position allocates nothing, so a deserialization which never asks costs what it did. What is
       read from text belongs to the deserialization, so while it is being lent `source_is_temporary` is `true`, and a
       `std::string_view` deserialized from it is refused rather than left to dangle (#235).
     - A `null` which takes a member's default under `default_on_null` has it applied once the walk is done, in
       declaration order with the defaults of the keys which never arrived, rather than as the walk meets the `null`.
       That is what the option always said a `null` means, and it is what lets that default read and quote the whole
       object. Its setter now runs after every member the document did give a value, rather than in document order
       (#235).
     - Finding the keys which claimed no member is now free. It used to be a second scan over the materialised object,
       registered as a `pre_deserialize` and comparing every key against every member; the walk now knows which keys
       those were because it is the thing which failed to place them. It is also no longer a `pre_deserialize`, so it
       runs after the walk rather than before it -- a handler which throws, as `deny_unknown_members` does, now does so
       with the object already read rather than untouched. The set of names is only built when a handler was registered
       (#231).
     - `deserialize_options::on_duplicate_key` is honoured by DSL deserialization. It was not before, and not by
       omission: on a `value` the duplicate had already been collapsed by the parse, and on JSON text the bridge's own
       materialisation kept the last spelling whatever the option said. The walk sees both keys, so it can answer for
       them -- `replace` deserializes the later one over the earlier, `ignore` steps over it, and `exception` reports
       `Duplicate key in object: "..."`, the same message `parse_index::extract_tree` raises for the same document.
       `exception` is asked of every key on the way past rather than only of the ones a member answers to: a repeated
       key is the same thing to it either way, and a member reading itself from its preferred name cannot tell a repeat
       of a name it has already passed over from a first sighting of another, so which of the two an object was refused
       for would otherwise depend on the order it listed them in. A document repeating a key no member wants is refused
       as well, which is again what building the `value` first would have done (#231).
     - `check` runs. The mutator it composes a check into was stored on the member and never read by anything,
       so every `check` in every DSL since the feature was added in 2015 has been a no-op; it is now applied
       to the value which was read, before that value reaches the member. A predicate which has never executed may
       well reject data which has been deserializing cleanly for years, which is the reason to call this out rather
       than file it as a fix (#231).
     - The two-argument forms of `check` compile. A predicate with a thrower, and a predicate with an exception to
       throw, were documented from the start and had never compiled: the exception form typed its predicate `void`
       and so could not hand it to the thrower form, and a thrower lambda was an exact match for the exception form's
       `TException` where the thrower form needed a conversion, so both calls ended in the exception form calling
       itself with the thrower as the exception, closure type after closure type, until the instantiation depth ran
       out. The predicate is `bool` in both, and the exception form is constrained to a `TException` which cannot be
       called with the value as `std::function` would call it, which is what tells an exception from a thrower (#334).
     - `alias` compiles. The friendship which lets the member builder reach the member adapter's list of
       names was declared unqualified inside `jsonv::detail`, so it named a `jsonv::detail::member_adapter_builder`
       which does not exist rather than the `jsonv::member_adapter_builder` which does. Every use of
       `alias` failed to compile, which is why nothing in the tree used one (#231).
     - The preference order among a member's names is enforced by the walk rather than falling out of the lookup.
       Searching a materialised object went through the names in order and read only the winner; meeting them in the
       order the *document* put them says nothing about which the type prefers, so each member remembers which of
       its names it is being read from and a better-ranked one supersedes a worse. A document which spells one
       member two ways therefore resolves to the earliest declared name whichever order it used, and the spelling
       which loses is stepped over unread -- so a `check` on the member never sees it. One case does not
       survive the change: where the losing spelling comes *first* and is itself malformed, it has already been read
       and its failure reported by the time the preferred one arrives, because a forward walk has to read a value
       when it meets it and the preferred name may never come (#231).
     - A second name for the same member is not a duplicate key. The two are different questions -- one member named
       two ways against one key repeated -- and only the second is `deserialize_options::on_duplicate_key`'s to decide,
       so strict duplicate handling no longer refuses a document which merely uses an `alias` (#231).
     - Deserializing a DSL-described type from something which is not an object reports a node type mismatch naming
       what was found. It used to be whatever `value::find` threw when a member looked itself up in a non-object,
       which was a `kind_error` about the wrong thing (#231).
     - A `std::string_view` member of a DSL-described type deserialized from JSON text is a view of the source rather
       than a refusal, for the same reason `std::vector<std::string_view>` became one in #230: the refusal was
       structural, and there is no longer a materialised tree for the view to dangle into (#231).
     - Added `deserialize<T>` overloads which read JSON text and `reader`s directly, so deserializing from text no
       longer means building a `value` first. `deserialize<T>(text)` takes anything which converts to
       `std::string_view`, optionally with `parse_options`; `deserialize<T>(reader&)` and `deserialize<T>(reader&&)`
       take a reader, the latter so `deserialize<my_type>(jsonv::reader(text))` reads naturally. Every one takes
       `formats` and `deserialize_options` as the `value` overloads do, and every one throws `deserialization_error`. A
       reader on `document_start` is read as a whole document: its source is checked with `reader::validate`, the
       `document_start` is stepped over -- so neither the caller nor any deserializer ever has to -- and the value must
       be all there is. A reader the caller has already positioned is read from where it is, leaving the cursor one past
       the value. Text which does not parse is reported as a problem carrying the `parse_error` as its cause, rather
       than as whatever a deserializer made of the `error` node it ran into -- including a reader positioned on that
       node, and a parse which failed before the document began, as a `max_structure_depth` of 0 does (#232).
     - A whole-document deserialization refuses anything after the value. The parser lets trailing text through after a
       top-level scalar -- `5 6` parses -- so without this `deserialize<int>("5 6")` would have been 5 where
       `deserialize<int>(parse("5 6"))` always threw. The same check applies to `deserialize<T>(const value&)`, where
       the only thing it can catch is a deserializer which broke `deserializer::deserialize`'s rule of leaving the
       cursor one past its value; one which did so used to pass unnoticed at the top level and is now an error (#232).
     - Source break: a C++ string passed to `deserialize<T>` is JSON text. `value` converts implicitly from `const
       char*`, `std::string` and `std::string_view`, so `deserialize<ring>("fire", fmts)` used to deserialize from the
       JSON *string* `"fire"`; it now parses `fire` as a document, which fails. Say `deserialize<ring>(value("fire"),
       fmts)` to mean the string, or `deserialize<ring>(R"("fire")", fmts)` to write it as JSON. Anything else which
       converts to `value` -- integers, `double`, `bool`, wide strings -- means what it did. This is the ambiguity
       `reader::from_value` was named to avoid (#224), settled the other way round: the text overloads are an exact
       match for a string where the conversion to `value` is not (#232).
     - Deserialization refuses to return a view of a source it was handed to own. `deserialize<T>(std::string&&)` takes
       the text over and frees it on return, as `deserialize<T>(reader&&)` does for a reader which
       `reader::owns_source`, so a `std::string_view` deserialized from either would dangle before the caller could read
       it; both are refused, as for a materialised tree. `deserialization_context::source_is_temporary` now covers both
       causes. Pass a `std::string_view`, a `std::string` lvalue or a reader over storage you keep to deserialize views
       (#232).
     - Added `deserialize<T>(reader&, deserialization_context&)`, `deserialize<T>(reader&&, deserialization_context&)`
       and `deserialize<T>(text, [parse_options,] deserialization_context&)`, which deserialize through a context the
       caller built rather than one made from `formats` and `deserialize_options`. A version, user data and a base path
       exist only on a context, so until now nothing read from a reader or from text could be given one: the only
       whole-document entry taking a context was `deserialization_context::deserialize<T>(const value&)`. A failed call
       takes the problems it recorded off the context with the exception, leaving it holding what it held before (#133).
     - `deserialization_error` says which document a problem is in as well as where in it. `deserialization_context`
       takes the name of the document -- usually the file its text was read from -- as a new last constructor argument,
       `source_name`, and every problem recorded on it carries the name as
       `deserialization_error::problem::source_name`. `what()` joins it to the path with a `#`, as in `Deserialization
       error at config.json#.servers[2].port: ...`, and gives the name alone for a problem with no position, such as a
       parse failure. A problem folded in from a context which named some other document keeps that name.
       `deserialization_error::source_name` is the first problem's, as `path` is, and without a name the message is
       unchanged. Spell out every argument before the name: a string in the place of `userdata` converts to `const
       void*` and is taken for the user data. `deserialization_error::problem` and `deserialization_context` both change
       layout (#133).
     - Removed `extraction_context::extract_sub`. It reached into an already-materialised tree by path, which a forward
       cursor has no equivalent for and which nothing in the library still did. Within a `value`-based adapter, name the
       part and say where it is: `context.deserialize<int>(from.at("a"))` under a `deserialization_context::path_scope`.
       Also removed are the `path_scope` constructor taking a whole `jsonv::path`, which only `extract_sub` used, and
       `deserialization_context::deserialize(const std::type_info&, const value&, void*)`, since a deserialization
       refused after it has built its object -- something after the value -- has to destroy that object, and a caller of
       the `void*` form had no way to let it (#232).
     - Deserializing a `value` from JSON text settles a repeated key by `deserialize_options::on_duplicate_key`, as
       `parse` does under the same options: `ignore` keeps the first value, and `exception` refuses the object with
       `Duplicate key in object: "..."` placed at the key which repeated -- below the deserialization's own base path or
       scope where it has one, which outranks the reader's path as it does for any other problem. It kept the last
       whatever the option said, so `deserialize<value>(text, options)` and `parse(text, parse_options(), options)`
       disagreed under anything but the default. The same goes for an adapter on the `value` bridge, which is handed the
       tree this builds, and for `read_value(deserialization_context&, reader&)` generally; `read_value(reader&)` has no
       options and still keeps the last (#263).
     - `polymorphic_adapter` reads the reader directly instead of a `value` materialised for it. Choosing a subtype
       means looking at the value before deserializing it, which a forward cursor cannot do by itself, so the
       discriminators are shown the value through a second cursor on the same source -- the reader itself never moves
       backward -- and the subtype they choose is then deserialized from the reader. From JSON text, a subtype
       registered with `add_subtype_keyed`, which is what the DSL's `subtype<T>(value)` registers, is shown only the
       members it discriminates on, found by stepping over every other member whole; a document such a subtype matches
       is read once however large it is and wherever the discriminator sits in it. A discriminator registered with
       `add_subtype` can ask anything of the value, so the first time one of those has to be asked the subtree is
       materialised for it, and listing the keyed subtypes first keeps any document they match from paying for that.
       From a `value` every discriminator is shown that value, as before, and registration order still decides between
       two which both match. What a discriminator is shown settles a repeated key by
       `deserialize_options::on_duplicate_key` at every depth, as the DSL and, since #263, a `value` read from text do
       -- so the subtype chosen and the subtype built agree on what the document said (#236).
     - A subtype chosen from JSON text reads the document rather than a copy of it. A `std::string_view` member is a
       view of the source rather than a refusal, and a member a keyed subtype never reads can no longer fail it: the
       bridge built the whole object before choosing, so a number with no finite `double` anywhere in it failed every
       subtype. A document no discriminator matches is refused with the cursor still on it, and the message names it as
       before -- unless naming it would mean reading something which cannot be read, in which case the message stops at
       `No discriminators matched JSON value` rather than reporting that instead. This is a source break for anyone
       deriving from `polymorphic_adapter` and overriding `create(deserialization_context&, const value&)`: the base is
       now `adapter_for` and the hook takes a `reader` (#236).
     - `enum_adapter` reads the reader directly instead of a `value` materialised for it. Under the library's own
       orderings -- `std::less<value>`, `value_less` and `value_less_icase`, which are what `enum_type` and
       `enum_type_icase` use -- a string read from JSON text is looked up by its text, so nothing is built to hold it;
       one written with escapes is decoded first. A number, a boolean or `null` is read where it sits and builds nothing
       either. Any other ordering can only be asked about a `value`, so it is still handed one. The
       `benchmark/deserialize/enum_strings` rows measure it: 100000 ticket states deserialize from text in 21 ms rather
       than 37 ms, and from an existing `value` in 23 ms rather than 28 ms (#237).
     - A value an `enum_adapter` has no mapping for is refused with the reader still on it, and the message now lists
       every JSON value the mapping accepts, in the mapping's order: `Invalid value for ring: "bogus" (expected one of
       "earth", "fire", "heart", "useless", "water", "wind")`. The refusal is placed by
       `deserialization_context::problem_path`, as every other is, so a reader positioned part-way through a document
       reports where it is unless a scope or base path says otherwise -- the `value` bridge reported the root. A `TEnum`
       whose move constructor throws no longer makes a collecting container skip the value after it, as the bridge still
       does (#259). This is a source break for anyone deriving from `enum_adapter` and overriding
       `create(deserialization_context&, const value&)`: the base is now `adapter_for` and the hook takes a `reader`
       (#237).
     - Source break: the type-level read interface takes serde's name. `extractor` is `deserializer` and its
       `extract` is `deserialize`; `extraction_context` is `deserialization_context`, `extraction_error` is
       `deserialization_error`, `extract_options` is `deserialize_options` and `no_extractor` is
       `no_deserializer`; `extractor_for`, `extractor_construction`, `function_extractor` and `make_extractor`
       are `deserializer_for`, `deserializer_construction`, `function_deserializer` and `make_deserializer`;
       `formats::extract`, `get_extractor` and `register_extractor` are `deserialize`, `get_deserializer` and
       `register_deserializer`; and the free `extract<T>` overloads are `deserialize<T>`. The headers follow:
       `serialization/extract.hpp` is `serialization/deserialize.hpp`, and `extractor_for.hpp`,
       `function_extractor.hpp` and `extractor_construction.hpp` are `deserializer_for.hpp`,
       `function_deserializer.hpp` and `deserializer_construction.hpp`. A `deserialization_error`'s message
       begins `Deserialization error`, and the `benchmark/extract/` rows are `benchmark/deserialize/`.
       `serializer` already had the serde name, so the two directions now share a root as `reader` and `writer`
       do. `value::extract` and `parse_index::extract_tree` keep their names; the serialization builder's hooks are
       renamed by #328, below (#318).
     - Source break: the serialization builder's hooks take serde's attribute names, as the layer's types took its
       trait names. `alternate_name` is `alias`; `encode_if` is `serialize_if`, keeping its positive sense rather than
       inverting it into serde's `skip_serializing_if`, because `since`, `until`, `after` and `before` compose on it;
       `check_input` is `check`, which is what it does -- it checks the value read for a member, never the JSON;
       `pre_extract` and `post_extract` are `pre_deserialize` and `post_deserialize`; and `on_extract_extra_keys` is
       `on_unknown_members`, "members" being what RFC 8259 calls an object's name/value pairs. The handler which
       refuses them, `throw_extra_keys_deserialization_error`, is `deny_unknown_members`, and its message begins
       `Unknown member`. `default_value`, `default_on_null`, `type_default_value`, `type_default_on_null`, `since`,
       `until`, `after` and `before` keep their names (#328).
     - Fixed `demangle` reading past the end of its `std::string_view`. The default demangler handed the view's
       `data()` to `__cxa_demangle`, which reads to a terminator, so a view of part of a longer string was demangled
       along with whatever followed it -- a name it understood came back undemangled, and a view at the end of a
       buffer was read beyond it (#188).
     - Fixed copying a `function_deserializer` or `function_serializer` from a non-const lvalue failing to compile.
       Their constructor taking the wrapped function by forwarding reference was a better match than the copy
       constructor, so the copy tried to build the function out of the adapter. The constructor now refuses the
       adapter's own type (#188).
     - Fixed a `formats` registration which fails taking the registrations it found down with it. Registering a shared
       `deserializer`, `serializer` or `adapter` changes the type's entry and then takes a share of the object, and when
       taking the share ran out of memory the entry was erased whether or not the call had added it. Under
       `duplicate_type_action::ignore` that deleted the mapping the call had been told to keep, under `replace` it
       deleted the mapping rather than restoring the one it had replaced, and an adapter could lose its deserializer and
       its serializer both. Registering an adapter by pointer did the same to a deserializer already there when adding
       the serializer failed, and the serialization builder registers through both under `on_duplicate_type`. A caller
       which caught the `std::bad_alloc` and carried on got `no_deserializer` or `no_serializer` -- or, in a `formats`
       composed over a base, the base's implementation without a word. A registration which throws now leaves the
       `formats` as it found it (#244).
     - Fixed `default_on_null` without a `default_value`, and `type_default_on_null` without a `type_default_value`,
       failing with `std::bad_function_call`. A `null` where either applied was handed to a default factory nobody
       provided, so the document was refused with a `deserialization_error` describing the library's internals. The flag
       is now only considered when there is a default to take -- the test a missing key was already put to, and what the
       DSL reference has always said for a member. A `null` with nothing to fall back on is deserialized like any other
       value: for an integer member, or for the type itself, a node type mismatch naming `null` (#258).
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
     - The reference documentation gives each overload the comment it shares with the rest of its set. Doxygen attached
       that comment to the set's first declaration only, so `reader::from_value(value&&)`, the `std::wstring` and
       `const` forms of `value`'s accessors and about ninety others had no entry; `DISTRIBUTE_GROUP_DOC` is now on, and
       each shared comment has been made true of every overload it now describes. The `string_view` and `istream`
       overloads of `parse`, `ostream_pretty_encoder` and one `enum_adapter` constructor are documented where they are
       declared rather than on a stray "Examples" page, and a new Configuration module lists the macros in `config.hpp`
       (#300).

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
