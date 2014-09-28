JSON Voorhees
=============

Yet another [JSON](http://www.json.org/) library for C++.
This one touts new C++11 features for developer-friendliness, an extremely high-speed parser and no dependencies beyond
 a compliant compiler.
If you love Doxygen, check out the [documentation](http://tgockel.github.io/json-voorhees/).

Features include (but are not necessarily limited to):

 - A simple system for the representation of JSON values
 - Documentation consumable by human beings that answers questions you might actually ask
 - Simple JSON parsing with `jsonv::parse`
 - Write a JSON value with `operator<<`
 - Reasonable error messages when parsing fails
 - Full support for Unicode-filled JSON (encoded in UTF-8 in C++)
 - Safe: in the best case, illegal code should fail to compile; in the worst case, an illegal action should throw an
   exception
 - Stable: worry less about upgrading -- the API and ABI will not change out from under you
 - Compiler support
     - GCC (4.8+)
     - Clang++ (3.3+)

[![Build Status](https://travis-ci.org/tgockel/json-voorhees.svg?branch=master)](https://travis-ci.org/tgockel/json-voorhees)

Compile and Install
-------------------

JSON Voorhees uses plain-old [GNU Make](http://www.gnu.org/software/make/) as the build and installation system so you
 do not have to deal with any wonky custom build systems or automatic configuration software.
If you have `boost`, `g++` and `make` installed, simply:

    $> make
    $> sudo make install

If you want to customize your compilation or installation, see the top of the `Makefile` for easy-to-use configuration
 options.

Future
------

This library is still in a "working prototype" stage, so there are a number of things that are not really implemented.
Future planned features can be found on the [issue tracker][future-features], but here is a list of things that matter
 most to me:

 - [A visitor system with JSON Path support](https://github.com/tgockel/json-voorhees/issues/3)
 - Compiler support
     - [MSVC](https://github.com/tgockel/json-voorhees/issues/7)

License
-------

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
the License. You may obtain a copy of the License at

  [http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0)

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

Miscellaneous
=============

Compiler Support
----------------

 - Supported
     - GCC 4.9+
     - GCC 4.8 (with [Boost.Regex][Boost.Regex])
     - Clang 3.3+ (with [Boost.Regex][Boost.Regex])
 - Unplanned
     - MSVC

While GCC 4.8 and Clang 3.3 are supported, you must compile them with `make USE_BOOST_REGEX=1`.
Early versions will happy compile regular expressions, but will fail at runtime with a `regex_error`, which is not very
 useful.
However, you can use [Boost.Regex][Boost.Regex] as the regular expression engine for compilers with incomplete `<regex>`
 implementations.
The downside of this is your application must link with the Boost libraries (`-lboost_regex -lboost_system`).

    $> make USE_BOOST_REGEX=1
    $> sudo make install

In the long-term, this library should be ported to MSVC, but there is no timeline for doing so at the moment.
For progress updates, check in at [the MSVC support issue](https://github.com/tgockel/json-voorhees/issues/7).

Versioning
----------

Like every software system ever, JSON Voorhees describes versions in 3 parts: `${major}.${minor}.${maintenance}`.
Unlike most software, it actually means something when the versions change.

 - `major`: There are no guarantees whatsoever across major releases.
 - `minor`: For a given minor release, all code is forward-compatible source-code compliant.
            That means code written against version 1.1 can be recompiled against version 1.4 and continue to work.
            No guarantees are made for going backwards (version 1.4 might have added new functions).
 - `maintenance`: Code is ABI-compatible across maintenance.
                  Library *A* built against JSON Voorhees 1.2.4 should be able to pass a `jsonv::value` to library *B*
                   built against JSON Voorhees 1.2.9.
                  Any change to publicly-visible data structures or calling conventions will correspond to a bump in the
                   minor version.

The preceding statements are not true if the version is suffixed with `-preN`.
These values are "pre-release" and are allowed to do whatever they feel like before a release.

When developing code, follow this simple workflow to determine which version components need to change:

 1. *Will this change force users to change their source code?*
    If yes, bump the `major` version.
 2. *Will this change force users to recompile to continue to work?*
    If yes, bump the `minor` version.
 3. *Will this change the behavior in any way?*
    If yes, bump the `maintenance` version.
 4. *Did I only change comments or rearrange code positioning (indentation, etc)?*
    If yes, you do not need to update any part of the version.
 5. *Did I miss something?*
    Yes. Go back to #1 and try again.

Character Encoding
------------------

This library assumes you [really love UTF-8](http://www.utf8everywhere.org/).
When parsing JSON, this library happily assumes all sequences of bytes from C++ are UTF-8 encoded strings; it assumes
 you also want UTF-8 encoded strings when converting from JSON in C++ land; and it assumes the non-ASCII contents of a
 source string should be treated as UTF-8.
If you wish to use some other encoding format for your `std::string`, there is no convenient way to do that beyond
 conversion at every use site.
There is a [proposal][decode-non-utf8] to potentially address this.

On output, the system takes the "safe" route of using the numeric encoding for dealing with non-ASCII `std::string`s.
For example, the `std::string` for `"Travis Göckel"` (`"Travis G\xc3\xb6ckel"`) will be encoded in JSON as
 `"Travis G\u00f6ckel"`, despite the fact that the character `'ö'` is the most basic of the Unicode Basic Multilingual
 Planes.
This is generally considered the most compatible option, as (hopefully) every transport mechanism can gracefully
 transmit ASCII character sequences without molestation.
The drawback to this route is a needlessly lengthened resultant encoding if all components of the pipeline gracefully
 deal with UTF-8.
There is an [outstanding issue][encode-utf8] to address this shortcoming.

 [Boost.Regex]: http://www.boost.org/doc/libs/1_56_0/libs/regex/doc/html/index.html
    "The Boost.Regex library"
 [decode-non-utf8]: https://github.com/tgockel/json-voorhees/issues/10
    "Decode numeric encodings into arbitrarily encoded std::string"
 [encode-utf8]: https://github.com/tgockel/json-voorhees/issues/11
    "Issue 21: String encoding should allow UTF-8 output"
 [future-features]: https://github.com/tgockel/json-voorhees/issues?q=is%3Aopen+is%3Aissue+no%3Amilestone
    "Future features"
