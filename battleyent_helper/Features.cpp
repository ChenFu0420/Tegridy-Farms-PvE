#include "Features.h"
#include "Menu.h"
#include "il2api.h"
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

// ========================================
// External Globals (defined in dllmain.cpp)
// ========================================
extern void* g_TarkovApplicationInstance;
extern void* g_CameraManagerInstance;
extern uintptr_t g_selectedLocation;

// ========================================
// Helper Function: Find Method by Name
// ========================================
const Il2CppMethod* FindMethod(Il2CppClass* klass, const char* methodName)
{
    if (!klass || !methodName) return nullptr;

    void* iter = nullptr;
    const Il2CppMethod* method = nullptr;

    while ((method = il2cpp_class_get_methods(klass, &iter)))
    {
        const char* name = il2cpp_method_get_name(method);
        if (name && strcmp(name, methodName) == 0)
        {
            return method;
        }
    }

    return nullptr;
}

// ========================================
// PART 1: Recoverable Patch System
// Reference: RecoverablePatchesAiTeleAndAdvapiSource.txt lines 199-262
// ========================================
namespace PatchSystem
{
    std::unordered_map<std::string, PatchBackup> g_patchBackups;

    void CreatePatch(const char* name, void* address, const std::vector<uint8_t>& patchBytes)
    {
        if (!name || !address || patchBytes.empty())
        {
            printf("[-] CreatePatch failed: Invalid parameters\n");
            return;
        }

        PatchBackup backup;
        backup.address = address;
        backup.patchedBytes = patchBytes;
        backup.originalBytes.resize(patchBytes.size());
        backup.featureName = name;
        backup.isPatched = false;

        // Save original bytes
        memcpy(backup.originalBytes.data(), address, patchBytes.size());

        g_patchBackups[name] = backup;
    }

    void EnablePatch(const char* name)
    {
        auto it = g_patchBackups.find(name);
        if (it == g_patchBackups.end())
        {
            printf("[-] EnablePatch: Patch '%s' not found\n", name);
            return;
        }

        auto& patch = it->second;
        if (patch.isPatched)
        {
            return; // Already patched
        }

        DWORD oldProtect;
        if (!VirtualProtect(patch.address, patch.patchedBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            printf("[-] EnablePatch: VirtualProtect failed for '%s'\n", name);
            return;
        }

        memcpy(patch.address, patch.patchedBytes.data(), patch.patchedBytes.size());
        VirtualProtect(patch.address, patch.patchedBytes.size(), oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), patch.address, patch.patchedBytes.size());

        patch.isPatched = true;
        printf("[+] Enabled: %s\n", name);
    }

    void DisablePatch(const char* name)
    {
        auto it = g_patchBackups.find(name);
        if (it == g_patchBackups.end())
        {
            printf("[-] DisablePatch: Patch '%s' not found\n", name);
            return;
        }

        auto& patch = it->second;
        if (!patch.isPatched)
        {
            return; // Already restored
        }

        DWORD oldProtect;
        if (!VirtualProtect(patch.address, patch.originalBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            printf("[-] DisablePatch: VirtualProtect failed for '%s'\n", name);
            return;
        }

        memcpy(patch.address, patch.originalBytes.data(), patch.originalBytes.size());
        VirtualProtect(patch.address, patch.originalBytes.size(), oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), patch.address, patch.originalBytes.size());

        patch.isPatched = false;
        printf("[-] Disabled: %s\n", name);
    }

    bool IsPatched(const char* name)
    {
        auto it = g_patchBackups.find(name);
        if (it == g_patchBackups.end())
        {
            return false;
        }
        return it->second.isPatched;
    }

    void CreateRETPatch(const char* name, const Il2CppMethod* method)
    {
        if (!method)
        {
            printf("[-] CreateRETPatch: Method is null for '%s'\n", name);
            return;
        }

        void* fn = *(void**)method;
        if (!fn)
        {
            printf("[-] CreateRETPatch: Method pointer is null for '%s'\n", name);
            return;
        }

        // Create RET (0xC3) patch
        std::vector<uint8_t> retBytes = { 0xC3 };
        CreatePatch(name, fn, retBytes);
    }

