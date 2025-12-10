#pragma once
#include <Windows.h>
#include <atomic>

// =================================================================
// IAT Hook Module - BattlEye Service Bypass
// =================================================================

// Global IAT hook interception counter
extern std::atomic<unsigned long> g_IATHookInterceptionCount;

/**
 * @brief Installs IAT hooks for BattlEye service detection bypass
 *
 * This function replaces direct memory patching with more stealthy
 * IAT (Import Address Table) hooking, intercepting OpenServiceA and
 * QueryServiceStatusEx to fake BEService existence.
 */
void InstallBEServiceBypass();

/**
 * @brief Get the current IAT hook interception count
 * @return Number of times BEService calls have been intercepted
 */
unsigned long GetIATHookInterceptionCount();
