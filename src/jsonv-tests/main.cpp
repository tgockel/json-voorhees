/** \file
 *
 *  Copyright (c) 2012-2016 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <algorithm>
#include <cassert>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <jsonv/value.hpp>
#include <jsonv/parse.hpp>

#if defined(_MSC_VER) && !defined(NDEBUG)
#   include <crtdbg.h>
#   include <cstdio>
#   include <cstdlib>

namespace
{

/// What a CRT diagnostic exits the run with. 3 is the status `abort` yields on Windows.
constexpr int crt_report_exit_status = 3;

/// Fail the run on a CRT diagnostic rather than let it narrate one.
///
/// Sending reports to a file instead of a message box is what stops an MSVC Debug build blocking on a dialog no CI
/// runner will ever dismiss, but on its own it inverts the problem. `_CrtDbgReport` returning 0 is the "carry on"
/// answer, and the runtime's own assertions -- `_ASSERTE` inside the UCRT, which is how a bad argument to a
/// `<cctype>` classifier surfaces -- are report-only. So the process printed the assertion, kept going, and the test
/// which tripped it reported success. `_set_abort_behavior` cannot cover that: nothing on this path calls `abort`.
///
/// This terminates rather than returning the value which asks for a debug break -- there is no debugger attached on
/// a runner, and `_exit` gives a deterministic nonzero status instead of a breakpoint exception. `_CRT_WARN` is
/// passed through, since a warning is not a failure.
int fail_on_crt_report(int report_type, const char* message)
{
    if (report_type == _CRT_WARN)
        return 0;

    std::fflush(nullptr);
    std::fprintf(stderr,
                 "\nCRT %s: %s\n",
                 report_type == _CRT_ASSERT ? "assertion" : "error",
                 (message && *message) ? message : "(no message)"
                );
    std::fflush(stderr);

    _exit(crt_report_exit_status);
}

int crt_report_hook(int report_type, char* message, int*)
{
    return fail_on_crt_report(report_type, message);
}

/// The wide hook is not redundant: the UCRT reports its own assertions through `_CrtDbgReportW`, so the narrow chain
/// never sees them.
int crt_report_hook_wide(int report_type, wchar_t* message, int*)
{
    if (report_type == _CRT_WARN)
        return 0;

    char        narrow[1024] = { 0 };
    std::size_t converted    = 0U;
    if (message != nullptr)
        (void) wcstombs_s(&converted, narrow, sizeof(narrow), message, _TRUNCATE);

    return fail_on_crt_report(report_type, narrow);
}

}
#endif

#include "filesystem_util.hpp"
#include "test.hpp"

TEST(demo)
{
    std::string src = "{ \"blazing\": [ 3, \"\\\"\\n\", 4.5, 5.123, 4.10921e19 ], "
                      "  \"text\": [ 1, 2, 3, 4, \t\"something\"],  "
                      "  \"we call him \\\"empty array\\\"\": [], "
                      "  \"we call him \\\"empty object\\\"\": {}, "
                      "  \"unicode\" :\"\\uface\""
                      "}";
    jsonv::value parsed = jsonv::parse(src);
    std::cout << parsed;
}

int main(int argc, char** argv)
{
    // An MSVC Debug build reports a failed `assert`, a failed checked-iterator precondition or an `abort()` through a
    // modal message box. There is nobody on a CI runner to dismiss one, so the process sits on it until the job hits
    // its time limit -- which reads as a hang with no output rather than as the test failure it actually is. Send the
    // reports to stderr and keep the abort message without handing the process to Windows Error Reporting.
    #if defined(_MSC_VER) && !defined(NDEBUG)
    for (int report_type : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
    {
        _CrtSetReportMode(report_type, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(report_type, _CRTDBG_FILE_STDERR);
    }
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, crt_report_hook);
    _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, crt_report_hook_wide);
    _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    #endif

    std::string filter;
    if (argc == 2)
        filter = argv[1];

    std::deque<const jsonv_test::unit_test*> failed_tests;
    for (auto test : jsonv_test::get_unit_tests())
    {
        bool shouldrun = filter.empty()
                      || test->name().find(filter) != std::string::npos;
        if (shouldrun && !test->run())
            failed_tests.push_back(test);
    }

    if (failed_tests.empty())
    {
        return 0;
    }
    else
    {
        std::ostringstream os;
        os << "Unit test failure:" << std::endl;
        for (auto test : failed_tests)
            os << " - " << test->name() << std::endl;

        // Printed everywhere, MSVC included: a Windows failure is the one hardest to reproduce locally, so the list
        // of tests which failed is worth more there than anywhere else.
        std::cerr << os.str();
        return 1;
    }

}
