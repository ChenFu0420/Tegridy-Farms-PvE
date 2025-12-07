#include "Advapi32Patch.h"
#include <psapi.h>

namespace Advapi32Patch
{
    fn_OpenServiceA g_OriginalOpenServiceA = nullptr;
    fn_QueryServiceStatusEx g_OriginalQueryServiceStatusEx = nullptr;

    // Thread freezing implementation
    bool FreezeAllThreads(std::vector<HANDLE>& frozenThreads)
    {
        DWORD currentProcessId = GetCurrentProcessId();
        DWORD currentThreadId = GetCurrentThreadId();

        HANDLE threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (threadSnapshot == INVALID_HANDLE_VALUE)
        {
            printf("[FATAL] Failed to create thread snapshot (Error: %d)\n", GetLastError());
            return false;
        }

        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);

        if (Thread32First(threadSnapshot, &te32))
        {
            do
            {
                if (te32.th32OwnerProcessID == currentProcessId &&
                    te32.th32ThreadID != currentThreadId)
                {
                    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                        FALSE, te32.th32ThreadID);
                    if (hThread)
                    {
                        if (SuspendThread(hThread) != (DWORD)-1)
                        {
                            frozenThreads.push_back(hThread);
                        }
                        else
                        {
                            CloseHandle(hThread);
                        }
                    }
                }
            } while (Thread32Next(threadSnapshot, &te32));
        }

        CloseHandle(threadSnapshot);
        return true;
    }

    void ResumeAllThreads(std::vector<HANDLE>& frozenThreads)
    {
        for (HANDLE hThread : frozenThreads)
        {
            ResumeThread(hThread);
            CloseHandle(hThread);
        }
        frozenThreads.clear();
    }

    // Memory writing with protection
    bool WriteProtectedMemory(void* address, const unsigned char* data, size_t size)
    {
        DWORD oldProtect;
        if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            printf("[-] VirtualProtect failed (Error: %d)\n", GetLastError());
            return false;
        }

        memcpy(address, data, size);

        DWORD temp;
        VirtualProtect(address, size, oldProtect, &temp);

        FlushInstructionCache(GetCurrentProcess(), address, size);

        return true;
    }

    // Apply Advapi32 patches
    bool ApplyAdvapi32Patch()
    {
        HMODULE hAdvapi = GetModuleHandleA("Advapi32.dll");
        if (!hAdvapi)
        {
            printf("[FATAL] Advapi32.dll not loaded\n");
            return false;
        }

        // Get function addresses
        g_OriginalOpenServiceA = (fn_OpenServiceA)GetProcAddress(hAdvapi, "OpenServiceA");
        g_OriginalQueryServiceStatusEx = (fn_QueryServiceStatusEx)GetProcAddress(hAdvapi, "QueryServiceStatusEx");

        if (!g_OriginalOpenServiceA || !g_OriginalQueryServiceStatusEx)
        {
            printf("[FATAL] Failed to get Advapi32 function addresses\n");
            return false;
        }

        // Patch OpenServiceA to always return success (mov al, 1; ret)
        const unsigned char patchOpenServiceA[] = { 0xB0, 0x01, 0xC3 };

        if (!WriteProtectedMemory((void*)g_OriginalOpenServiceA, patchOpenServiceA, sizeof(patchOpenServiceA)))
        {
            printf("[FATAL] Failed to patch OpenServiceA\n");
            return false;
        }

        // Patch QueryServiceStatusEx to return SERVICE_RUNNING
        const unsigned char patchQueryServiceStatusEx[] = {
            0x41, 0xC7, 0x40, 0x04, 0x04, 0x00, 0x00, 0x00, // mov dword ptr [r8+4], 4 (SERVICE_RUNNING)
            0xB0, 0x01,                                     // mov al, 1
            0xC3                                            // ret
        };

        if (!WriteProtectedMemory((void*)g_OriginalQueryServiceStatusEx,
            patchQueryServiceStatusEx,
            sizeof(patchQueryServiceStatusEx)))
        {
            printf("[FATAL] Failed to patch QueryServiceStatusEx\n");
            return false;
        }

        return true;
    }

    // Verify patches are correctly applied
    bool VerifyPatchesApplied()
    {
        if (!g_OriginalOpenServiceA || !g_OriginalQueryServiceStatusEx)
            return false;

        // Verify OpenServiceA patch
        uint8_t openServiceBuffer[3];
        memcpy(openServiceBuffer, (void*)g_OriginalOpenServiceA, 3);

        if (!(openServiceBuffer[0] == 0xB0 && openServiceBuffer[1] == 0x01 && openServiceBuffer[2] == 0xC3))
        {
            printf("[-] OpenServiceA patch verification failed\n");
            return false;
        }

        // Verify QueryServiceStatusEx patch
        uint8_t queryServiceBuffer[11];
        memcpy(queryServiceBuffer, (void*)g_OriginalQueryServiceStatusEx, 11);

        if (!(queryServiceBuffer[0] == 0x41 && queryServiceBuffer[1] == 0xC7 &&
            queryServiceBuffer[2] == 0x40 && queryServiceBuffer[3] == 0x04 &&
            queryServiceBuffer[4] == 0x04))
        {
            printf("[-] QueryServiceStatusEx patch verification failed\n");
            return false;
        }

        return true;
    }

    // Check if BEClient_x64.dll is loaded
    bool IsBEClientLoaded()
    {
        HMODULE hModules[1024];
        DWORD cbNeeded;

        if (EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &cbNeeded))
        {
            for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
            {
                char szModuleName[MAX_PATH];
                if (GetModuleFileNameExA(GetCurrentProcess(), hModules[i], szModuleName, sizeof(szModuleName)))
                {
                    if (strstr(szModuleName, "BEClient_x64.dll"))
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Main thread-freezing patch function
    bool ApplyPatchWithThreadFreezing(int maxRetries)
    {
        printf("[SECURITY] Starting thread-freezing BEService bypass...\n");

        // Safety check: Ensure BEClient is not loaded
        if (IsBEClientLoaded())
        {
            printf("[FATAL] Cannot proceed - BattlEye is already active\n");
            return false;
        }

        std::vector<HANDLE> frozenThreads;

        for (int attempt = 1; attempt <= maxRetries; attempt++)
        {
            printf("[*] Attempt %d/%d to apply bypass...\n", attempt, maxRetries);

            // Step 1: Freeze all threads
            if (!FreezeAllThreads(frozenThreads))
            {
                ResumeAllThreads(frozenThreads);
                continue;
            }

            // Step 2: Apply patches while threads are frozen
            bool patchSuccess = ApplyAdvapi32Patch();

            // Step 3: Resume threads
            ResumeAllThreads(frozenThreads);

            // Step 4: Verify patches
            if (patchSuccess && VerifyPatchesApplied())
            {
                printf("[SUCCESS] BEService bypass applied successfully on attempt %d\n", attempt);
                return true;
            }

            printf("[-] Attempt %d failed, retrying...\n", attempt);

            // Small delay before retry
            if (attempt < maxRetries)
                Sleep(100);
        }

        printf("[FATAL] Failed to apply BEService bypass after %d attempts\n", maxRetries);
        return false;
    }

    // Perform all pre-injection checks
    bool PerformPreInjectionChecks()
    {
        printf("[SECURITY] Performing pre-injection checks...\n");

        // 1. Check if BEClient is already loaded
        if (IsBEClientLoaded())
        {
            printf("[FATAL] BattlEye is already active - ABORTING\n");
            return false;
        }

        // 2. Apply BEService bypass with thread freezing
        printf("[SECURITY] Applying BEService bypass...\n");

        if (!ApplyPatchWithThreadFreezing(3)) // 3 retry attempts
        {
            printf("[FATAL] BEService bypass failed - ABORTING\n");
            return false;
        }

        return true;
    }
}