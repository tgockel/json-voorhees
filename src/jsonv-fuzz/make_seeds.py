#!/usr/bin/env python3
"""Generate the committed fuzz seed corpus.

Run from anywhere; output lands in the corpus/ directory beside this script. The result is
committed. This generator is kept in-tree so the byte-exact contents of the awkward seeds --
invalid UTF-8, deep nesting, JSON escape soup -- are reviewable as source rather than as opaque
blobs.

Backslashes are built via BS rather than written literally, so nothing here depends on the editor
or tooling preserving escape sequences verbatim.
"""

import pathlib

OUT = pathlib.Path(__file__).parent / "corpus"

BS = chr(92).encode()  # a single backslash


def esc(text):
    """A JSON escape sequence: esc('n') -> backslash-n."""
    return BS + text.encode()


def write(name, data):
    (OUT / name).write_bytes(data)


def main():
    OUT.mkdir(exist_ok=True)

    # Depth. Parsing is iterative, but extract_tree, encoder::encode and ~value are mutually
    # recursive, so these probe the stack rather than the tape.
    write("depth_at_limit.json", b"[" * 127 + b"1" + b"]" * 127)
    write("depth_exceeded.json", b"[" * 200 + b"1" + b"]" * 200)
    write("depth_mixed.json", b'{"a":' * 60 + b"1" + b"}" * 60)

    # UTF-8 conformance cases (issue #207). These parse today; after that fix they must be rejected.
    write("utf8_five_byte.json", b'["' + b"\xf8\x80\x80\x80\x80" + b'"]')
    write("utf8_six_byte.json", b'["' + b"\xfc\x80\x80\x80\x80\x80" + b'"]')
    write("utf8_overlong.json", b'["' + b"\xc0\x80" + b'"]')
    write("utf8_surrogate.json", b'["' + b"\xed\xa0\x80" + b'"]')
    write("utf8_above_max.json", b'["' + b"\xf5\x80\x80\x80" + b'"]')

    # Valid UTF-8 across all four sequence lengths, which must keep working.
    write("utf8_valid_mixed.json", b'["ascii", "\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80"]')

    # Escape handling, around the surrogate-pair off-by-one fixed in b43fdd2.
    write("escapes_surrogate_pair.json",
          b'["' + esc("ud83d") + esc("ude00") + b'", "' + esc("uD800") + esc("uDC00")
          + b'", "' + esc("uDBFF") + esc("uDFFF") + b'"]')
    write("escapes_truncated.json", b'["' + esc("ud83d") + b'"]')
    write("escapes_lone_low.json", b'["' + esc("udc00") + b'"]')

    # A `\u` escape whose digits are non-ASCII bytes. `match_string` hands these to `std::isxdigit`
    # as a negative `char`, which is undefined behavior -- issue #211.
    write("escapes_high_byte_digits.json", b'["' + esc("u") + b"\x80\x30\x30\x30" + b'"]')
    write("escapes_all.json",
          b'["' + esc('"') + b" " + esc(BS.decode()) + b" " + esc("/") + b" " + esc("b")
          + b" " + esc("f") + b" " + esc("n") + b" " + esc("r") + b" " + esc("t")
          + b" " + esc("u0000") + b" " + esc("u001f") + b" " + esc("uffff") + b'"]')

    # Numeric extremes, including the slow-path integers that motivated the from_chars fix.
    write("numbers_extremes.json",
          b"[0,-0,1e309,1e-999,-1e-999,0e0,9223372036854775807,-9223372036854775808,"
          b"9223372036854775808,18446744073709551615,18446744073709551616,"
          b"12345678901234567890123,-99999999999999999999999,1.7976931348623157e308,"
          b"5e-324,0.1,-0.0,1E+2,1e-2]")
    # Integer tokens flush against the end of the input, which is what the `strtoull`/`strtoll` slow path used
    # to read past. The negative form is the one AddressSanitizer reports, since it intercepts `strtoll` but
    # not `strtoull`.
    write("numbers_at_eof.json", b"12345678901234567890")
    write("numbers_at_eof_negative.json", b"-1234567890123456789")

    # Duplicate keys are resolved during extraction, not indexing -- stage 2, which is where the round-trip oracle
    # lives. The default policy keeps the last value rather than throwing.
    write("duplicate_keys.json",
          b'{"a": 1, "a": 2, "b": {"c": null, "c": [], "c": {}}, "": 0, "": 1}')

    # `require_document` is off by default, so a bare scalar is a whole document. `create_strict` turns it on, so
    # these separate the two configurations.
    write("top_level_string.json", b'"a bare string is a document when require_document is off"')
    write("top_level_number.json", b"-12.5e3")

    # Comments are accepted by default, which is easy to forget.
    write("comments.json", b'{/* c */ "a": /* c */ [1, 2] /* c */ }')

    for path in sorted(OUT.iterdir()):
        print(f"{path.name}: {path.stat().st_size}")


if __name__ == "__main__":
    main()
