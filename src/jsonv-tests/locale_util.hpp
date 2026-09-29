/// \file
/// Running a test under a locale which would change how a stream writes numbers.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <locale>
#include <string>

namespace jsonv_test
{

/// Digits grouped in threes with a `,`, which a stream applies to every number inserted into it while this is part of
/// its locale.
class grouping_numpunct final :
        public std::numpunct<char>
{
protected:
    virtual char do_thousands_sep() const override
    {
        return ',';
    }

    virtual std::string do_grouping() const override
    {
        return "\3";
    }
};

/// Make \c grouping_numpunct part of the global locale for as long as this lives, which is what every stream built in
/// the meantime starts out with. Restored on the way out however that happens, so that a failure part-way through a
/// test does not leave every test after it running under a different locale.
class global_grouping_locale final
{
public:
    global_grouping_locale() :
            _previous(std::locale::global(std::locale(std::locale::classic(), new grouping_numpunct)))
    { }

    global_grouping_locale(const global_grouping_locale&)            = delete;
    global_grouping_locale& operator=(const global_grouping_locale&) = delete;

    ~global_grouping_locale() noexcept
    {
        std::locale::global(_previous);
    }

private:
    std::locale _previous;
};

}
