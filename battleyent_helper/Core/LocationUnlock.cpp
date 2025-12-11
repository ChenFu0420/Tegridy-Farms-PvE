#include "LocationUnlock.h"
#include "../Core/IL2CPP_API.h"
#include <cstdio>
#include <string>

// External globals
extern void* g_TarkovApplicationInstance;

// Helper to safely check if memory is readable
static inline bool IsSafeToRead(void* ptr, size_t size)
{
    if (!ptr) return false;

    __try
    {
        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)))
        {
            if (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))
            {
                // Additional bounds check
                volatile char test = *(char*)ptr;
                return true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return false;
}

// Helper to safely write memory
static inline bool SafeMemWrite(void* address, const void* data, size_t size)
{
    if (!address || !data) return false;

    __try
    {
        DWORD oldProtect;
        if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        memcpy(address, data, size);
        VirtualProtect(address, size, oldProtect, &oldProtect);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Helper namespace for patch operations
namespace PatchHelpers
{
    void PatchBoolGetter(const char* name, void* fn, bool value)
    {
        if (!fn || !IsSafeToRead(fn, 16)) return;

        DWORD oldProtect;
        if (!VirtualProtect(fn, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
            return;

        uint8_t* code = (uint8_t*)fn;
        code[0] = 0xB0;  // mov al, imm8
        code[1] = value ? 1 : 0;
        code[2] = 0xC3;  // ret

        VirtualProtect(fn, 16, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), fn, 16);
    }

    void PatchIntGetter(const char* name, void* fn, int value)
    {
        if (!fn || !IsSafeToRead(fn, 16)) return;

        DWORD oldProtect;
        if (!VirtualProtect(fn, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
            return;

        uint8_t* code = (uint8_t*)fn;
        code[0] = 0xB8;  // mov eax, imm32
        memcpy(&code[1], &value, sizeof(value));
        code[5] = 0xC3;  // ret

        VirtualProtect(fn, 16, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), fn, 16);
    }

    void PatchBytes(const char* name, void* fn, const uint8_t* bytes, size_t size)
    {
        if (!fn || !IsSafeToRead(fn, size)) return;

        DWORD oldProtect;
        if (!VirtualProtect(fn, size, PAGE_EXECUTE_READWRITE, &oldProtect))
            return;

        memcpy(fn, bytes, size);

        VirtualProtect(fn, size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), fn, size);
    }
}

namespace LocationUnlock
{
    void ForceUnlockLocationFields(void* loc)
    {
        if (!loc || !IsSafeToRead(loc, 0x200))
        {
            return;
        }

        __try
        {
            // Read current values first
            printf("[*] Current location field values:\n");
            bool currentEnabled = *(bool*)((uintptr_t)loc + 0x30);
            bool currentLocked = *(bool*)((uintptr_t)loc + 0x34);
            bool currentLockedByQuest = *(bool*)((uintptr_t)loc + 0x35);
            int32_t currentMinLevel = *(int32_t*)((uintptr_t)loc + 0x54);
            int32_t currentMaxLevel = *(int32_t*)((uintptr_t)loc + 0x58);
            bool currentDisabledForScav = *(bool*)((uintptr_t)loc + 0x171);
            void* currentAccessKeys = *(void**)((uintptr_t)loc + 0x1c8);
            int32_t currentMinPlayerLvl = *(int32_t*)((uintptr_t)loc + 0x1d0);

            printf("  Enabled: %d\n", currentEnabled);
            printf("  Locked: %d\n", currentLocked);
            printf("  LockedByQuest: %d\n", currentLockedByQuest);
            printf("  RequiredPlayerLevelMin: %d\n", currentMinLevel);
            printf("  RequiredPlayerLevelMax: %d\n", currentMaxLevel);
            printf("  DisabledForScav: %d\n", currentDisabledForScav);
            printf("  AccessKeys: 0x%p\n", currentAccessKeys);
            printf("  MinPlayerLvlAccessKeys: %d\n", currentMinPlayerLvl);

            // Offsets from NoNamespace.cs Location class - write safely
            bool trueVal = true;
            bool falseVal = false;
            int32_t minLevel = 1;
            int32_t maxLevel = 100;
            int32_t zeroVal = 0;
            void* nullPtr = nullptr;

            printf("[*] Patching location fields...\n");
            SafeMemWrite((void*)((uintptr_t)loc + 0x30), &trueVal, sizeof(bool));      // Enabled
            SafeMemWrite((void*)((uintptr_t)loc + 0x34), &falseVal, sizeof(bool));     // Locked
            SafeMemWrite((void*)((uintptr_t)loc + 0x35), &falseVal, sizeof(bool));     // LockedByQuest
            SafeMemWrite((void*)((uintptr_t)loc + 0x54), &minLevel, sizeof(int32_t));  // RequiredPlayerLevelMin
            SafeMemWrite((void*)((uintptr_t)loc + 0x58), &maxLevel, sizeof(int32_t));  // RequiredPlayerLevelMax
            SafeMemWrite((void*)((uintptr_t)loc + 0x171), &falseVal, sizeof(bool));    // DisabledForScav
            SafeMemWrite((void*)((uintptr_t)loc + 0x1c8), &nullPtr, sizeof(void*));    // AccessKeys
            SafeMemWrite((void*)((uintptr_t)loc + 0x1d0), &zeroVal, sizeof(int32_t));  // MinPlayerLvlAccessKeys

            printf("[+] Location fields patched successfully\n");
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            printf("[-] Exception in ForceUnlockLocationFields\n");
        }
    }

    void UpdateRaidSelectedLocation(Il2CppImage* image)
    {
        __try
        {
            if (!g_TarkovApplicationInstance || !IsSafeToRead(g_TarkovApplicationInstance, 0x200))
                return;

            // Read RaidSettings pointer from TarkovApplication + 0xD0
            uintptr_t raidSettings = 0;
            if (!SafeMemWrite(&raidSettings, (void*)((uintptr_t)g_TarkovApplicationInstance + OFFSET_RAID_SETTINGS), sizeof(uintptr_t)))
                return;

            if (!raidSettings || !IsSafeToRead((void*)raidSettings, 0x200))
                return;

            // Read SelectedLocation pointer from RaidSettings + 0xA8
            uintptr_t selectedLocation = 0;
            memcpy(&selectedLocation, (void*)(raidSettings + OFFSET_SELECTED_LOCATION), sizeof(uintptr_t));

            // Check if location changed
            if (selectedLocation && IsSafeToRead((void*)selectedLocation, 0x200) && g_selectedLocation != selectedLocation)
            {
                printf("[+] SelectedLocation = 0x%llX\n", selectedLocation);

                // Force unlock selected location fields
                ForceUnlockLocationFields((void*)selectedLocation);

                printf("[+] Updated fields for selected location\n");

                // Update tracked location
                g_selectedLocation = selectedLocation;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            printf("[-] Exception in UpdateRaidSelectedLocation\n");
        }
    }

    void ApplyUnlockLocations(Il2CppImage* image)
    {
        __try
        {
            printf("[*] Applying permanent location unlock patches...\n");
            printf("[*] NO EXCLUSIONS - All maps will be unlocked\n");

            if (!image)
            {
                printf("[-] Image is null, cannot apply patches\n");
                return;
            }

            // Patch MatchMakerSelectionLocationScreen visibility checks
            Il2CppClass* mmScreen = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.UI.Matchmaker", "MatchMakerSelectionLocationScreen");
            if (!mmScreen)
            {
                printf("[-] MatchMakerSelectionLocationScreen not found\n");
                return;
            }

            auto patchBoolRet = [&](const char* name, bool value)
                {
                    const Il2CppMethod* m = FindMethodRecursive(mmScreen, name);
                    if (!m)
                    {
                        void* iter2 = nullptr;
                        while (const Il2CppMethod* mm = il2cpp_class_get_methods(mmScreen, &iter2))
                        {
                            const char* n = il2cpp_method_get_name(mm);
                            if (n && std::string(n).find(name) != std::string::npos) { m = mm; break; }
                        }
                    }
                    if (!m) { printf("[-] %s not found on MatchMakerSelectionLocationScreen\n", name); return; }
                    void* fn = *(void**)m;
                    if (!fn) { printf("[-] %s ptr null\n", name); return; }
                    PatchHelpers::PatchBoolGetter("UnlockLocations", fn, value);
                    printf("[+] %s patched => %s\n", name, value ? "true" : "false");
                };

            // Always display and allow selection
            patchBoolRet("DisplayLocation", true);
            patchBoolRet("AvailableByLevel", true);
            patchBoolRet("AvailableBySide", true);
            patchBoolRet("CanDisplayAcceptButton", true);
            patchBoolRet("GetCoopBlockReason", false);

            // Stop locking individual buttons
            Il2CppClass* locButton = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.UI", "LocationButton");
            if (locButton)
            {
                auto patchRet = [&](const char* name)
                    {
                        const Il2CppMethod* m = FindMethodRecursive(locButton, name);
                        if (!m) return;
                        void* fn = *(void**)m;
                        if (!fn) return;
                        const uint8_t retOnly[1] = { 0xC3 };
                        PatchHelpers::PatchBytes("UnlockLocations", fn, retOnly, sizeof(retOnly));
                        printf("[+] %s patched => ret\n", name);
                    };
                patchRet("HandleOnlineModeToggle");
                patchRet("SetSpecial");
                patchRet("SetDefault");
                patchRet("ShowUnavailableOverlay");
                patchRet("SetLocked");
                patchRet("set_interactable");

                // Force empty unavailable text
                const Il2CppMethod* mTxt = FindMethodRecursive(locButton, "GetUnavailableText");
                if (mTxt)
                {
                    void* fn = *(void**)mTxt;
                    if (fn)
                    {
                        const uint8_t code[2] = { 0x33, 0xC0 }; // xor eax,eax
                        PatchHelpers::PatchBytes("UnlockLocations", fn, code, sizeof(code));
                        printf("[+] GetUnavailableText patched => null\n");
                    }
                }
            }

            // BaseGroupPanel: ensure in-raid button not blocked
            Il2CppClass* baseGroupPanel = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.UI.Matchmaker", "BaseGroupPanel");
            if (baseGroupPanel)
            {
                auto patchBool = [&](const char* name, bool val)
                    {
                        const Il2CppMethod* m = FindMethodRecursive(baseGroupPanel, name);
                        if (!m) return;
                        void* fn = *(void**)m;
                        if (!fn) return;
                        PatchHelpers::PatchBoolGetter("UnlockLocations", fn, val);
                        printf("[+] %s patched => %s\n", name, val ? "true" : "false");
                    };

                auto patchRet = [&](const char* name)
                    {
                        const Il2CppMethod* m = FindMethodRecursive(baseGroupPanel, name);
                        if (!m) return;
                        void* fn = *(void**)m;
                        if (!fn) return;
                        const uint8_t retOnly[1] = { 0xC3 };
                        PatchHelpers::PatchBytes("UnlockLocations", fn, retOnly, sizeof(retOnly));
                        printf("[+] %s patched => ret\n", name);
                    };

                patchBool("get_CanShowInRaidButton", true);
                patchRet("SetGroupsAvailability");
                patchRet("IsLocationAvailable");
                patchRet("UpdateRaidReadyButton");
            }

            // Patch Location class getters directly
            auto patchLocationClass = [&](const char* nsName, const char* className)
                {
                    Il2CppClass* locCls = (Il2CppClass*)il2cpp_class_from_name(image, nsName, className);
                    if (!locCls) return false;

                    auto patchBool = [&](const char* name, bool val)
                        {
                            const Il2CppMethod* m = FindMethodRecursive(locCls, name);
                            if (!m)
                            {
                                void* it = nullptr;
                                while (const Il2CppMethod* mm = il2cpp_class_get_methods(locCls, &it))
                                {
                                    const char* n = il2cpp_method_get_name(mm);
                                    if (n && std::string(n).find(name) != std::string::npos) { m = mm; break; }
                                }
                            }
                            if (!m) return;
                            void* fn = *(void**)m;
                            if (!fn) return;
                            PatchHelpers::PatchBoolGetter("UnlockLocations", fn, val);
                            printf("[+] %s.%s patched %s => %s\n", nsName, className, il2cpp_method_get_name(m), val ? "true" : "false");
                        };

                    auto patchInt = [&](const char* name, int val)
                        {
                            const Il2CppMethod* m = FindMethodRecursive(locCls, name);
                            if (!m)
                            {
                                void* it = nullptr;
                                while (const Il2CppMethod* mm = il2cpp_class_get_methods(locCls, &it))
                                {
                                    const char* n = il2cpp_method_get_name(mm);
                                    if (n && std::string(n).find(name) != std::string::npos) { m = mm; break; }
                                }
                            }
                            if (!m) return;
                            void* fn = *(void**)m;
                            if (!fn) return;
                            PatchHelpers::PatchIntGetter("UnlockLocations", fn, val);
                            printf("[+] %s.%s patched %s => %d\n", nsName, className, il2cpp_method_get_name(m), val);
                        };

                    auto patchNullRet = [&](const char* name)
                        {
                            const Il2CppMethod* m = FindMethodRecursive(locCls, name);
                            if (!m)
                            {
                                void* it = nullptr;
                                while (const Il2CppMethod* mm = il2cpp_class_get_methods(locCls, &it))
                                {
                                    const char* n = il2cpp_method_get_name(mm);
                                    if (n && std::string(n).find(name) != std::string::npos) { m = mm; break; }
                                }
                            }
                            if (!m) return;
                            void* fn = *(void**)m;
                            if (!fn) return;
                            const uint8_t code[2] = { 0x33, 0xC0 }; // xor eax,eax
                            PatchHelpers::PatchBytes("UnlockLocations", fn, code, sizeof(code));
                            printf("[+] %s.%s patched %s => null\n", nsName, className, il2cpp_method_get_name(m));
                        };

                    patchBool("get_Enabled", true);
                    patchBool("get_Locked", false);
                    patchBool("get_LockedByQuest", false);
                    patchBool("get_DisabledForScav", false);
                    patchBool("get_ForceOnlineRaidInPVE", false);
                    patchBool("get_ForceOfflineRaidInPVE", true);
                    patchInt("get_RequiredPlayerLevelMin", 1);
                    patchInt("get_RequiredPlayerLevelMax", 100);
                    patchInt("get_MinPlayerLvlAccessKeys", 0);
                    patchNullRet("get_AccessKeys");
                    return true;
                };

            if (!patchLocationClass("JsonType", "LocationSettings/Location"))
            {
                patchLocationClass("", "Location"); // fallback NoNamespace
            }

            printf("[+] Location unlock patches applied (PERMANENT)\n");
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            printf("[-] EXCEPTION in ApplyUnlockLocations - game may be unstable\n");
        }
    }

    void UpdateLocationRoutes(Il2CppImage* image)
    {
        // DISABLED - This was causing crashes
        // The one-time patches should be sufficient
        return;
    }
}