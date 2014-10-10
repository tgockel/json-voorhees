/** \file jsonv/all.hpp
 *  A head which includes all other JSON Voorhees headers.
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
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
 *  developer-friendliness, a high-speed parser and no dependencies beyond compliant compiler.
 * 
 *  Features include (but are not necessarily limited to):
 *  
 *   - A simple system for the representation of JSON values
 *   - Documentation consumable by human beings that answers questions you might actually ask
 *   - Simple JSON parsing with \c parse or use the \c tokenizer system and customize to your heart's desires
 *   - Write a JSON value with \c operator<< or use the \c encoder API for advanced output options
 *   - Reasonable error messages when parsing fails
 *   - Full support for Unicode-filled JSON (encoded in UTF-8 in C++)
 *   - Safe: in the best case, illegal code should fail to compile; in the worst case, an illegal action should throw an
 *     exception
 *   - Stable: worry less about upgrading -- the API and ABI will not change out from under you
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
 *      std::cout << x.as_string() << "\tis the same as\t" << x << std::endl;
 *  }
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
 *          std::cout << "Saving \"file.json\"..." << obj << std::endl;
 *          std::ofstream file("file.json");
 *          file << obj;
 *      }
 *      
 *      jsonv::value loaded;
 *      {
 *          std::cout << "Loading \"file.json\"...";
 *          std::ifstream file("file.json");
 *          loaded = jsonv::parse(file);
 *          std::cout << loaded << std::endl;
 *      }
 *      
 *      return obj == loaded ? 0 : 1;
 *  }
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
 *  \see https://github.com/tgockel/json-voorhees
 *  \see http://json.org/
**/

#include "coerce.hpp"
#include "config.hpp"
#include "encode.hpp"
#include "forward.hpp"
#include "parse.hpp"
#include "string_view.hpp"
#include "tokenizer.hpp"
#include "value.hpp"

#endif/*__JSONV_ALL_HPP_INCLUDED__*/