    void CreateBoolGetterPatch(const char* name, void* methodPtr, bool returnValue)
    {
        if (!methodPtr)
        {
            printf("[-] CreateBoolGetterPatch: Method pointer is null for '%s'\n", name);
            return;
        }

        // x86-64 assembly:
        // mov al, <returnValue>  ; 0xB0 <value>
        // ret                     ; 0xC3
        std::vector<uint8_t> patch = { 0xB0, returnValue ? (uint8_t)1 : (uint8_t)0, 0xC3 };
        CreatePatch(name, methodPtr, patch);
    }

    void CreateFloatGetterPatch(const char* name, void* methodPtr, float returnValue)
    {
        if (!methodPtr)
        {
            printf("[-] CreateFloatGetterPatch: Method pointer is null for '%s'\n", name);
            return;
        }

        // x86-64 assembly to return a float in xmm0:
        // movss xmm0, dword ptr [rip+0x00]  ; Load from memory right after ret
        // ret
        // <float value as 4 bytes>
        
        // Allocate memory for the float value (never freed, intentional)
        float* floatPtr = new float(returnValue);

        // Calculate relative offset from instruction end to float location
        // Instruction: F3 0F 10 05 [4-byte offset] (7 bytes)
        // After instruction + ret (1 byte) = 8 bytes total before float
        int32_t offset = 1; // Float comes right after ret

        std::vector<uint8_t> patch;
        patch.push_back(0xF3); // MOVSS prefix
        patch.push_back(0x0F);
        patch.push_back(0x10);
        patch.push_back(0x05); // ModRM byte for RIP-relative addressing
        
        // Add 4-byte offset
        patch.push_back(offset & 0xFF);
        patch.push_back((offset >> 8) & 0xFF);
        patch.push_back((offset >> 16) & 0xFF);
        patch.push_back((offset >> 24) & 0xFF);
        
        patch.push_back(0xC3); // RET

        // Append float bytes
        uint8_t* floatBytes = reinterpret_cast<uint8_t*>(floatPtr);
        for (int i = 0; i < 4; i++)
        {
            patch.push_back(floatBytes[i]);
        }

        CreatePatch(name, methodPtr, patch);
    }
}

