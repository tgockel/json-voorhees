/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// jsonv is compiled with the harness's memorystat.h, so its malloc, realloc and free calls are calls to these. The
/// benchmark program gets them from memorystat.cpp, which counts; jsonstat_jsonvtest measures code size, not memory,
/// and gets them from here.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <cstdlib>

extern "C"
{

void* MemoryStatMalloc(std::size_t size)
{
    return std::malloc(size);
}

void* MemoryStatCalloc(std::size_t num, std::size_t size)
{
    return std::calloc(num, size);
}

void* MemoryStatRealloc(void* ptr, std::size_t size)
{
    return std::realloc(ptr, size);
}

void MemoryStatFree(void* ptr)
{
    std::free(ptr);
}

}
