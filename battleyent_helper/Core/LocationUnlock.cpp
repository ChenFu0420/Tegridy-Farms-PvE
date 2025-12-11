#include "LocationUnlock.h"
#include "../Core/IL2CPP_API.h"
#include <cstdio>
#include <string>

// External globals
extern void* g_TarkovApplicationInstance;


namespace PatchHelpers
{

    // these are specific for locations, maybe redundant, will fix later ughhhhhh
    void PatchBoolGetter(const char* name, void* fn, bool value)
    {
        if (!fn) return;

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
        if (!fn) return;

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
        if (!fn) return;

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
    // Use the reference implementation's approach
    static inline bool IsBadReadPtr(void* ptr, size_t size)
    {
        if (!ptr) return true;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
            return true;

        if (mbi.State != MEM_COMMIT)
            return true;

        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
            return true;

        return false;
    }

    void ForceUnlockLocationFields(void* loc)
    {
        if (!loc || IsBadReadPtr(loc, 0x200))
        {
            printf("[-] Invalid location pointer or not readable\n");
            return;
        }

        // Offsets from NoNamespace.cs Location class - direct memory write
        *(bool*)((uintptr_t)loc + 0x30) = true;      // Enabled
        *(bool*)((uintptr_t)loc + 0x34) = false;     // Locked
        *(bool*)((uintptr_t)loc + 0x35) = false;     // LockedByQuest
        *(int32_t*)((uintptr_t)loc + 0x54) = 1;      // RequiredPlayerLevelMin
        *(int32_t*)((uintptr_t)loc + 0x58) = 100;    // RequiredPlayerLevelMax
        *(bool*)((uintptr_t)loc + 0x171) = false;    // DisabledForScav
        *(void**)((uintptr_t)loc + 0x1c8) = nullptr; // AccessKeys
        *(int32_t*)((uintptr_t)loc + 0x1d0) = 0;     // MinPlayerLvlAccessKeys

    }

    void UpdateRaidSelectedLocation(Il2CppImage* image)
    {
        if (!g_TarkovApplicationInstance)
            return;

        // Read RaidSettings pointer from TarkovApplication + 0xD0
        uintptr_t raidSettings = *(uintptr_t*)((uintptr_t)g_TarkovApplicationInstance + OFFSET_RAID_SETTINGS);
        if (!raidSettings || IsBadReadPtr((void*)raidSettings, 0x200))
            return;

        // Read SelectedLocation pointer from RaidSettings + 0xA8
        uintptr_t selectedLocation = *(uintptr_t*)(raidSettings + OFFSET_SELECTED_LOCATION);

        // Check if location changed
        if (selectedLocation && !IsBadReadPtr((void*)selectedLocation, 0x200) && g_selectedLocation != selectedLocation)
        {

            // IMPORTANT: Set offline raid flags CORRECTLY
            // Both false = normal behavior (let server decide)
            // ForceOfflineRaid = true = force offline
            *(bool*)(selectedLocation + OFFSET_FORCE_ONLINE_RAID) = false;
            *(bool*)(selectedLocation + OFFSET_FORCE_OFFLINE_RAID) = true;

            // Force unlock selected location fields
            ForceUnlockLocationFields((void*)selectedLocation);



            // Update tracked location
            g_selectedLocation = selectedLocation;
        }
    }

    void ApplyUnlockLocations(Il2CppImage* image)
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
                patchBool("get_IsDiscovered", true);
                patchBool("get_LockedByQuest", false);
                patchBool("get_DisabledForScav", false);

                patchBool("get_ForceOnlineRaidInPVE", false);  // Don't force online
                patchBool("get_ForceOfflineRaidInPVE", true);   // DO force offline
                                                                // yes, these are retarded comments, I am on 1 hour of sleep and 2 monster cans
                patchInt("get_RequiredPlayerLevelMin", 1);
                patchInt("get_RequiredPlayerLevelMax", 100);
                patchInt("get_MinPlayerLvlAccessKeys", 0);
                patchNullRet("get_AccessKeys");
                return true;
            };

        if (!patchLocationClass("JsonType", "LocationSettings/Location"))
        {
            patchLocationClass("", "Location"); // fallback
        }

        // FFS, JUST KILL ME ALREADY: Unlock routes/exits after location patches
        UnlockLocationRoutes(image);


    }

    void UnlockLocationRoutes(Il2CppImage* image)
    {
        // Routes are controlled by LocationSelectView and TravelRoute classes
        printf("[*] Unlocking location routes/exits...\n");

        // Patch LocationSelectView (controls route selection UI)
        Il2CppClass* locSelectView = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.UI.Matchmaker", "LocationSelectView");
        if (locSelectView)
        {
            auto patchBool = [&](const char* name, bool val)
                {
                    const Il2CppMethod* m = FindMethodRecursive(locSelectView, name);
                    if (!m) return;
                    void* fn = *(void**)m;
                    if (!fn) return;
                    PatchHelpers::PatchBoolGetter("UnlockRoutes", fn, val);
                    printf("[+] LocationSelectView.%s patched => %s\n", name, val ? "true" : "false");
                };

            patchBool("get_IsAvailable", true);
            patchBool("get_IsLocked", false);
            patchBool("get_IsDiscovered", true);
        }

        // Patch TravelRoute (individual route/exit control)
        Il2CppClass* travelRoute = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.UI", "TravelRoute");
        if (!travelRoute)
            travelRoute = (Il2CppClass*)il2cpp_class_from_name(image, "", "TravelRoute");

        if (travelRoute)
        {
            auto patchBool = [&](const char* name, bool val)
                {
                    const Il2CppMethod* m = FindMethodRecursive(travelRoute, name);
                    if (!m) return;
                    void* fn = *(void**)m;
                    if (!fn) return;
                    PatchHelpers::PatchBoolGetter("UnlockRoutes", fn, val);
                    printf("[+] TravelRoute.%s patched => %s\n", name, val ? "true" : "false");
                };

            patchBool("get_IsAvailable", true);
            patchBool("get_IsLocked", false);
            patchBool("get_IsDiscovered", true);
        }

        // Patch LocationSettings (contains available routes list)
        Il2CppClass* locSettings = (Il2CppClass*)il2cpp_class_from_name(image, "JsonType", "LocationSettings");
        if (!locSettings)
            locSettings = (Il2CppClass*)il2cpp_class_from_name(image, "", "LocationSettings");

        if (locSettings)
        {
            auto patchRet = [&](const char* name)
                {
                    const Il2CppMethod* m = FindMethodRecursive(locSettings, name);
                    if (!m) return;
                    void* fn = *(void**)m;
                    if (!fn) return;
                    const uint8_t retOnly[1] = { 0xC3 };
                    PatchHelpers::PatchBytes("UnlockRoutes", fn, retOnly, sizeof(retOnly));
                    printf("[+] LocationSettings.%s patched => ret\n", name);
                };

            patchRet("ValidateRoutes");
            patchRet("FilterUnavailableRoutes");
        }

        printf("[+] Route unlock patches applied\n");
    }
    /*
    void UpdateLocationRoutes(Il2CppImage* image)
    {
        // This was causing crashes because it was called every frame
        // Now it's called once at startup via UnlockLocationRoutes
        return;
    }
    */
}