// ========================================
// PART 2: Legacy Patch Helpers (for compatibility)
// ========================================
namespace PatchHelpers
{
    void PatchMethodsWithLogging(Il2CppClass* klass, const char* namespaceName, const char* className, const std::vector<std::string>& methodNames)
    {
        if (!klass)
        {
            printf("[-] PatchMethodsWithLogging: Class is null\n");
            return;
        }

        for (const auto& methodName : methodNames)
        {
            const Il2CppMethod* method = FindMethod(klass, methodName.c_str());
            if (method)
            {
                void* fn = *(void**)method;
                if (fn)
                {
                    DWORD oldProtect;
                    if (VirtualProtect(fn, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
                    {
                        *(uint8_t*)fn = 0xC3; // RET
                        VirtualProtect(fn, 1, oldProtect, &oldProtect);
                        printf("[+] Patched: %s::%s::%s\n", namespaceName, className, methodName.c_str());
                    }
                }
            }
        }
    }

    void PatchBoolGetter(void* methodPtr, bool returnValue)
    {
        if (!methodPtr) return;

        DWORD oldProtect;
        if (VirtualProtect(methodPtr, 3, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            uint8_t* bytes = (uint8_t*)methodPtr;
            bytes[0] = 0xB0; // mov al, <value>
            bytes[1] = returnValue ? 1 : 0;
            bytes[2] = 0xC3; // ret
            VirtualProtect(methodPtr, 3, oldProtect, &oldProtect);
        }
    }

    void PatchFloatGetter(void* methodPtr, float returnValue)
    {
        if (!methodPtr) return;

        // Similar to PatchSystem::CreateFloatGetterPatch but applies immediately
        float* floatPtr = new float(returnValue);

        std::vector<uint8_t> patch;
        patch.push_back(0xF3);
        patch.push_back(0x0F);
        patch.push_back(0x10);
        patch.push_back(0x05);
        
        int32_t offset = 1;
        patch.push_back(offset & 0xFF);
        patch.push_back((offset >> 8) & 0xFF);
        patch.push_back((offset >> 16) & 0xFF);
        patch.push_back((offset >> 24) & 0xFF);
        
        patch.push_back(0xC3);

        uint8_t* floatBytes = reinterpret_cast<uint8_t*>(floatPtr);
        for (int i = 0; i < 4; i++)
        {
            patch.push_back(floatBytes[i]);
        }

        DWORD oldProtect;
        if (VirtualProtect(methodPtr, patch.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            memcpy(methodPtr, patch.data(), patch.size());
            VirtualProtect(methodPtr, patch.size(), oldProtect, &oldProtect);
        }
    }
}

// ========================================
// PART 3: Player Features - Reversible
// ========================================
namespace FeaturePatch
{
    void ApplyGodMode(Il2CppImage* image)
    {
        // God mode is continuous write, not a patch
        // Handled in UpdateGodMode()
    }

    void ApplyInfiniteStamina(Il2CppImage* image)
    {
        if (Features::infinite_stamina)
        {
            if (!PatchSystem::IsPatched("infinite_stamina"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT", "Physical");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "ChangeStamina");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("infinite_stamina", method);
                        PatchSystem::EnablePatch("infinite_stamina");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("infinite_stamina"))
            {
                PatchSystem::DisablePatch("infinite_stamina");
            }
        }
    }

    void ApplyNoFallDamage(Il2CppImage* image)
    {
        if (Features::no_fall_damage)
        {
            if (!PatchSystem::IsPatched("no_fall_damage"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT", "Player");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "ApplyDamage");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_fall_damage", method);
                        PatchSystem::EnablePatch("no_fall_damage");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_fall_damage"))
            {
                PatchSystem::DisablePatch("no_fall_damage");
            }
        }
    }

    void ApplyNoEnergyDrain(Il2CppImage* image)
    {
        if (Features::no_energy_drain)
        {
            if (!PatchSystem::IsPatched("no_energy_drain"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.HealthSystem", "ActiveHealthController");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "ChangeEnergy");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_energy_drain", method);
                        PatchSystem::EnablePatch("no_energy_drain");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_energy_drain"))
            {
                PatchSystem::DisablePatch("no_energy_drain");
            }
        }
    }

    void ApplyNoHydrationDrain(Il2CppImage* image)
    {
        if (Features::no_hydration_drain)
        {
            if (!PatchSystem::IsPatched("no_hydration_drain"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.HealthSystem", "ActiveHealthController");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "ChangeHydration");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_hydration_drain", method);
                        PatchSystem::EnablePatch("no_hydration_drain");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_hydration_drain"))
            {
                PatchSystem::DisablePatch("no_hydration_drain");
            }
        }
    }

    void ApplyNoFatigue(Il2CppImage* image)
    {
        if (Features::no_fatigue)
        {
            if (!PatchSystem::IsPatched("no_fatigue"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT", "Physical");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "AddFatigue");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_fatigue", method);
                        PatchSystem::EnablePatch("no_fatigue");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_fatigue"))
            {
                PatchSystem::DisablePatch("no_fatigue");
            }
        }
    }

    void ApplyNoEnvironmentDamage(Il2CppImage* image)
    {
        // Combined feature: barbed wire + mines + sniper zones
        if (Features::no_environment_damage)
        {
            // Barbed wire
            if (!PatchSystem::IsPatched("no_barbed_wire"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.Interactive", "BarbedWire");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "ApplyDamage");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_barbed_wire", method);
                        PatchSystem::EnablePatch("no_barbed_wire");
                    }
                }
            }

            // Minefields
            if (!PatchSystem::IsPatched("no_minefield"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.Interactive", "Minefield");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "Explosion");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_minefield", method);
                        PatchSystem::EnablePatch("no_minefield");
                    }
                }
            }

            // Sniper zones
            if (!PatchSystem::IsPatched("no_sniper_zones"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.Interactive", "BorderZone");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "TryKillPlayer");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_sniper_zones", method);
                        PatchSystem::EnablePatch("no_sniper_zones");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_barbed_wire"))
            {
                PatchSystem::DisablePatch("no_barbed_wire");
            }
            if (PatchSystem::IsPatched("no_minefield"))
            {
                PatchSystem::DisablePatch("no_minefield");
            }
            if (PatchSystem::IsPatched("no_sniper_zones"))
            {
                PatchSystem::DisablePatch("no_sniper_zones");
            }
        }
    }

    void ApplyBreachAllDoors(Il2CppImage* image)
    {
        if (Features::breach_all_doors)
        {
            if (!PatchSystem::IsPatched("breach_all_doors"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.Interactive", "Door");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "get_CanBeBreached");
                    if (method)
                    {
                        void* fn = *(void**)method;
                        PatchSystem::CreateBoolGetterPatch("breach_all_doors", fn, true);
                        PatchSystem::EnablePatch("breach_all_doors");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("breach_all_doors"))
            {
                PatchSystem::DisablePatch("breach_all_doors");
            }
        }
    }

    void ApplyInstantSearch(Il2CppImage* image)
    {
        if (Features::instant_search)
        {
            if (!PatchSystem::IsPatched("instant_search"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.Interactive", "SearchableItem");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "get_SearchTimeLeft");
                    if (method)
                    {
                        void* fn = *(void**)method;
                        PatchSystem::CreateFloatGetterPatch("instant_search", fn, 0.0f);
                        PatchSystem::EnablePatch("instant_search");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("instant_search"))
            {
                PatchSystem::DisablePatch("instant_search");
            }
        }
    }

    void ApplyAIIgnore(Il2CppImage* image)
    {
        // Combined feature: AI tracking + AI attacks
        if (Features::ai_ignore)
        {
            // Disable AI tracking/vision
            if (!PatchSystem::IsPatched("ai_tracking"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT", "BotVision");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "CheckVisibility");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("ai_tracking", method);
                        PatchSystem::EnablePatch("ai_tracking");
                    }
                }
            }

            // Disable AI attacks
            if (!PatchSystem::IsPatched("ai_attacks"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT", "BotMemory");
                if (klass)
                {
                    std::vector<std::string> methods = {
                        "CanShoot", "IsVisible", "IsEnemy"
                    };
                    
                    for (const auto& methodName : methods)
                    {
                        std::string patchName = "ai_attacks_" + methodName;
                        if (!PatchSystem::IsPatched(patchName.c_str()))
                        {
                            const Il2CppMethod* method = FindMethod(klass, methodName.c_str());
                            if (method)
                            {
                                void* fn = *(void**)method;
                                PatchSystem::CreateBoolGetterPatch(patchName.c_str(), fn, false);
                                PatchSystem::EnablePatch(patchName.c_str());
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("ai_tracking"))
            {
                PatchSystem::DisablePatch("ai_tracking");
            }

            // Disable all AI attack patches
            std::vector<std::string> methods = { "CanShoot", "IsVisible", "IsEnemy" };
            for (const auto& methodName : methods)
            {
                std::string patchName = "ai_attacks_" + methodName;
                if (PatchSystem::IsPatched(patchName.c_str()))
                {
                    PatchSystem::DisablePatch(patchName.c_str());
                }
            }
        }
    }

    // ========================================
    // PART 4: Legacy Combat Features
    // ========================================
    void ApplyNoRecoil(Il2CppImage* image)
    {
        if (Features::no_recoil)
        {
            if (!PatchSystem::IsPatched("no_recoil"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT", "Player+FirearmController");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "ApplyRecoil");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_recoil", method);
                        PatchSystem::EnablePatch("no_recoil");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_recoil"))
            {
                PatchSystem::DisablePatch("no_recoil");
            }
        }
    }

    void ApplyNoWeaponDurability(Il2CppImage* image)
    {
        if (Features::no_weapon_durability)
        {
            if (!PatchSystem::IsPatched("no_weapon_durability"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.InventoryLogic", "Weapon");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "OnShot");
                    if (method)
                    {
                        PatchSystem::CreateRETPatch("no_weapon_durability", method);
                        PatchSystem::EnablePatch("no_weapon_durability");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_weapon_durability"))
            {
                PatchSystem::DisablePatch("no_weapon_durability");
            }
        }
    }

    void ApplyNoWeaponMalfunction(Il2CppImage* image)
    {
        if (Features::no_weapon_malfunction)
        {
            if (!PatchSystem::IsPatched("no_weapon_malfunction"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.InventoryLogic", "Weapon");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "get_CanJam");
                    if (method)
                    {
                        void* fn = *(void**)method;
                        PatchSystem::CreateBoolGetterPatch("no_weapon_malfunction", fn, false);
                        PatchSystem::EnablePatch("no_weapon_malfunction");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_weapon_malfunction"))
            {
                PatchSystem::DisablePatch("no_weapon_malfunction");
            }
        }
    }

    void ApplyNoWeaponOverheating(Il2CppImage* image)
    {
        if (Features::no_weapon_overheating)
        {
            if (!PatchSystem::IsPatched("no_weapon_overheating"))
            {
                Il2CppClass* klass = il2cpp_class_from_name(image, "EFT.InventoryLogic", "Weapon");
                if (klass)
                {
                    const Il2CppMethod* method = FindMethod(klass, "get_AllowOverheat");
                    if (method)
                    {
                        void* fn = *(void**)method;
                        PatchSystem::CreateBoolGetterPatch("no_weapon_overheating", fn, false);
                        PatchSystem::EnablePatch("no_weapon_overheating");
                    }
                }
            }
        }
        else
        {
            if (PatchSystem::IsPatched("no_weapon_overheating"))
            {
                PatchSystem::DisablePatch("no_weapon_overheating");
            }
        }
    }

    // ========================================
    // PART 5: Continuous Update Functions
    // ========================================
    void UpdateRaidSelectedLocation(Il2CppImage* image)
    {
        if (!g_TarkovApplicationInstance) return;

        // Reference: NewLuaOffsets.txt
        // TarkovApp + 0xD0 → RaidSettings + 0xA8 → SelectedLocation
        uintptr_t raidSettings = *(uintptr_t*)((uintptr_t)g_TarkovApplicationInstance + 0xD0);
        if (!raidSettings) return;

        uintptr_t selectedLocation = *(uintptr_t*)(raidSettings + 0xA8);
        if (!selectedLocation || g_selectedLocation == selectedLocation) return;

        // Force offline PVE raid
        *(bool*)(selectedLocation + 0x32) = false; // ForceOnlineRaidInPVE
        *(bool*)(selectedLocation + 0x33) = false; // ForceOfflineRaidInPVE

        g_selectedLocation = selectedLocation;
    }

    void UpdateGodMode(Il2CppImage* image)
    {
        if (!g_CameraManagerInstance) return;

        // Reference: NewLuaOffsets.txt
        // CameraManager + 0x18 → EffectsController + 0x108 → LocalPlayer + 0x940 → HealthController
        uintptr_t effectsController = *(uintptr_t*)((uintptr_t)g_CameraManagerInstance + 0x18);
        if (!effectsController) return;

        uintptr_t localPlayer = *(uintptr_t*)(effectsController + 0x108);
        if (!localPlayer) return;

        uintptr_t healthController = *(uintptr_t*)(localPlayer + 0x940);
        if (!healthController) return;

        // Set damage coefficient: -1.0f = god mode, 1.0f = normal
        float damageCoeff = Features::god_mode ? -1.0f : 1.0f;
        *(float*)(healthController + 0x34) = damageCoeff;
    }

    // ========================================
    // PART 6: Thermal/NVG
    // Reference: ThermalNvgSource.txt lines 1317-1377
    // ========================================
    void UpdateThermalNVG()
    {
        if (!g_CameraManagerInstance) return;

        // Reference: NewLuaOffsets.txt
        // CameraManager + 0x48 → ThermalVision
        uintptr_t thermalVision = *(uintptr_t*)((uintptr_t)g_CameraManagerInstance + 0x48);
        if (!thermalVision) return;

        if (Features::thermal_vision)
        {
            // Enable thermal with clean settings
            *(bool*)(thermalVision + 0x20) = true;   // TV_On
            *(bool*)(thermalVision + 0x21) = false;  // TV_IsNoisy
            *(bool*)(thermalVision + 0x23) = false;  // TV_IsMotionBlurred
            *(bool*)(thermalVision + 0x24) = false;  // TV_IsGlitch
        }
        else if (Features::night_vision)
        {
            // Enable night vision (just turn on thermal without effects)
            *(bool*)(thermalVision + 0x20) = true;
            *(bool*)(thermalVision + 0x21) = true;  // Keep some noise for NVG effect
        }
        else
        {
            // Disable all vision modes
            *(bool*)(thermalVision + 0x20) = false;
        }
    }

    // ========================================
    // PART 7: Master Function
    // ========================================
    void ApplyAllEnabledFeatures(Il2CppImage* image)
    {
        if (!image) return;

        // Player features
        ApplyGodMode(image);
        ApplyInfiniteStamina(image);
        ApplyNoFallDamage(image);
        ApplyNoEnergyDrain(image);
        ApplyNoHydrationDrain(image);
        ApplyNoFatigue(image);
        
        // Combined features
        ApplyNoEnvironmentDamage(image);
        ApplyAIIgnore(image);
        
        // Utility features
        ApplyBreachAllDoors(image);
        ApplyInstantSearch(image);

        // Legacy combat features (if needed)
        ApplyNoRecoil(image);
        ApplyNoWeaponDurability(image);
        ApplyNoWeaponMalfunction(image);
        ApplyNoWeaponOverheating(image);
    }
}

// ========================================
// PART 8: Thermal/NVG Namespace Wrapper
// ========================================
namespace ThermalVision
{
    void UpdateThermalNVG()
    {
        FeaturePatch::UpdateThermalNVG();
    }
}

// ========================================
// PART 9: ESP Features
// Reference: PlayerESP.cs, GamePlayer.cs, GameUtils.cs
// ========================================
namespace ESPFeatures
{
    std::vector<ESPPlayer> espPlayers;

    // Note: These functions are stubs for now
    // Full implementation requires camera access and more IL2CPP work
    // Will be completed in separate ESPFeatures.cpp file

    Vector3 WorldToScreen(Vector3 worldPos)
    {
        // TODO: Implement using IL2CPP Camera.WorldToScreenPoint
        return Vector3{ 0, 0, 0 };
    }

    bool IsScreenPointVisible(Vector3 screenPoint)
    {
        // TODO: Implement screen bounds check
        return false;
    }

    Vector3 GetPlayerPosition(void* player)
    {
        // TODO: Implement
        return Vector3{ 0, 0, 0 };
    }

    Vector3 GetPlayerHeadPosition(void* player)
    {
        // TODO: Implement
        return Vector3{ 0, 0, 0 };
    }

    bool IsPlayerAI(void* player)
    {
        // TODO: Implement
        return false;
    }

    bool IsPlayerBoss(void* player)
    {
        // TODO: Implement
        return false;
    }

    std::string GetPlayerName(void* player)
    {
        // TODO: Implement
        return "";
    }

    void UpdatePlayerESP(Il2CppImage* image)
    {
        // TODO: Full implementation in separate file
        // This is a placeholder
    }

    void RenderPlayerESP()
    {
        // TODO: Full implementation in separate file
        // This is a placeholder
    }
}

// ========================================
// PART 10: Item Duping
// Reference: NewLuaOffsets.txt (backpack structure)
// ========================================
namespace ItemDuping
{
    std::vector<BackpackItem> scannedItems;
    int selectedIndex = 0;

    void ReadIL2CPPString(uintptr_t stringPtr, char* buffer, size_t maxLen)
    {
        if (!stringPtr || !buffer)
        {
            if (buffer) buffer[0] = '\0';
            return;
        }

        // IL2CPP string structure: [object header][length (int32)][char data]
        // String data starts at offset 0x14
        const char* strData = (const char*)(stringPtr + 0x14);
        
        size_t len = 0;
        while (len < maxLen - 1 && strData[len] != '\0')
        {
            buffer[len] = strData[len];
            len++;
        }
        buffer[len] = '\0';
    }

    void ScanBackpack()
    {
        scannedItems.clear();

        if (!g_CameraManagerInstance)
        {
            printf("[-] CameraManager not found, cannot scan backpack\n");
            return;
        }

        // Reference: NewLuaOffsets.txt
        // CameraManager + 0x18 → EffectsController
        //   + 0x108 → LocalPlayer
        //     + 0x958 → InventoryController
        //       + 0x100 → Inventory
        //         + 0x18 → Equipment
        //           + 0x80 → Slots
        //             + 0x60 → Backpack
        //               + 0x48 → ContainedItem
        //                 + 0x78 → Grids
        //                   + 0x20 → Grid[0]
        //                     + 0x48 → ItemCollection
        //                       + 0x18 → ItemList
        //                         + 0x10 → _items (array)
        //                         + 0x18 → _size (int32)

        uintptr_t effectsController = *(uintptr_t*)((uintptr_t)g_CameraManagerInstance + 0x18);
        if (!effectsController) { printf("[-] EffectsController not found\n"); return; }

        uintptr_t localPlayer = *(uintptr_t*)(effectsController + 0x108);
        if (!localPlayer) { printf("[-] LocalPlayer not found\n"); return; }

        uintptr_t inventoryController = *(uintptr_t*)(localPlayer + 0x958);
        if (!inventoryController) { printf("[-] InventoryController not found\n"); return; }

        uintptr_t inventory = *(uintptr_t*)(inventoryController + 0x100);
        if (!inventory) { printf("[-] Inventory not found\n"); return; }

        uintptr_t equipment = *(uintptr_t*)(inventory + 0x18);
        if (!equipment) { printf("[-] Equipment not found\n"); return; }

        uintptr_t slots = *(uintptr_t*)(equipment + 0x80);
        if (!slots) { printf("[-] Slots not found\n"); return; }

        uintptr_t backpack = *(uintptr_t*)(slots + 0x60);
        if (!backpack) { printf("[-] Backpack not found\n"); return; }

        uintptr_t containedItem = *(uintptr_t*)(backpack + 0x48);
        if (!containedItem) { printf("[-] No item in backpack\n"); return; }

        uintptr_t grids = *(uintptr_t*)(containedItem + 0x78);
        if (!grids) { printf("[-] Grids not found\n"); return; }

        uintptr_t grid0 = *(uintptr_t*)(grids + 0x20);
        if (!grid0) { printf("[-] Grid[0] not found\n"); return; }

        uintptr_t itemCollection = *(uintptr_t*)(grid0 + 0x48);
        if (!itemCollection) { printf("[-] ItemCollection not found\n"); return; }

        uintptr_t itemList = *(uintptr_t*)(itemCollection + 0x18);
        if (!itemList) { printf("[-] ItemList not found\n"); return; }

        uintptr_t itemsArray = *(uintptr_t*)(itemList + 0x10);
        int32_t itemCount = *(int32_t*)(itemList + 0x18);

        if (!itemsArray || itemCount <= 0)
        {
            printf("[-] No items in backpack\n");
            return;
        }

        printf("[+] Found %d items in backpack:\n", itemCount);

        for (int i = 0; i < itemCount && i < 100; i++)
        {
            uintptr_t item = *(uintptr_t*)(itemsArray + 0x20 + (i * 8));
            if (!item) continue;

            BackpackItem bpItem;
            bpItem.itemPtr = (void*)item;
            bpItem.stackAddress = item + 0x24; // StackObjectsCount offset
            bpItem.currentStack = *(int32_t*)(bpItem.stackAddress);

            // Get item name
            uintptr_t itemTemplate = *(uintptr_t*)(item + 0x60);
            if (itemTemplate)
            {
                uintptr_t namePtr = *(uintptr_t*)(itemTemplate + 0x10);
                if (namePtr)
                {
                    char nameBuf[256];
                    ReadIL2CPPString(namePtr, nameBuf, sizeof(nameBuf));
                    bpItem.name = nameBuf;
                }
            }

            if (bpItem.name.empty())
            {
                bpItem.name = "Unknown Item";
            }

            scannedItems.push_back(bpItem);
            printf("  [%d] %s (Stack: %d)\n", i, bpItem.name.c_str(), bpItem.currentStack);
        }

        printf("[+] Backpack scan complete\n");
    }

    void DupeSelectedItem()
    {
        if (scannedItems.empty())
        {
            printf("[-] No items scanned. Use 'Scan Backpack' first.\n");
            return;
        }

        if (selectedIndex < 0 || selectedIndex >= (int)scannedItems.size())
        {
            printf("[-] Invalid selection index\n");
            return;
        }

        BackpackItem& item = scannedItems[selectedIndex];
        
        // Set stack count to 2 (simple dupe)
        *(int32_t*)(item.stackAddress) = 2;

        printf("[+] Duped: %s (set stack to 2)\n", item.name.c_str());
    }

    void SetStackAmount(int amount)
    {
        if (scannedItems.empty())
        {
            printf("[-] No items scanned\n");
            return;
        }

        if (selectedIndex < 0 || selectedIndex >= (int)scannedItems.size())
        {
            printf("[-] Invalid selection index\n");
            return;
        }

        BackpackItem& item = scannedItems[selectedIndex];
        *(int32_t*)(item.stackAddress) = amount;

        printf("[+] Set stack amount to %d for: %s\n", amount, item.name.c_str());
    }
}

// ========================================
// PART 11: AI Features
// Reference: RecoverablePatchesAiTeleAndAdvapiSource.txt (AI Teleport)
// Reference: NewLuaOffsets.txt (Wave Spawning)
// ========================================
namespace AIFeatures
{
    WaveData waveEditor[8] = {0};

    void TeleportAllAI()
    {
        // TODO: Full implementation requires GameWorld singleton access
        // This is a placeholder for now
        printf("[*] AI Teleport not yet implemented\n");
    }

    void ApplyWaveEdits()
    {
        if (!g_TarkovApplicationInstance)
        {
            printf("[-] TarkovApplication not found\n");
            return;
        }

        // Reference: NewLuaOffsets.txt
        // TarkovApp + 0xD0 → RaidSettings
        //   + 0xA8 → SelectedLocation
        //     + 0xB8 → Waves (array)
        //       Wave[i] + 0x10 → time_min
        //       Wave[i] + 0x14 → time_max
        //       Wave[i] + 0x18 → slots_min
        //       Wave[i] + 0x1C → slots_max
        //       Wave[i] + 0x2C → WildSpawnType (ID)

        uintptr_t raidSettings = *(uintptr_t*)((uintptr_t)g_TarkovApplicationInstance + 0xD0);
        if (!raidSettings)
        {
            printf("[-] RaidSettings not found\n");
            return;
        }

        uintptr_t selectedLocation = *(uintptr_t*)(raidSettings + 0xA8);
        if (!selectedLocation)
        {
            printf("[-] SelectedLocation not found\n");
            return;
        }

        uintptr_t waves = *(uintptr_t*)(selectedLocation + 0xB8);
        if (!waves)
        {
            printf("[-] Waves array not found\n");
            return;
        }

        printf("[+] Applying wave edits...\n");

        for (int i = 0; i < 8; i++)
        {
            uintptr_t wave = *(uintptr_t*)(waves + 0x20 + (i * 8));
            if (!wave) continue;

            if (waveEditor[i].spawnID > 0)
            {
                *(int32_t*)(wave + 0x2C) = waveEditor[i].spawnID;
            }
            if (waveEditor[i].countMin > 0)
            {
                *(int32_t*)(wave + 0x18) = waveEditor[i].countMin;
            }
            if (waveEditor[i].countMax > 0)
            {
                *(int32_t*)(wave + 0x1C) = waveEditor[i].countMax;
            }
            if (waveEditor[i].timeMin > 0)
            {
                *(int32_t*)(wave + 0x10) = waveEditor[i].timeMin;
            }
            if (waveEditor[i].timeMax > 0)
            {
                *(int32_t*)(wave + 0x14) = waveEditor[i].timeMax;
            }

            printf("  [Wave %d] ID=%d, Count=%d-%d, Time=%d-%d\n",
                i, waveEditor[i].spawnID,
                waveEditor[i].countMin, waveEditor[i].countMax,
                waveEditor[i].timeMin, waveEditor[i].timeMax);
        }

        printf("[+] Wave edits applied\n");
    }
}
