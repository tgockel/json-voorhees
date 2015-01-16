/** \file jsonv/all.hpp
 *  A head which includes all other JSON Voorhees headers.
 *  
 *  Copyright (c) 2012-2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_ALL_HPP_INCLUDED__
#define __JSONV_ALL_HPP_INCLUDED__

/** \mainpage JSON Voorhees
 *  JSON Voorhees is yet another library for parsing JSON in C++. This one touts new C++11 features for
 *  developer-friendliness, a high-speed parser and no dependencies beyond compliant compiler. It is hosted on
 *  <a href="https://github.com/tgockel/json-voorhees">GitHub</a> and sports an Apache License, so use it anywhere you
 *  need.
 * 
 *  Features include (but are not necessarily limited to):
 *  
 *   - Simple
 *     - A `value` should not feel terribly different from a C++ Standard Library container
 *     - Write valid JSON with `operator<<`
 *     - Simple JSON parsing with `parse`
 *     - Reasonable error messages when parsing fails
 *     - Full support for Unicode-filled JSON (encoded in UTF-8 in C++)
 *   - Efficient
 *     - Minimal overhead to store values (a `value` is 16 bytes on a 64-bit platform)
 *     - No-throw move semantics wherever possible
 *   - Safe
 *     - In the best case, illegal code should fail to compile
 *     - An illegal action should throw an exception
 *     - Almost all utility functions have a <a href="http://www.gotw.ca/gotw/082.htm">strong exception guarantee</a>.
 *   - Stable
 *     - Worry less about upgrading -- the API and ABI will not change out from under you
 *   - Documented
 *     - Consumable by human beings
 *     - Answers questions you might actually ask
 *  
 *  \dotfile doc/conversions.dot
 *  
 *  \section usage Using the library
 *  
 *  JSON Voorhees is designed with ease-of-use in mind. So let's look at some code!
 *  
 *  \subsection demo_value The jsonv::value
 *  
 *  The central class of JSON Voorhees is the \c value. This class represents a JSON AST and is somewhat of a dynamic
 *  type. This can make things a little bit awkward for C++ programmers who are used to static typing. Don't worry about
 *  it -- you can learn to love it.
 *  
 *  Putting values of different types is super-easy.
 *  
 *  \code
 *  #include <jsonv/value.hpp>
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      jsonv::value x = nullptr;
 *      std::cout << x << std::endl;
 *      x = 5.9;
 *      std::cout << x << std::endl;
 *      x = -100;
 *      std::cout << x << std::endl;
 *      x = "something else";
 *      std::cout << x << std::endl;
 *      x = jsonv::array({ "arrays", "of", "the", 7, "different", "types"});
 *      std::cout << x << std::endl;
 *      x = jsonv::object({
 *                          { "objects", jsonv::array({
 *                                                     "Are fun, too.",
 *                                                     "Do what you want."
 *                                                   })
 *                          },
 *                          { "compose like", "standard library maps" },
 *                       });
 *      std::cout << x << std::endl;
 *  }
 *  \endcode
 *  
 *  Output:
 *  
 *  \code
 *  null
 *  5.9
 *  -100
 *  "something else"
 *  ["arrays","of","the",7,"different","types"]
 *  {"compose like":"standard library maps","objects":["Are fun, too.","Do what you want."]}
 *  \endcode
 *  
 *  If that isn't convenient enough for you, there is a user-defined literal \c _json in the \c jsonv namespace you can
 *  use
 *  
 *  \code
 *  // You can use this hideous syntax if you do not want to bring in the whole jsonv namespace:
 *  using jsonv::operator"" _json;
 *  
 *  jsonv::value x = R"({
 *                        "objects": [ "Are fun, too.",
 *                                     "Do what you want."
 *                                   ],
 *                        "compose like": "You are just writing JSON",
 *                        "which I guess": ["is", "also", "neat"]
 *                     })"_json;
 *  \endcode
 *  
 *  JSON is dynamic, which makes value access a bit more of a hassle, but JSON Voorhees aims to make it not too
 *  horrifying for you. A \c jsonv::value has a number of accessor methods named things like \c as_integer and
 *  \c as_string which let you access the value as if it was that type. But what if it isn't that type? In that case,
 *  the function will throw a \c jsonv::kind_error with a bit more information as to what rule you violated.
 *  
 *  \code
 *  #include <jsonv/value.hpp>
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      jsonv::value x = nullptr;
 *      try
 *      {
 *          x.as_string();
 *      }
 *      catch (const jsonv::kind_error& err)
 *      {
 *          std::cout << err.what() << std::endl;
 *      }
 *      
 *      x = "now make it a string";
 *      std::cout << x.as_string().size() << std::endl;
 *      std::cout << x.as_string() << "\tis not the same as\t" << x << std::endl;
 *  }
 *  \endcode
 *  
 *  Output:
 *  
 *  \code
 *  Unexpected type: expected string but found null.
 *  20
 *  now make it a string    is not the same as  "now make it a string"
 *  \endcode
 *  
 *  You can also deal with container types in a similar manner that you would deal with the equivalent STL container
 *  type, with some minor caveats. Because the \c value_type of a JSON object and JSON array are different, they have
 *  different iterator types in JSON Voorhees. They are aptly-named \c object_iterator and \c array_iterator. The access
 *  methods for these iterators are \c begin_object / \c end_object and \c begin_array / \c end_array, respectively.
 *  The object interface behaves exactly like you would expect a \c std::map<std::string,jsonv::value> to, while the
 *  array interface behaves just like a \c std::deque<jsonv::value> would.
 *  
 *  \code
 *  #include <jsonv/value.hpp>
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      jsonv::value x = jsonv::object({ { "one", 1 }});
 *      auto iter = x.find("one");
 *      if (iter != x.end_object())
 *          std::cout << iter->first << ": " << iter->second << std::endl;
 *      else
 *          std::cout << "Nothing..." << std::end;
 *      
 *      iter = x.find("two");
 *      if (iter != x.end_object())
 *          std::cout << iter->first << ": " << iter->second << std::endl;
 *      else
 *          std::cout << "Nothing..." << std::end;
 *      
 *      x["two"] = 2;
 *      iter = x.find("two");
 *      if (iter != x.end_object())
 *          std::cout << iter->first << ": " << iter->second << std::endl;
 *      else
 *          std::cout << "Nothing..." << std::end;
 *      
 *      x["two"] = jsonv::array({ "one", "+", x.at("one") });
 *      iter = x.find("two");
 *      if (iter != x.end_object())
 *          std::cout << iter->first << ": " << iter->second << std::endl;
 *      else
 *          std::cout << "Nothing..." << std::end;
 *      
 *      x.erase("one");
 *      iter = x.find("one");
 *      if (iter != x.end_object())
 *          std::cout << iter->first << ": " << iter->second << std::endl;
 *      else
 *          std::cout << "Nothing..." << std::end;
 *  }
 *  \endcode
 *  
 *  Output:
 *  
 *  \code
 *  one: 1
 *  Nothing...
 *  two: 2
 *  two: ["one","+",1]
 *  Nothing...
 *  \endcode
 *  
 *  The iterator types \e work. This means you are free to use all of the C++ things just like you would a regular
 *  container. To use a ranged-based for, simply call \c as_array or \c as_object. Everything from \c <algorithm> and
 *  \c <iterator> or any other library works great with JSON Voorhees. Bring those templates on!
 *  
 *  \code
 *  #include <jsonv/value.hpp>
 *  #include <algorithm>
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      jsonv::value arr = jsonv::array({ "taco", "cat", 3, -2, nullptr, "beef", 4.8, 5 });
 *      std::cout << "Initial: ";
 *      for (const auto& val : arr.as_array())
 *          std::cout << val << '\t';
 *      std::cout << std::endl;
 *      
 *      std::sort(arr.begin_array(), arr.end_array());
 *      std::cout << "Sorted: ";
 *      for (const auto& val : arr.as_array())
 *          std::cout << val << '\t';
 *      std::cout << std::endl;
 *  }
 *  \endcode
 *  
 *  Output:
 *  
 *  \code
 *  Initial: "taco" "cat"   3   -2  null    "beef"  4.8   5
 *  Sorted:  null   -2  3   4.8 5   "beef"  "cat"   "taco"
 *  \endcode
 *  
 *  \subsection demo_parsing Encoding and decoding
 *  
 *  Usually, the reason people are using JSON is as a data exchange format, either for communicating with other services
 *  or storing things in a file or a database. JSON Voorhees makes this very easy for you.
 *  
 *  \code
 *  #include <jsonv/value.hpp>
 *  #include <jsonv/encode.hpp>
 *  #include <jsonv/parse.hpp>
 *  
 *  #include <iostream>
 *  #include <fstream>
 *  #include <limits>
 *  
 *  int main()
 *  {
 *      jsonv::value obj = jsonv::object();
 *      obj["taco"]  = "cat";
 *      obj["array"] = jsonv::array({ 1, 2, 3, 4, 5 });
 *      obj["infinity"] = std::numeric_limits<double>::infinity();
 *      
 *      {
 *          std::cout << "Saving \"file.json\"... " << obj << std::endl;
 *          std::ofstream file("file.json");
 *          file << obj;
 *      }
 *      
 *      jsonv::value loaded;
 *      {
 *          std::cout << "Loading \"file.json\"...";
 *          std::ifstream file("file.json");
 *          loaded = jsonv::parse(file);
 *      }
 *      std::cout << loaded << std::endl;
 *      
 *      return obj == loaded ? 0 : 1;
 *  }
 *  \endcode
 *  
 *  Output:
 *  
 *  \code
 *  Saving "file.json"... {"array":[1,2,3,4,5],"infinity":null,"taco":"cat"}
 *  Loading "file.json"...{"array":[1,2,3,4,5],"infinity":null,"taco":"cat"}
 *  \endcode
 *  
 *  If you are paying close attention, you might have notice that the value for the \c "infinity" looks a little bit
 *  more \c null than \c infinity. This is because, much like mathematicians before Anaximander, JSON has no concept of
 *  infinity, so it is actually \e illegal to serialize a token like \c infinity anywhere. By default, when an encoder
 *  encounters an unrepresentable value in the JSON it is trying to encode, it outputs \c null instead. If you wish to
 *  change this behavior, implement your own \c jsonv::encoder (or derive from \c jsonv::ostream_encoder). If you ran
 *  the example program, you might have noticed that the return code was 1, meaning the value you put into the file and
 *  what you got from it were not equal. This is because all the type and value information is still kept around in the
 *  in-memory \c obj. It is only upon encoding that information is lost.
 *  
 *  Getting tired of all this compact rendering of your JSON strings? Want a little more whitespace in your life? Then
 *  \c jsonv::ostream_pretty_encoder is the class for you! Unlike our standard \e compact encoder, this guy will put
 *  newlines and indentation in your JSON so you can present it in a way more readable format.
 *  
 *  \code
 *  #include <jsonv/encode.hpp>
 *  #include <jsonv/parse.hpp>
 *  #include <jsonv/value.hpp>
 *  
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      // Make a pretty encoder and point to std::cout
 *      jsonv::ostream_pretty_encoder prettifier(std::cout);
 *      prettifier.encode(jsonv::parse(std::cin));
 *  }
 *  \endcode
 *  
 *  Compile that code and you now have your own little JSON prettification program!
 *  
 *  \subsection demo_algorithm Algorithms
 *  
 *  JSON Voorhees takes a "batteries included" approach. A few building blocks for powerful operations can be found in
 *  the \c algorithm.hpp header file.
 *  
 *  One of the simplest operations you can perform is the \c map operation. This operation takes in some \c jsonv::value
 *  and returns another. Let's try it.
 *  
 *  \code
 *  #include <jsonv/algorithm.hpp>
 *  #include <jsonv/value.hpp>
 *  
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      jsonv::value x = 5;
 *      std::cout << jsonv::map([] (const jsonv::value& y) { return y.as_integer() * 2; }, x) << std::endl;
 *  }
 *  \endcode
 *  
 *  If everything went right, you should see a number:
 *  
 *  \code
 *  10
 *  \endcode
 *  
 *  Okay, so that was not very interesting. To be fair, that is not the most interesting example of using \c map, but it
 *  is enough to get the general idea of what is going on. This operation is so common that it is a member function of
 *  \c value as \c jsonv::value::map. Let's make things a bit more interesting and \c map an \c array...
 *  
 *  \code
 *  #include <jsonv/value.hpp>
 *  
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      std::cout << jsonv::array({ 1, 2, 3, 4, 5 })
 *                          .map([] (const jsonv::value& y) { return y.as_integer() * 2; })
 *                << std::endl;
 *  }
 *  \endcode
 *  
 *  Now we're starting to get somewhere!
 *  
 *  \code
 *  [2,4,6,8,10]
 *  \endcode
 *  
 *  The \c map function maps over whatever the contents of the \c jsonv::value happens to be and returns something for
 *  you based on the \c kind. This simple concept is so ubiquitous that <a href="http://www.disi.unige.it/person/MoggiE/">
 *  Eugenio Moggi</a> named it a <a href="http://stackoverflow.com/questions/44965/what-is-a-monad">monad</a>. If you're
 *  feeling adventurous, try using \c map with an \c object or chaining multiple \c map operations together.
 *  
 *  Another common building block is the function \c jsonv::traverse. This function walks a JSON structure and calls a
 *  some user-provided function.
 *  
 *  \code
 *  #include <jsonv/algorithm.hpp>
 *  #include <jsonv/parse.hpp>
 *  #include <jsonv/value.hpp>
 *  
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      jsonv::traverse(jsonv::parse(std::cin),
 *                      [] (const jsonv::path& path, const jsonv::value& value)
 *                      {
 *                          std::cout << path << "=" << value << std::endl;
 *                      },
 *                      true
 *                     );
 *  }
 *  \endcode
 *  
 *  Now we have a tiny little program! Here's what happens when I pipe <tt>{ "bar": [1, 2, 3], "foo": "hello" }</tt>
 *  into the program:
 *  
 *  \code
 *  .bar[0]=1
 *  .bar[1]=2
 *  .bar[2]=3
 *  .foo="hello"
 *  \endcode
 *  
 *  Imagine the possibilities!
 *  
 *  All of the \e really powerful functions can be found in \c util.hpp. My personal favorite is \c jsonv::merge. The
 *  idea is simple: it merges two (or more) JSON values into one.
 *  
 *  \code
 *  #include <jsonv/util.hpp>
 *  #include <jsonv/value.hpp>
 *  
 *  #include <iostream>
 *  
 *  int main()
 *  {
 *      jsonv::value a = jsonv::object({ { "a", "taco" }, { "b", "cat" } });
 *      jsonv::value b = jsonv::object({ { "c", "burrito" }, { "d", "dog" } });
 *      jsonv::value merged = jsonv::merge(std::move(a), std::move(b));
 *      std::cout << merged << std::endl;
 *  }
 *  \endcode
 *  
 *  Output:
 *  
 *  \code
 *  {"a":"taco","b":"cat","c":"burrito","d":"dog"}
 *  \endcode
 *  
 *  You might have noticed the use of \c std::move into the \c merge function. \c merge, like most functions in JSON
 *  Voorees, takes advantage of move semantics. In this case, the implementation will move the contents of the values
 *  instead of copying them around. While it may not matter in this simple case, if you have large JSON structures, the
 *  support for movement will save you a ton of memory.
 *  
 *  \see https://github.com/tgockel/json-voorhees
 *  \see http://json.org/
**/

#include "algorithm.hpp"
#include "coerce.hpp"
#include "config.hpp"
#include "encode.hpp"
#include "forward.hpp"
#include "parse.hpp"
#include "path.hpp"
#include "string_view.hpp"
#include "tokenizer.hpp"
#include "util.hpp"
#include "value.hpp"

#endif/*__JSONV_ALL_HPP_INCLUDED__*/
