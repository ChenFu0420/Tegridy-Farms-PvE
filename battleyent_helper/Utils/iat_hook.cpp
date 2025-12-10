#include "iat_hook.h"
#include <cstdio>
#include <random>
#include <atomic>

// =================================================================
// IAT Hook Implementation - BattlEye Service Bypass
// =================================================================

// Original function pointers
static decltype(&OpenServiceA) g_pOriginalOpenServiceA = OpenServiceA;
static decltype(&QueryServiceStatusEx) g_pOriginalQueryServiceStatusEx = QueryServiceStatusEx;

// Fake service handle (randomly generated)
static SC_HANDLE g_FakeBEServiceHandle = nullptr;

// Global IAT hook interception counter (thread-safe)
std::atomic<unsigned long> g_IATHookInterceptionCount(0);

/**
 * @brief Generates a realistic fake service handle by analyzing real service handles
 * @return A random SC_HANDLE value that mimics real service handles
 *
 * Advanced strategy:
 * 1. Open a real harmless service (EventLog) to get a reference handle
 * 2. Extract the handle's address range
 * 3. Generate a fake handle in the same low-memory range
 * 4. This makes our fake handle indistinguishable from real ones
 */
SC_HANDLE GenerateRandomServiceHandle()
{
    SC_HANDLE referenceHandle = nullptr;
    SC_HANDLE scManager = nullptr;
    uintptr_t baseAddress = 0;

    // Try to open SCM and get a reference handle
    scManager = g_pOriginalOpenServiceA(
        OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT),
        nullptr,
        0
    );

    if (scManager)
    {
        // Open a harmless system service to get a reference handle
        // EventLog is always running and safe to query
        referenceHandle = g_pOriginalOpenServiceA(scManager, "EventLog", SERVICE_QUERY_STATUS);

        if (referenceHandle)
        {
            // Extract the base address from real handle
            baseAddress = reinterpret_cast<uintptr_t>(referenceHandle);

            // Close the reference handle immediately
            CloseServiceHandle(referenceHandle);
        }

        CloseServiceHandle(scManager);
    }

    // If we successfully got a reference, generate in same range
    if (baseAddress != 0)
    {
        // Generate offset in range ±0x10000 from reference handle
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> offsetDis(-0x10000, 0x10000);

        int offset = offsetDis(gen);
        // Align to 8-byte boundary (typical for handles on x64)
        offset = (offset / 8) * 8;

        uintptr_t fakeHandle = baseAddress + offset;

        // Ensure it's not null and not the exact same value
        if (fakeHandle != 0 && fakeHandle != baseAddress)
        {
            printf("[IAT Hook] Generated realistic fake handle: 0x%p (based on real handle: 0x%p)\n",
                reinterpret_cast<void*>(fakeHandle),
                reinterpret_cast<void*>(baseAddress));
            return reinterpret_cast<SC_HANDLE>(fakeHandle);
        }
    }

    // Fallback: Use magic value if we couldn't get a reference
    printf("[IAT Hook] Could not sample real handle, using fallback method\n");

    std::random_device rd;
    std::mt19937 gen(rd());

    // Fallback to low-range realistic values
    // Real service handles typically fall in 0x00000000 - 0x0FFFFFFF range
    std::uniform_int_distribution<uintptr_t> dis(0x00100000, 0x0FFFFFFF);
    uintptr_t fallbackValue = dis(gen);

    // Align to 8-byte boundary
    fallbackValue = (fallbackValue / 8) * 8;

    return reinterpret_cast<SC_HANDLE>(fallbackValue);
}


/**
 * @brief Hooked OpenServiceA - Intercepts BEService access
 */
SC_HANDLE WINAPI HookedOpenServiceA(SC_HANDLE hSCManager, LPCSTR lpServiceName, DWORD dwDesiredAccess)
{
    if (lpServiceName && strcmp(lpServiceName, "BEService") == 0)
    {
        // Increment interception counter (thread-safe)
        g_IATHookInterceptionCount++;

        // Return fake handle for BEService (silent operation)
        return g_FakeBEServiceHandle;
    }

    // Forward other service requests to original function
    return g_pOriginalOpenServiceA(hSCManager, lpServiceName, dwDesiredAccess);
}

