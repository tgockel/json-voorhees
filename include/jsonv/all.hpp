/// \file jsonv/all.hpp
/// A header which includes all other JSON Voorhees headers.
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

namespace jsonv
{

/// \mainpage Overview
///
/// JSON Voorhees is a JSON library written for the C++ programmer who wants to be productive in
/// this modern world. This one targets C++23 for developer-friendliness, a reasonably fast parser,
/// and no dependencies beyond a compliant compiler and standard library. It is hosted on
/// <a href="https://github.com/tgockel/json-voorhees">GitHub</a> and sports an Apache License, so
/// use it anywhere you need.
///
/// Features include (but are not necessarily limited to):
///
/// - Simple
///   - A `value` should not feel terribly different from a C++ Standard Library container
///   - Write valid JSON with `operator<<`
///   - Simple JSON parsing with `parse`
///   - Reasonable error messages when parsing fails
///   - Full support for Unicode-filled JSON (encoded in UTF-8 in C++)
/// - Efficient
///   - Minimal overhead to store values (a `value` is 16 bytes on a 64-bit platform)
///   - No-throw move semantics wherever possible
/// - Serialization/Deserialization
///   - Deserialize a C++ type straight from JSON text, or from a `value`, using `deserialize<T>`
///   - Serialize a C++ type straight to JSON text using `serialize`, or into a `value` using `to_json`
/// - Safe
///   - In the best case, illegal code should fail to compile
///   - An illegal action should throw an exception
///   - The query API is `[[nodiscard]]`, so dropping the answer to a question you asked is a warning
///   - Almost all utility functions have a [strong exception guarantee](http://www.gotw.ca/gotw/082.htm)
/// - Stable
///   - Worry less about upgrading -- the API and ABI will not change out from under you
/// - Documented
///   - Consumable by human beings
///   - Answers questions you might actually ask
///
/// \dotfile doc/conversions.dot
///
/// JSON Voorhees is designed with ease-of-use in mind. So let's look at some code!
///
/// \section demo_value The jsonv::value
///
/// The central class of JSON Voorhees is the \c jsonv::value, which represents a JSON AST. Putting
/// values of different types is easy.
///
/// \code
/// #include <jsonv/value.hpp>
/// #include <iostream>
///
/// int main()
/// {
///     jsonv::value x = jsonv::null;
///     std::cout << x << std::endl;
///     x = 5.9;
///     std::cout << x << std::endl;
///     x = -100;
///     std::cout << x << std::endl;
///     x = "something else";
///     std::cout << x << std::endl;
///     x = jsonv::array({ "arrays", "of", "the", 7, "different", "types?", true });
///     std::cout << x << std::endl;
///     x = jsonv::object({
///                         { "objects", jsonv::array({
///                                                    "Are fun, too.",
///                                                    "Do what you want."
///                                                  })
///                         },
///                         { "compose like", "standard library maps" },
///                      });
///     std::cout << x << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// null
/// 5.9
/// -100
/// "something else"
/// ["arrays","of","the",7,"different","types?",true]
/// {"compose like":"standard library maps","objects":["Are fun, too.","Do what you want."]}
/// \endcode
///
/// If that isn't convenient enough for you, there is a user-defined literal \c _json in the
/// \c jsonv namespace you can use:
///
/// \code
/// // You can use this hideous syntax if you do not want to bring in the whole jsonv namespace:
/// using jsonv::operator""_json;
///
/// jsonv::value x = R"({
///                       "objects": [ "Are fun, too.",
///                                    "Do what you want."
///                                  ],
///                       "compose like": "You are just writing JSON",
///                       "which I guess": ["is", "also", "neat"]
///                    })"_json;
/// \endcode
///
/// JSON is dynamic, which makes value access a bit more of a hassle, but JSON Voorhees aims to make
/// it not too horrifying for you. A \c jsonv::value has a number of accessor methods named things
/// like \c as_integer and \c as_string which let you access the value as if it was that type. But
/// what if it isn't that type? In that case, the function will throw a \c jsonv::kind_error with a
/// bit more information as to what rule you violated.
///
/// \code
/// #include <jsonv/value.hpp>
/// #include <iostream>
///
/// int main()
/// {
///     jsonv::value x = jsonv::null;
///     try
///     {
///         (void) x.as_string();
///     }
///     catch (const jsonv::kind_error& err)
///     {
///         std::cout << err.what() << std::endl;
///     }
///
///     x = "now make it a string";
///     std::cout << x.as_string().size() << std::endl;
///     std::cout << x.as_string() << "\tis not the same as\t" << x << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// Unexpected type: expected string but found null.
/// 20
/// now make it a string    is not the same as  "now make it a string"
/// \endcode
///
/// You can also deal with container types in a similar manner that you would deal with the
/// equivalent STL container type, with some minor caveats. Because the \c value_type of a JSON
/// object and JSON array are different, they have different iterator types in JSON Voorhees. They
/// are named \c object_iterator and \c array_iterator. The access methods for these iterators are
/// \c begin_object / \c end_object and \c begin_array / \c end_array, respectively. The object
/// interface behaves exactly like you would expect a \c std::map<std::string,jsonv::value> to,
/// while the array interface behaves just like a \c std::deque<jsonv::value> would.
///
/// \code
/// #include <jsonv/value.hpp>
/// #include <iostream>
///
/// int main()
/// {
///     jsonv::value x = jsonv::object({ { "one", 1 }});
///     auto iter = x.find("one");
///     if (iter != x.end_object())
///         std::cout << iter->first << ": " << iter->second << std::endl;
///     else
///         std::cout << "Nothing..." << std::endl;
///
///     iter = x.find("two");
///     if (iter != x.end_object())
///         std::cout << iter->first << ": " << iter->second << std::endl;
///     else
///         std::cout << "Nothing..." << std::endl;
///
///     x["two"] = 2;
///     iter = x.find("two");
///     if (iter != x.end_object())
///         std::cout << iter->first << ": " << iter->second << std::endl;
///     else
///         std::cout << "Nothing..." << std::endl;
///
///     x["two"] = jsonv::array({ "one", "+", x.at("one") });
///     iter = x.find("two");
///     if (iter != x.end_object())
///         std::cout << iter->first << ": " << iter->second << std::endl;
///     else
///         std::cout << "Nothing..." << std::endl;
///
///     x.erase("one");
///     iter = x.find("one");
///     if (iter != x.end_object())
///         std::cout << iter->first << ": " << iter->second << std::endl;
///     else
///         std::cout << "Nothing..." << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// one: 1
/// Nothing...
/// two: 2
/// two: ["one","+",1]
/// Nothing...
/// \endcode
///
/// The iterator types \e work. This means you are free to use all of the C++ things just like you
/// would a regular container. To use a ranged-based for, simply call \c as_array or \c as_object.
/// Everything from \c <algorithm> and \c <iterator> or any other library works great with JSON
/// Voorhees.
///
/// \code
/// #include <jsonv/value.hpp>
/// #include <algorithm>
/// #include <iostream>
///
/// int main()
/// {
///     jsonv::value arr = jsonv::array({ "taco", "cat", 3, -2, jsonv::null, "beef", 4.8, 5 });
///     std::cout << "Initial: ";
///     for (const auto& val : arr.as_array())
///         std::cout << val << '\t';
///     std::cout << std::endl;
///
///     std::sort(arr.begin_array(), arr.end_array());
///     std::cout << "Sorted: ";
///     for (const auto& val : arr.as_array())
///         std::cout << val << '\t';
///     std::cout << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// Initial: "taco" "cat"   3   -2  null    "beef"  4.8   5
/// Sorted:  null   -2  3   4.8 5   "beef"  "cat"   "taco"
/// \endcode
///
/// \section demo_parsing Encoding and decoding
///
/// Usually, the reason people are using JSON is as a data exchange format, either for communicating
/// with other services or storing things in a file or a database. To do this, you need to \e encode
/// your \c json::value into an \c std::string and \e parse it back. JSON Voorhees makes this easy
/// for you.
///
/// \code
/// #include <jsonv/value.hpp>
/// #include <jsonv/encode.hpp>
/// #include <jsonv/parse.hpp>
///
/// #include <iostream>
/// #include <fstream>
/// #include <limits>
///
/// int main()
/// {
///     jsonv::value obj = jsonv::object();
///     obj["taco"]  = "cat";
///     obj["array"] = jsonv::array({ 1, 2, 3, 4, 5 });
///     obj["infinity"] = std::numeric_limits<double>::infinity();
///
///     {
///         std::cout << "Saving \"file.json\"... " << obj << std::endl;
///         std::ofstream file("file.json");
///         file << obj;
///     }
///
///     jsonv::value loaded;
///     {
///         std::cout << "Loading \"file.json\"...";
///         std::ifstream file("file.json");
///         loaded = jsonv::parse(file);
///     }
///     std::cout << loaded << std::endl;
///
///     return obj == loaded ? 0 : 1;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// Saving "file.json"... {"array":[1,2,3,4,5],"infinity":null,"taco":"cat"}
/// Loading "file.json"...{"array":[1,2,3,4,5],"infinity":null,"taco":"cat"}
/// \endcode
///
/// If you are paying close attention, you might have noticed that the value for the \c "infinity"
/// looks a little bit more \c null than \c infinity. This is because, much like mathematicians
/// before Anaximander, JSON has no concept of infinity, so it is actually \e illegal to serialize a
/// token like \c infinity anywhere.
///
/// By default, when an encoder encounters an unrepresentable value in the JSON it is trying to
/// encode, it outputs \c null instead. If you wish to change this behavior, implement your own
/// \c jsonv::encoder (or derive from \c jsonv::ostream_encoder).
///
/// If you ran the example program, you might have noticed that the return code was 1, meaning the
/// value you put into the file and what you got from it were not equal. This is because all the
/// type and value information is still kept around in the in-memory \c obj. It is only upon
/// encoding that information is lost.
///
/// Getting tired of all this compact rendering of your JSON strings? Want a little more whitespace
/// in your life? Then \c jsonv::ostream_pretty_encoder is the class for you! Unlike our standard
/// \e compact encoder, this guy will put newlines and indentation in your JSON so you can present
/// it in a way more readable format.
///
/// \code
/// #include <jsonv/encode.hpp>
/// #include <jsonv/parse.hpp>
/// #include <jsonv/value.hpp>
///
/// #include <iostream>
///
/// int main()
/// {
///     // Make a pretty encoder and point to std::cout
///     jsonv::ostream_pretty_encoder prettifier(std::cout);
///     prettifier.encode(jsonv::parse(std::cin));
/// }
/// \endcode
///
/// Compile that code and you now have your own little JSON prettification program!
///
/// Not everything you want to write starts out as a \c jsonv::value. A \c jsonv::writer writes a
/// document one token at a time into any encoder, the pretty one included, and \c jsonv::serialize
/// writes a C++ object as one value wherever the writer is:
///
/// \code
/// #include <jsonv/encode.hpp>
/// #include <jsonv/serialization.hpp>
/// #include <jsonv/writer.hpp>
///
/// #include <iostream>
/// #include <string>
/// #include <vector>
///
/// int main()
/// {
///     std::vector<std::string> villains = { "Jason", "Freddy", "Michael" };
///
///     jsonv::ostream_pretty_encoder prettifier(std::cout);
///     jsonv::writer                 to(prettifier);
///     to.object_begin();
///     to.key("genre").string("slasher");
///     to.key("villains").array_begin();
///     for (const std::string& name : villains)
///         jsonv::serialize(name, to);
///     to.array_end();
///     to.object_end();
/// }
/// \endcode
///
/// Output:
///
/// \code
/// {
///   "genre": "slasher",
///   "villains": [
///     "Jason",
///     "Freddy",
///     "Michael"
///   ]
/// }
/// \endcode
///
/// The writer writes the commas and the colons, and refuses a token which does not belong where it
/// is -- a key outside an object, say -- before the encoder ever sees it. Each call to \c serialize
/// writes one more element into the array the writer has open, so the same loop could write a
/// million names without ever holding them all in a \c jsonv::value. \c serialize works for any type
/// a \c jsonv::formats knows -- here, \c jsonv::formats::global() -- and teaching one about your own
/// types is what the \ref serialization "next section" is about.
///
/// \section serialization Serialization
///
/// Most of the time, you do not want to deal with \c jsonv::value instances directly. Instead, most
/// people prefer to convert JSON into their own strong C++ \c class or \c struct. JSON Voorhees
/// provides utilities to make this easy for you to use. At the end of the day, you should be able
/// to create an arbitrary C++ type with <tt>jsonv::deserialize&lt;my_type&gt;(text)</tt> and turn
/// one back into JSON text with <tt>jsonv::serialize(my_instance)</tt> -- or into a \c jsonv::value
/// with <tt>jsonv::to_json(my_instance)</tt>.
///
/// \subsection serialization_encoding Deserializing with deserialize
///
/// Let's start with converting JSON into C++ types with <tt>jsonv::deserialize&lt;T&gt;</tt>.
///
/// \code
/// #include <jsonv/parse.hpp>
/// #include <jsonv/serialization.hpp>
/// #include <jsonv/value.hpp>
///
/// #include <iostream>
/// #include <string>
///
/// int main()
/// {
///     std::cout << "a=" << jsonv::deserialize<int>("1") << std::endl;
///     std::cout << "b=" << jsonv::deserialize<double>("2.5") << std::endl;
///     std::cout << "c=" << jsonv::deserialize<std::string>(R"("Hello!")") << std::endl;
///
///     jsonv::value val = jsonv::parse(R"({ "d": 4 })");
///     std::cout << "d=" << jsonv::deserialize<int>(val.at("d")) << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// a=1
/// b=2.5
/// c=Hello!
/// d=4
/// \endcode
///
/// The first three deserialize from JSON text, which is the spelling to reach for when text is what you
/// have: the C++ value is read straight out of the text, and no \c jsonv::value is built along the
/// way. That does mean a C++ string handed to \c deserialize is JSON \e text rather than a JSON string,
/// so <tt>deserialize&lt;std::string&gt;(R"("Hello!")")</tt> is <tt>Hello!</tt> while
/// <tt>deserialize&lt;std::string&gt;("Hello!")</tt> fails to parse. The last deserializes from a
/// \c jsonv::value, which is the spelling for JSON you have already parsed or built. Either way,
/// JSON which does not hold what you asked for -- <tt>deserialize&lt;int&gt;(R"("one")")</tt> -- throws
/// a \c jsonv::deserialization_error saying what was found instead.
///
/// Overall, this is not very complicated. We did not do anything that could not have been done
/// through a little use of \c parse and the \c as_ accessors like \c as_integer. So what is this
/// \c deserialize giving us?
///
/// The real power comes in when we start talking about \c jsonv::formats. These objects provide a
/// set of rules to encode and decode arbitrary types. So let's make a C++ \c class for our JSON
/// object and write a special constructor for it.
///
/// \code
/// #include <jsonv/serialization.hpp>
/// #include <jsonv/serialization/deserializer_construction.hpp>
///
/// #include <iostream>
/// #include <string>
/// #include <string_view>
/// #include <utility>
///
/// class my_type
/// {
/// public:
///     my_type(jsonv::reader& from, jsonv::deserialization_context& context)
///     {
///         if (!from.expect(jsonv::ast_node_type::object_begin))
///             throw jsonv::deserialization_error(context.problem_path(from), "Expected an object");
///
///         // Step off the { and onto the first key -- or onto the } of an empty object.
///         (void) from.next_token();
///         while (from.current_type() != jsonv::ast_node_type::object_end)
///         {
///             std::string key = from.current().visit_key([] (const auto& k) { return std::string(k.value()); });
///
///             // Step off the key and onto its value.
///             (void) from.next_token();
///
///             if (key == "a")
///                 a = deserialize_member<int>(from, context, "a");
///             else if (key == "b")
///                 b = deserialize_member<int>(from, context, "b");
///             else if (key == "c")
///                 c = deserialize_member<std::string>(from, context, "c");
///             else
///                 (void) from.next_value();
///         }
///
///         // Step off the } too, leaving the reader one past this object.
///         (void) from.next_token();
///     }
///
///     static const jsonv::deserializer* get_deserializer()
///     {
///         static jsonv::deserializer_construction<my_type> instance;
///         return &instance;
///     }
///
///     friend std::ostream& operator<<(std::ostream& os, const my_type& self)
///     {
///         return os << "{ a=" << self.a << ", b=" << self.b << ", c=" << self.c << " }";
///     }
///
/// private:
///     template <typename T>
///     static T deserialize_member(jsonv::reader& from, jsonv::deserialization_context& context, std::string_view key)
///     {
///         jsonv::deserialization_context::path_scope scope(context, key);
///
///         auto mark = context.problems().size();
///         if (auto result = context.deserialize<T>(from))
///             return *std::move(result);
///
///         throw jsonv::deserialization_error(context.take_problems_since(mark));
///     }
///
/// private:
///     int         a = 0;
///     int         b = 0;
///     std::string c;
/// };
///
/// int main()
/// {
///     jsonv::formats local_formats;
///     local_formats.register_deserializer(my_type::get_deserializer());
///     jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });
///
///     my_type x = jsonv::deserialize<my_type>(R"({ "a": 1, "b": 2, "c": "Hello!" })", format);
///     std::cout << x << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// { a=1, b=2, c=Hello! }
/// \endcode
///
/// There is a lot going on in that example, so let's take it one step at a time. First, we are
/// creating a \c my_type object to store our values, which is nice. Then, we gave it a
/// funny-looking constructor:
///
/// \code
///     my_type(jsonv::reader& from, jsonv::deserialization_context& context)
/// \endcode
///
/// This is a <i>deserializing constructor</i>. All that means is that it has those two arguments: a
/// \c jsonv::reader and a \c jsonv::deserialization_context. The reader is a forward cursor over the
/// JSON, and when the constructor is called it is sitting on the first token of the value to
/// deserialize from -- for \c my_type, the <tt>{</tt> of an object. From there, the constructor walks
/// the object one key at a time, in whatever order the document wrote them:
///
/// \code
///         while (from.current_type() != jsonv::ast_node_type::object_end)
///         {
///             std::string key = from.current().visit_key([] (const auto& k) { return std::string(k.value()); });
///
///             // Step off the key and onto its value.
///             (void) from.next_token();
///
///             if (key == "a")
///                 a = deserialize_member<int>(from, context, "a");
///             // ...
///             else
///                 (void) from.next_value();
///         }
/// \endcode
///
/// Each value it wants is deserialized by \c deserialize_member, which leaves the reader on the next key,
/// or on the <tt>}</tt>. A key it does not recognize has its value skipped with \c next_value,
/// which steps over the whole value in one go, however large it is. Once the closing <tt>}</tt> has
/// been stepped off as well, the reader is left one position past the object. Every deserializer
/// promises that, because it is where whatever is deserializing around this object carries on from. A
/// key the document leaves out leaves its member as it was initialized.
///
/// \code
///     template <typename T>
///     static T deserialize_member(jsonv::reader& from, jsonv::deserialization_context& context, std::string_view key)
///     {
///         jsonv::deserialization_context::path_scope scope(context, key);
///
///         auto mark = context.problems().size();
///         if (auto result = context.deserialize<T>(from))
///             return *std::move(result);
///
///         throw jsonv::deserialization_error(context.take_problems_since(mark));
///     }
/// \endcode
///
/// The \c jsonv::deserialization_context is what does the work. <tt>context.deserialize&lt;T&gt;(from)</tt>
/// deserializes a \c T from the value under the cursor, using the \c jsonv::formats the deserialization was
/// started with, and leaves the cursor one past that value. When it cannot, it does not throw: it
/// records the problem on the context and returns a \c std::unexpected. A constructor can only fail
/// by throwing, so \c deserialize_member throws a \c jsonv::deserialization_error carrying what the context
/// recorded -- \e taking it with \c take_problems_since rather than copying it, so the problem is
/// reported once. The \c path_scope names the member for as long as it is being deserialized, which is
/// what puts a problem with \c "a" at <tt>.a</tt> -- or at <tt>[3].a</tt> when the \c my_type is
/// the fourth element of an array.
///
/// Giving up at the first problem is what deserialization does by default. With
/// \c jsonv::deserialize_options::on_error::collect_all, it carries on past a problem so that it can
/// report as many as it finds, and a deserializer which walks the reader itself has more to do for
/// that to work -- see \c jsonv::deserialization_context::recover. The
/// \ref serialization_composition "DSL" described below does all of that for you.
///
/// \code
///     static const jsonv::deserializer* get_deserializer()
///     {
///         static jsonv::deserializer_construction<my_type> instance;
///         return &instance;
///     }
/// \endcode
///
/// A \c jsonv::deserializer is a type that knows how to read JSON and create some C++ type out of it.
/// In this case, we are creating a \c jsonv::deserializer_construction, which is a subtype that knows
/// how to call the constructor of a type. There are all sorts of \c jsonv::deserializer
/// implementations in \c jsonv/serialization/, so you should be able to find one that fits your
/// needs.
///
/// \code
///     jsonv::formats local_formats;
///     local_formats.register_deserializer(my_type::get_deserializer());
///     jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });
/// \endcode
///
/// Now things are starting to get interesting. The \c jsonv::formats object is a collection of
/// <tt>jsonv::deserializer</tt>s, so we create one of our own and add the \c jsonv::deserializer* from
/// the static function of \c my_type. The \c local_formats \e only knows how to deserialize instances
/// of \c my_type -- it does \e not know even the most basic things like how to deserialize an \c int.
/// We use \c jsonv::formats::compose to create a new instance of \c jsonv::formats that combines
/// the qualities of \c local_formats (which knows how to deal with \c my_type) and the
/// \c jsonv::formats::defaults (which knows how to deal with things like \c int and
/// \c std::string). The \c formats instance now has the power to do everything we need!
///
/// \code
///     my_type x = jsonv::deserialize<my_type>(R"({ "a": 1, "b": 2, "c": "Hello!" })", format);
/// \endcode
///
/// This is not terribly different from the example before, but now we are explicitly passing a
/// \c jsonv::formats object to the function. If we had not provided \c format as an argument here,
/// the function would have thrown a \c jsonv::deserialization_error complaining about how it did not
/// know how to deserialize a \c my_type.
///
/// When the JSON came from a file, an error is more use if it says which file. Build the
/// \c jsonv::deserialization_context yourself, with the name of the source last, and hand it to
/// \c deserialize in place of the \c format:
///
/// \code
///     jsonv::deserialization_context context(format,
///                                       std::nullopt,
///                                       jsonv::path(),
///                                       nullptr,
///                                       jsonv::deserialize_options(),
///                                       "my_type.json"
///                                      );
///     my_type y = jsonv::deserialize<my_type>(R"({ "a": 1, "b": "two", "c": "Hello!" })", context);
/// \endcode
///
/// The \c "b" is not an \c int, so this throws a \c jsonv::deserialization_error reading
/// <tt>Deserialization error at my_type.json#.b: Read node of type string when expecting integer</tt>.
/// Write out every argument before the name: the \c nullptr is the user data, and a string in its
/// place would be taken for user data rather than for a name. A context is meant for one
/// document, so make a new one for each file.
///
/// If you are coming from JSON Voorhees 1.x, you may be looking for \c extract_sub. A deserializing
/// constructor used to be handed a whole \c jsonv::value and pull each member out of it by name,
/// which is what \c extraction_context::extract_sub did. It is gone because that \c value is gone:
/// deserialization reads the JSON as it goes rather than building a \c value first, and a forward cursor
/// has no way to look a key up. Walking the keys, as \c my_type does, is what replaces it. When
/// random access is genuinely wanted -- what one member means depends on another written after it,
/// say -- read the object into a \c jsonv::value and look things up in that, with \c value::find or
/// \c value::at_path. A deserializing constructor can still take a <tt>const jsonv::value&</tt> in
/// place of the reader for exactly this, and is handed the object as a \c value -- read off the
/// reader with \c jsonv::read_value, if it was not one already. That keeps the \c extract_sub calls
/// of a 1.x constructor easy to port:
///
/// \code
///     my_type(const jsonv::value& from, jsonv::deserialization_context& context) :
///             a(deserialize_member<int>(from, context, "a")),
///             b(deserialize_member<int>(from, context, "b")),
///             c(deserialize_member<std::string>(from, context, "c"))
///     { }
///
///     template <typename T>
///     static T deserialize_member(const jsonv::value&              from,
///                                 jsonv::deserialization_context& context,
///                                 const std::string&              key
///                                )
///     {
///         jsonv::deserialization_context::path_scope scope(context, key);
///
///         auto member = from.find(key);
///         if (member == from.end_object())
///             throw jsonv::deserialization_error(context.path(), "Missing required member");
///
///         return context.deserialize<T>(member->second);
///     }
/// \endcode
///
/// The \c path_scope is what says where a problem is, since a \c value has no idea where in the
/// document it came from. That includes a member which is not there at all: looking it up with
/// \c find rather than \c value::at is what reports a missing \c "b" at <tt>.b</tt>, while the
/// scope is still alive to say so. The \c std::out_of_range from \c at would only be caught once
/// the scope had gone, so it would be reported at the object around the member, and it does not
/// say which member it was. <tt>context.deserialize&lt;T&gt;</tt> throws rather than returning when it
/// is handed a \c value, so nothing needs handing over. Building that \c value for every object
/// deserialized is the cost the reader-based constructor avoids.
///
/// \subsection serialization_to_json Serializing with serialize and to_json
///
/// JSON Voorhees also converts from your C++ structures into JSON, using \c jsonv::serialize for
/// JSON text and \c jsonv::to_json for a \c jsonv::value. It should feel like a mirror of
/// \c jsonv::deserialize, with similar argument types and many shared concepts. Just like
/// deserialization, both use the \c jsonv::formats class, but they look up a \c jsonv::serializer
/// in it to convert from C++ into JSON. Where a deserializer reads from a \c jsonv::reader, a
/// serializer writes into a \c jsonv::writer.
///
/// \code
/// #include <jsonv/serialization.hpp>
/// #include <jsonv/serialization/function_serializer.hpp>
/// #include <jsonv/value.hpp>
/// #include <jsonv/writer.hpp>
///
/// #include <iostream>
/// #include <string>
/// #include <utility>
///
/// class my_type
/// {
/// public:
///     my_type(int a, int b, std::string c) :
///             a(a),
///             b(b),
///             c(std::move(c))
///     { }
///
///     static const jsonv::serializer* get_serializer()
///     {
///         static auto instance = jsonv::make_serializer<my_type>
///                                (
///                                 [] (const jsonv::serialization_context& context,
///                                     const my_type&                      self,
///                                     jsonv::writer&                      to
///                                    )
///                                 {
///                                     to.object_begin();
///                                     to.key("a");
///                                     context.serialize(self.a, to);
///                                     to.key("b");
///                                     context.serialize(self.b, to);
///                                     to.key("c");
///                                     context.serialize(self.c, to);
///                                     to.object_end();
///                                 }
///                                );
///         return &instance;
///     }
///
/// private:
///     int         a;
///     int         b;
///     std::string c;
/// };
///
/// int main()
/// {
///     jsonv::formats local_formats;
///     local_formats.register_serializer(my_type::get_serializer());
///     jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });
///
///     my_type x(5, 6, "Hello");
///     std::cout << jsonv::serialize(x, format) << std::endl;
///
///     jsonv::value tree = jsonv::to_json(x, format);
///     std::cout << tree.at("c") << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// {"a":5,"b":6,"c":"Hello"}
/// "Hello"
/// \endcode
///
/// The serializer is handed a \c jsonv::writer positioned where the \c my_type goes -- at the root
/// here, but just as well as the element of an array or the value of another object's member -- and
/// writes exactly one value there. It opens an object, and for each member writes the key and then
/// hands the member to <tt>context.serialize</tt>, which looks up the serializer for the member's
/// type in the same \c jsonv::formats and has it write the member where the writer now is. The
/// writer writes the punctuation, and refuses a token which does not belong where it is: a key
/// outside an object, say, or a value inside one with no key before it.
///
/// \c jsonv::serialize writes \c x as compact JSON text with nothing built in between: no
/// \c jsonv::value is made along the way, and the members come out in the order the serializer
/// wrote them. \c jsonv::to_json runs the same serializer into a \c jsonv::value_encoder and hands
/// back the tree it built, for when a \c jsonv::value is what you want -- to look things up in, as
/// here, or to change before writing it out. A \c jsonv::value keeps an object's members sorted by
/// key, so the text of one can list them in a different order from \c serialize; for \c my_type,
/// they happen to agree. To write the text to a stream rather than into a \c std::string, there is
/// <tt>jsonv::serialize(x, std::cout, format)</tt>; and <tt>jsonv::serialize(x, to, format)</tt>
/// writes \c x into a \c jsonv::writer of your own, as in \ref demo_parsing "Encoding and decoding".
///
/// A serializer written for JSON Voorhees 1.x returned a \c jsonv::value rather than writing one.
/// That still works: \c jsonv::make_serializer also takes a function of
/// <tt>(const jsonv::serialization_context&, const T&)</tt>, or of just the <tt>const T&</tt>,
/// which returns a \c jsonv::value to be written whole. Building that \c value for every object
/// serialized is the cost the writer avoids.
///
/// \subsection serialization_composition Composing Type Adapters
///
/// Does all this seem a little bit \e manual to you? Creating a \c deserializer and \c serializer for
/// every single type can get a little bit tedious. Unfortunately, until C++ has a standard way to
/// do reflection, we must specify the conversions manually. However, there \e is an easier way!
/// That way is the \ref serialization_builder_dsl "Serialization Builder DSL".
///
/// Let's start with a couple of simple structures:
///
/// \code
/// struct foo
/// {
///     int         a;
///     int         b;
///     std::string c;
/// };
///
/// struct bar
/// {
///     foo         x;
///     foo         y;
///     std::string z;
///     std::string w;
/// };
/// \endcode
///
/// Let's make a \c formats for them using the DSL:
///
/// \code
/// jsonv::formats formats =
///     jsonv::formats_builder()
///         .type<foo>()
///             .member("a", &foo::a)
///             .member("b", &foo::b)
///                 .default_value(10)
///             .member("c", &foo::c)
///         .type<bar>()
///             .member("x", &bar::x)
///             .member("y", &bar::y)
///             .member("z", &bar::z)
///                 .since(jsonv::version(2, 0))
///             .member("w", &bar::w)
///                 .until(jsonv::version(5, 0))
///         .compose_checked(jsonv::formats::defaults())
///     ;
/// \endcode
///
/// What is going on there? The giant chain of function calls is building up a collection of type
/// adapters into a \c formats for you. The indentation shows the intent -- the
/// <tt>.member("a", &foo::a)</tt> is attached to the type \c adapter for \c foo (if you tried to
/// specify \c &bar::y in that same place, it would fail to compile). Each function call returns a
/// reference back to the builder so you can chain as many of these together as you want to. The
/// \c jsonv::formats_builder is a proper object, so if you wish to spread out building your type
/// adapters into multiple functions, you can do that by passing around an instance.
///
/// The two most-used functions are \c type and \c member. \c type defines a \c jsonv::adapter for
/// the C++ class provided at the template parameter. All of the calls before the second \c type
/// call modify the adapter for \c foo. There, we attach members with the \c member function. This
/// tells the \c formats how to encode and deserialize each of the specified members to and from a JSON
/// object using the provided string as the key. The extra function calls like \c default_value,
/// \c since and \c until are just a couple of the many functions available to modify how the
/// members of the type get transformed.
///
/// The chain ends with \c compose_checked, which checks that every type the members refer to --
/// \c int and \c std::string here -- can be deserialized and serialized once the adapters the DSL
/// built are combined with \c jsonv::formats::defaults, and then composes the two, just as we
/// composed \c local_formats by hand earlier.
///
/// The \c formats we built would be perfectly capable of serializing to and deserializing from this
/// JSON document:
///
/// \code
/// {
///     "x": { "a": 50, "b": 20, "c": "Blah" },
///     "y": { "a": 10,          "c": "No B?" },
///     "z": "Only serialized in 2.0+",
///     "w": "Only serialized before 5.0"
/// }
/// \endcode
///
/// Deserializing a \c bar reads the document just the way the constructor of \c my_type did: each
/// object's keys are walked in the order the document wrote them, each is handed to the member
/// which claims it, and a key no member claims has its value stepped over unread. What the DSL adds
/// is everything that constructor left out. A member without a \c default_value is required, a key
/// the document repeats is settled by \c jsonv::deserialize_options::on_duplicate_key, and
/// \c jsonv::deserialize_options::on_error::collect_all carries on past a problem to report the rest.
///
/// For a more in-depth reference, see the \ref serialization_builder_dsl "Serialization Builder DSL page".
///
/// \section demo_algorithm Algorithms
///
/// JSON Voorhees takes a "batteries included" approach. A few building blocks for powerful
/// operations can be found in the \c algorithm.hpp header file.
///
/// One of the simplest operations you can perform is the \c map operation. This operation takes in
/// some \c jsonv::value and returns another. Let's try it.
///
/// \code
/// #include <jsonv/algorithm.hpp>
/// #include <jsonv/value.hpp>
///
/// #include <iostream>
///
/// int main()
/// {
///     jsonv::value x = 5;
///     std::cout << jsonv::map([] (const jsonv::value& y) { return y.as_integer() * 2; }, x) << std::endl;
/// }
/// \endcode
///
/// If everything went right, you should see a number:
///
/// \code
/// 10
/// \endcode
///
/// That is not the most interesting example of using \c map, but it is enough to get the general
/// idea of what is going on. This operation is so common that it is a member function of \c value
/// as \c jsonv::value::map. Let's make things a bit more interesting and \c map an \c array...
///
/// \code
/// #include <jsonv/value.hpp>
///
/// #include <iostream>
///
/// int main()
/// {
///     std::cout << jsonv::array({ 1, 2, 3, 4, 5 })
///                         .map([] (const jsonv::value& y) { return y.as_integer() * 2; })
///               << std::endl;
/// }
/// \endcode
///
/// Now we're starting to get somewhere!
///
/// \code
/// [2,4,6,8,10]
/// \endcode
///
/// The \c map function maps over whatever the contents of the \c jsonv::value happens to be and
/// returns something for you based on the \c kind. This simple concept is so ubiquitous that
/// <a href="http://www.disi.unige.it/person/MoggiE/"> Eugenio Moggi</a> named it a
/// <a href="http://stackoverflow.com/questions/44965/what-is-a-monad">monad</a>. If you're feeling
/// adventurous, try using \c map with an \c object or chaining multiple \c map operations together.
///
/// Another common building block is the function \c jsonv::traverse. This function walks a JSON
/// structure and calls a some user-provided function.
///
/// \code
/// #include <jsonv/algorithm.hpp>
/// #include <jsonv/parse.hpp>
/// #include <jsonv/value.hpp>
///
/// #include <iostream>
///
/// int main()
/// {
///     jsonv::traverse(jsonv::parse(std::cin),
///                     [] (const jsonv::path& path, const jsonv::value& value)
///                     {
///                         std::cout << path << " => " << value << std::endl;
///                     },
///                     true
///                    );
/// }
/// \endcode
///
/// Now we have a tiny little program to decompose JSON into <a href="https://jqlang.org/">jq</a>
/// style path expressions and their values. For example, if you pipe
/// <tt>{ "bar": [1, 2, 3], "foo": "hello" }</tt> into the program:
///
/// \code
/// .bar[0] => 1
/// .bar[1] => 2
/// .bar[2] => 3
/// .foo => "hello"
/// \endcode
///
/// All of the \e really powerful functions can be found in \c algorithm.hpp. My personal favorite
/// is \c jsonv::merge. The idea is simple: it merges two (or more) JSON values into one.
///
/// \code
/// #include <jsonv/algorithm.hpp>
/// #include <jsonv/value.hpp>
///
/// #include <iostream>
///
/// int main()
/// {
///     jsonv::value a = jsonv::object({ { "a", "taco" }, { "b", "cat" } });
///     jsonv::value b = jsonv::object({ { "c", "burrito" }, { "d", "dog" } });
///     jsonv::value merged = jsonv::merge(std::move(a), std::move(b));
///     std::cout << merged << std::endl;
/// }
/// \endcode
///
/// Output:
///
/// \code
/// {"a":"taco","b":"cat","c":"burrito","d":"dog"}
/// \endcode
///
/// You might have noticed the use of \c std::move into the \c merge function. Like most functions
/// in JSON Voorhees, \c merge takes advantage of move semantics. In this case, the implementation
/// will move the contents of the values instead of copying them around. While it may not matter in
/// this simple case, if you have large JSON structures, the support for movement will save you a
/// ton of memory.
///
/// \see https://github.com/tgockel/json-voorhees
/// \see http://json.org/

}

#include "algorithm.hpp"
#include "ast.hpp"
#include "coerce.hpp"
#include "config.hpp"
#include "demangle.hpp"
#include "encode.hpp"
#include "forward.hpp"
#include "functional.hpp"
#include "kind.hpp"
#include "optional.hpp"
#include "parse.hpp"
#include "parse_index.hpp"
#include "path.hpp"
#include "reader.hpp"
#include "serialization.hpp"
#include "serialization_builder.hpp"
#include "serialization/all.hpp"
#include "value.hpp"
#include "version.hpp"
#include "writer.hpp"
