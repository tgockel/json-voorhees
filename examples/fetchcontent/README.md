# Consuming JSON Voorhees with FetchContent

`CMakeLists.txt` here is the snippet from the top-level README: declare `jsonv`, make it available, link
`jsonv::jsonv`. There are three ways to configure it, depending on where the library should come from.

As written, `find_package(jsonv)` is tried first and the tag is cloned only if that fails:

```sh
cmake -S . -B build
cmake --build build
./build/jsonv-example
```

Against a checkout of this repository, for instance to test a change before it is tagged. Pointing FetchContent at a
source directory also skips the `find_package` attempt:

```sh
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_JSONV=/path/to/json-voorhees
```

Against an installed copy only, failing rather than downloading if it is not found:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

Whichever way the library arrives, `jsonv::jsonv` carries everything a consumer needs: the include directory, the
C++23 requirement, and `JSONV_SO=0` when the library is static.