/**
 * @brief Hooked QueryServiceStatusEx - Fakes BEService running status
 */
BOOL WINAPI HookedQueryServiceStatusEx(
    SC_HANDLE hService,
    SC_STATUS_TYPE InfoLevel,
    LPBYTE lpBuffer,
    DWORD cbBufSize,
    LPDWORD pcbBytesNeeded)
{
    // Check if this is querying our fake BEService handle
    if (hService == g_FakeBEServiceHandle && InfoLevel == SC_STATUS_PROCESS_INFO && cbBufSize >= sizeof(SERVICE_STATUS_PROCESS))
    {
        // Increment interception counter (thread-safe)
        g_IATHookInterceptionCount++;

        // Forge SERVICE_STATUS_PROCESS structure
        SERVICE_STATUS_PROCESS* status = reinterpret_cast<SERVICE_STATUS_PROCESS*>(lpBuffer);
        memset(status, 0, sizeof(SERVICE_STATUS_PROCESS));

        status->dwCurrentState = SERVICE_RUNNING;  // Critical: Report service as running
        status->dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        status->dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
        status->dwWin32ExitCode = NO_ERROR;
        status->dwServiceSpecificExitCode = 0;
        status->dwCheckPoint = 0;
        status->dwWaitHint = 0;
        status->dwProcessId = GetCurrentProcessId();  // Use current process ID
        status->dwServiceFlags = 0;

        if (pcbBytesNeeded)
            *pcbBytesNeeded = sizeof(SERVICE_STATUS_PROCESS);

        // Silent operation - return success
        return TRUE;
    }

    // Forward other queries to original function
    return g_pOriginalQueryServiceStatusEx(hService, InfoLevel, lpBuffer, cbBufSize, pcbBytesNeeded);
}

/**
 * @brief 64-bit inline hook engine using JMP RAX
 * @param target Target function address to hook
 * @param hook Hook function address
 */
void HookFunction64(void* target, void* hook)
{
    DWORD oldProtect;

    // Change memory protection to allow writing
    if (!VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        printf("[IAT Hook] VirtualProtect failed for address %p\n", target);
        return;
    }

    // Write 64-bit absolute jump:
    // 48 B8 [8 bytes address]  ; mov rax, hook_address
    // FF E0                     ; jmp rax
    uint8_t* p = static_cast<uint8_t*>(target);
    p[0] = 0x48;  // REX.W prefix
    p[1] = 0xB8;  // MOV RAX, imm64
    *reinterpret_cast<void**>(p + 2) = hook;  // Write hook address
    p[10] = 0xFF; // JMP RAX (opcode part 1)
    p[11] = 0xE0; // JMP RAX (opcode part 2)

    // Restore original memory protection
    VirtualProtect(target, 12, oldProtect, &oldProtect);

    // Flush instruction cache to ensure CPU sees the new code
    FlushInstructionCache(GetCurrentProcess(), target, 12);

    printf("[IAT Hook] Installed hook at %p -> %p (12 bytes)\n", target, hook);
}

/**
 * @brief Installs all BattlEye service bypass hooks
 */
void InstallBEServiceBypass()
{
    printf("[+] Installing IAT Hook for BattlEye service bypass...\n");

    // Generate random fake service handle
    g_FakeBEServiceHandle = GenerateRandomServiceHandle();
    printf("[+] Generated random fake BEService handle: 0x%p\n", g_FakeBEServiceHandle);

    // Save original function pointers
    g_pOriginalOpenServiceA = OpenServiceA;
    g_pOriginalQueryServiceStatusEx = QueryServiceStatusEx;

    // Install hooks using 64-bit JMP technique
    HookFunction64(reinterpret_cast<void*>(OpenServiceA), reinterpret_cast<void*>(HookedOpenServiceA));
    HookFunction64(reinterpret_cast<void*>(QueryServiceStatusEx), reinterpret_cast<void*>(HookedQueryServiceStatusEx));

    printf("[+] BattlEye service check bypass installed successfully!\n");
}

/**
 * @brief Get the current IAT hook interception count
 * @return Number of times BEService calls have been intercepted
 */
unsigned long GetIATHookInterceptionCount()
{
    return g_IATHookInterceptionCount.load();
}