#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <cstdio>

namespace Advapi32Patch
{
    // Function prototypes
    typedef SC_HANDLE(WINAPI* fn_OpenServiceA)(SC_HANDLE, LPCSTR, DWORD);
    typedef BOOL(WINAPI* fn_QueryServiceStatusEx)(SC_HANDLE, SC_STATUS_TYPE, LPBYTE, DWORD, LPDWORD);

    // Original function pointers
    extern fn_OpenServiceA g_OriginalOpenServiceA;
    extern fn_QueryServiceStatusEx g_OriginalQueryServiceStatusEx;

    // Main patch function with thread freezing
    bool ApplyPatchWithThreadFreezing(int maxRetries = 3);

    // Patch verification
    bool VerifyPatchesApplied();

    // Safety checks
    bool IsBEClientLoaded();

    // Early injection checks
    bool PerformPreInjectionChecks();
}