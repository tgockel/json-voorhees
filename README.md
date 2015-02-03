JSON Voorhees
=============

Yet another [JSON][JSON] library for C++.
This one touts new C++11 features for developer-friendliness, an extremely high-speed parser and no dependencies beyond
 a compliant compiler.
If you love Doxygen, check out the [documentation](http://tgockel.github.io/json-voorhees/).

Features include (but are not necessarily limited to):

 - Simple
   - A `value` should not feel terribly different from a C++ Standard Library container
   - Write valid JSON with `operator<<`
   - Simple JSON parsing with `parse`
   - Reasonable error messages when parsing fails
   - Full support for Unicode-filled JSON (encoded in UTF-8 in C++)
 - Efficient
   - Minimal overhead to store values (a `value` is 16 bytes on a 64-bit platform)
   - No-throw move semantics wherever possible
 - Easy
   - Convert a `value` into a C++ type using `extract<T>`
   - Encode a C++ type into a value using `to_json`
 - Safe
   - In the best case, illegal code should fail to compile
   - An illegal action should throw an exception
   - Almost all utility functions have a [strong exception guarantee](http://www.gotw.ca/gotw/082.htm)
 - Stable
   - Worry less about upgrading -- the API and ABI will not change out from under you
 - Documented
   - Consumable by human beings
   - Answers questions you might actually ask
 - Compiler support
     - GCC (4.8+)
     - Clang++ (3.3+)

[![Build Status](https://travis-ci.org/tgockel/json-voorhees.svg?branch=master)](https://travis-ci.org/tgockel/json-voorhees)
[![Code Coverage](https://img.shields.io/coveralls/tgockel/json-voorhees.svg)](https://coveralls.io/r/tgockel/json-voorhees)
[![Github Issues](https://img.shields.io/github/issues/tgockel/json-voorhees.svg)](http://github.com/tgockel/json-voorhees/issues)
[![Flattr this git repo](http://api.flattr.com/button/flattr-badge-large.png)](https://flattr.com/submit/auto?user_id=tgockel&url=https://github.com/tgockel/json-voorhees&title=json-voorhees&language=c++&tags=github&category=software)

![JSON Conversions](https://raw.githubusercontent.com/tgockel/json-voorhees/master/doc/conversions.png)

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

 - [A system for converting between JSON and C++ types](https://github.com/tgockel/json-voorhees/issues/8)
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
     - Clang 3.5+
     - Clang 3.3 - 3.4 (with [Boost.Regex][Boost.Regex])
 - Planned
     - MSVC 12.0+ (Visual C++ 2013)

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

F.A.Q.
------

### Why are `integer` and `decimal` distinct types?

The [JSON specification][JSON] only has a *number* type, whereas this library has `kind::integer` and `kind::decimal`.
Behavior between the two types should be fairly consistent -- comparisons between two different `kind`s should behave as
 you would expect (assuming you expect things like `value(1) == value(1.0)` and `value(2.0) < value(10)`).
If you wish for behavior more like JavaScript, feel free to only use `as_decimal()`.

The reason integer and decimal are not a single type is because of how people tend to use JSON in C++.
If you look at projects that consume JSON, they make a distinction between integer and decimal values.
Even in specification land, a distinction between the numeric type is pretty normal (for example:
 [Swagger](http://swagger.io/)).
To ultimately answer the question: integer and decimal are distinct types for the convenience of users.

### Why are `NaN` and `INFINITY` serialized as a `null`?

The [JSON specification][JSON] does *not* have support for non-finite floating-point numbers like `NaN` and infinity.
This means the `value` defined with `object({ { "nan", std::nan("") }, { "infinity", INFINITY } })` to get serialized as
 `{ "nan": null, "infinity": null }`.
While this seems to constitute a loss of information, not doing this would lead to the encoder outputting invalid JSON
 text, which is completely unacceptable (unfortunately, this is a *very common* mistake in JSON libraries).
If you want to check that there will be no information loss when encoding, use the utility funciton `validate`.

*Why not throw when encoding?*
One could imagine the `encoder::encode` throwing something like an `encode_error` instead of outputting `null` to the
 stream.
However, that would make `operator<<` a potentially-throwing operation, which is extremely uncommon and would be very
 surprizing (imagine if you tried to log a `value` and it threw).

*Why not throw when constructing the `value`?*
Instead of waiting for encoding time to do anything about the problem, the library could attack the issue at the source
 and throw an exception if someone says `value(INFINITY)`.
This was not chosen as the behavior, because non-finite values are only an issue in the string representation, which is
 not a problem if the `value` is never encoded.
You are free to use this JSON library without `parse` and `encode`, so it should not prevent an action simply because
 someone *might* use `encode`.

### Why are there so many corruption checks?

If you are looking through the implementation, you will find a ton of places where there are `default` cases in `switch`
 statements that should be impossible to hit without memory corruption.
This is because it is unclear what should happen when the library detects something like an invalid `kind`.
The library could `assert`, but that seems overbearing when there is a reasonable option to fall back to.
Alternatively, the library could throw in these cases, but that leads to innocuous-looking operations like `x == y`
 being able to throw, which is somewhat disconcerning.

### Why do you use GNU Make instead of a modern build system?

There are a lot of build automation tools that can work with C++ code, from [CMake](http://www.cmake.org/) to
 [SCons](http://www.scons.org/), yet this project uses GNU Make.
This is done so all you have to do to build the project is type `make`.
I personally do not understand what these alternative build systems give me that GNU Make does not already do, so I will
 stick with my plain old `Makefile` until I see a benefit to changing.
Yes, I am a luddite who does not understand the fancy things the kids are using these days.
Get off my lawn.

### This is really helpful! Can I give you money?

I would be [Flattr-ed](https://flattr.com/submit/auto?user_id=tgockel&url=https://github.com/tgockel/json-voorhees&title=json-voorhees&language=c++&tags=github&category=software)!

### With such a cool name, do you have an equally cool logo?

Not really...

![JSON: Serialized Killer](https://raw.githubusercontent.com/tgockel/json-voorhees/master/doc/meme.jpg)


 [Boost.Regex]: http://www.boost.org/doc/libs/1_56_0/libs/regex/doc/html/index.html
    "The Boost.Regex library"
 [decode-non-utf8]: https://github.com/tgockel/json-voorhees/issues/10
    "Decode numeric encodings into arbitrarily encoded std::string"
 [encode-utf8]: https://github.com/tgockel/json-voorhees/issues/11
    "Issue 21: String encoding should allow UTF-8 output"
 [future-features]: https://github.com/tgockel/json-voorhees/issues?q=is%3Aopen+is%3Aissue+no%3Amilestone
    "Future features"
 [JSON]: http://www.json.org/
    "JSON Specification"
