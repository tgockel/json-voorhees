Introduction
============

Yet another [JSON](http://www.json.org/) library for C++.
This one touts new C++11 features for developer-friendliness, an extremely high-speed parser and no dependencies beyond
 a compliant compiler.

Features include (but are not necessarily limited to):

 - An awesome type heirarchy for the representation of JSON values
 - Documentation consumable by human beings that answers questions you might actually ask
 - Read a JSON value with `jsonv::parse`
 - Write a JSON value with `operator<<`
 - Reasonable error messages when parsing fails

Future
------

This library is still in a "working prototype" stage, so there are a number of things that are not really implemented.
Future planned features can be found on the [issue tracker][future-features], but here is a list of things that matter
 most to me:

 - Full support for Unicode-filled JSON (encoded in UTF-8 in C++)
 - [A visitor system with JSON Path support](https://bitbucket.org/tgockel/json-voorhees/issue/15/generic-visitor-system-for-a-json-tree)
 - Compiler support
     - [Clang++](https://bitbucket.org/tgockel/json-voorhees/issue/20/clang-support)
     - [MSVC](https://bitbucket.org/tgockel/json-voorhees/issue/18/msvc-support)

Versioning
----------

Like every software system ever, JsonVoorhees describes versions in 3 parts: `${major}.${minor}.${revision}`.
Unlike most software, it actually means something when the versions change.

 - `${major}`: There are no guarantees whatsoever across major releases.
 - `${minor}`: For a given minor release, all code is forward-compatible source-code compliant.
               That means code written against version 1.1 can be recompiled against version 1.4 and continue to work.
               No guarantees are made for going backwards (version 1.4 might have added new functions).
 - `${revision}`: Code is ABI-compatible across revision.
                  Library *A* built against JsonVoorhees 1.2.4 should be able to pass a `jsonv::value` to library *B*
                   built against JsonVoorhees 1.2.9.
                  Any change to publicly-visible data structures or calling conventions will correspond to a bump in the
                   minor version.

The preceding statements are not true if any of the version components has a negative value.
Negatives are "pre-release" and are allowed to do whatever they feel like before a release.

License
-------

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
the License. You may obtain a copy of the License at
 
  [http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0)

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

Details
=======

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

 [decode-non-utf8]: https://bitbucket.org/tgockel/json-voorhees/issue/24/decode-numeric-encodings-into-arbitrarily
    "Decode numeric encodings into arbitrarily encoded std::string"
 [encode-utf8]: https://bitbucket.org/tgockel/json-voorhees/issue/21/string-encoding-should-allow-utf-8-output
    "Issue 21: String encoding should allow UTF-8 output"
 [future-features]: https://bitbucket.org/tgockel/json-voorhees/issues?kind=enhancement&kind=proposal&status=new&status=open
    "Future features"
