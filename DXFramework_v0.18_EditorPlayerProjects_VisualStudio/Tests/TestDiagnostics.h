#pragma once
// Failed assertions in automated tests must report to stderr, never open a
// modal Windows CRT dialog that would block unattended verification.
#ifdef _WIN32
#include <cstdlib>
#include <crtdbg.h>
namespace DXTest
{
    inline const bool DiagnosticsReady=[]
    {
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
        return true;
    }();
}
#endif
