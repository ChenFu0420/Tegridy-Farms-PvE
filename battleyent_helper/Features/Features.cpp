#include "Features.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>

// External globals from dllmain.cpp
extern void* g_TarkovApplicationInstance;
extern void* g_CameraManagerInstance;
extern uintptr_t g_selectedLocation;

// IL2CPP function pointer typedefs (matching dllmain.cpp)
struct Il2CppImage;
struct Il2CppClass;
struct Il2CppMethod;

typedef Il2CppClass* (*il2cpp_class_from_name_prot)(const Il2CppImage*, const char*, const char*);
typedef const Il2CppMethod* (*il2cpp_class_get_methods_prot)(Il2CppClass*, void**);
typedef const char* (*il2cpp_method_get_name_prot)(const Il2CppMethod*);

// External IL2CPP function pointers from dllmain.cpp
extern il2cpp_class_from_name_prot il2cpp_class_from_name;
extern il2cpp_class_get_methods_prot il2cpp_class_get_methods;
extern il2cpp_method_get_name_prot il2cpp_method_get_name;

// Additional IL2CPP typedefs for AI Vacuum
struct Il2CppObject;
struct Il2CppField;
struct Il2CppType;
struct Il2CppDomain;
struct Il2CppAssembly;

typedef Il2CppImage* (*il2cpp_image_loaded_prot)(const char*);
typedef Il2CppClass* (*il2cpp_object_get_class_prot)(void*);
typedef void* (*il2cpp_class_get_type_prot)(Il2CppClass*);
typedef void* (*il2cpp_type_get_object_prot)(void*);
typedef void* (*il2cpp_runtime_invoke_prot)(const Il2CppMethod*, void*, void**, void**);
typedef Il2CppClass* (*il2cpp_class_get_parent_prot)(Il2CppClass*);
typedef void* (*il2cpp_class_get_fields_prot)(Il2CppClass*, void**);
typedef const char* (*il2cpp_field_get_name_prot)(void*);
typedef void (*il2cpp_field_get_value_prot)(void*, void*, void*);
typedef uint32_t(*il2cpp_method_get_param_count_prot)(const Il2CppMethod*);
typedef void* (*il2cpp_object_unbox_prot)(void*);
typedef Il2CppObject* (*il2cpp_string_new_prot)(const char*);
typedef uint32_t(*il2cpp_method_get_flags_prot)(const Il2CppMethod*, uint32_t*);

extern il2cpp_object_get_class_prot il2cpp_object_get_class;
extern il2cpp_class_get_type_prot il2cpp_class_get_type;
extern il2cpp_type_get_object_prot il2cpp_type_get_object;
extern il2cpp_runtime_invoke_prot il2cpp_runtime_invoke;
extern il2cpp_class_get_parent_prot il2cpp_class_get_parent;
extern il2cpp_class_get_fields_prot il2cpp_class_get_fields;
extern il2cpp_field_get_name_prot il2cpp_field_get_name;
extern il2cpp_field_get_value_prot il2cpp_field_get_value;
extern il2cpp_method_get_param_count_prot il2cpp_method_get_param_count;
extern il2cpp_object_unbox_prot il2cpp_object_unbox;

// Additional exports from dllmain.cpp for Item Spawner
extern il2cpp_string_new_prot il2cpp_string_new;
extern il2cpp_method_get_flags_prot il2cpp_method_get_flags;

// External wrapper function from dllmain.cpp
extern Il2CppImage* il2cpp_image_loaded(const char* image_name);

// External helper functions from dllmain.cpp
extern const Il2CppMethod* FindMethod(Il2CppClass* klass, const char* methodName);
extern void PatchBoolGetter(void* methodPtr, bool returnValue, unsigned char* savedBytes);
extern void RestoreBoolGetter(void* methodPtr, unsigned char* savedBytes);
extern void PatchFloatGetter(void* methodPtr, float returnValue, unsigned char* savedBytes);
extern void RestoreFloatGetter(void* methodPtr, unsigned char* savedBytes);

// ========================================
// Feature Toggle States
// ========================================
namespace Features
{
    bool god_mode = false;
    bool infinite_stamina = false;
    bool no_recoil = false;
    bool no_weapon_durability = false;
    bool no_weapon_malfunction = false;
    bool no_weapon_overheating = false;
    bool ai_not_attack = false;
    bool breach_all_doors = false;
    bool lucky_search = false;

    // Planned features (placeholders)
    bool thermal_vision = false;
    bool night_vision = false;
    bool player_esp = false;
    bool loot_esp = false;
    float loot_esp_min_value = 10000.0f;
    bool item_spawner = false;
    bool loot_vacuum = false;

    bool menu_open = false;
}

// ========================================
// AI Vacuum Helper Structures and Functions
// Reference: AiVacuum.txt snippet
// ========================================

// Unity Vector3 Struct
struct Vector3 {
    float x, y, z;
};

// Simple C# Array Header Structure (IL2CPP)
struct Il2CppArraySize {
    void* klass;
    void* monitor;
    void* bounds;
    int32_t max_length;
};

// Helper: Get C# System.Type object from class name
void* GetSystemType(Il2CppClass* klass) {
    if (!klass) return nullptr;
    void* type = il2cpp_class_get_type(klass);
    return il2cpp_type_get_object(type);
}

// Core: Get current GameWorld instance (via FindObjectOfType)
void* GetGameWorldInstance() {
    Il2CppImage* coreModule = il2cpp_image_loaded("UnityEngine.CoreModule.dll");
    if (!coreModule) return nullptr;

    Il2CppClass* unityObjectClass = il2cpp_class_from_name(coreModule, "UnityEngine", "Object");

    const Il2CppMethod* findMethod = nullptr;
    void* iter = nullptr;
    while (const Il2CppMethod* m = il2cpp_class_get_methods(unityObjectClass, &iter)) {
        if (strcmp(il2cpp_method_get_name(m), "FindObjectOfType") == 0 &&
            il2cpp_method_get_param_count(m) == 1) {
            findMethod = m;
            break;
        }
    }
    if (!findMethod) return nullptr;

    Il2CppImage* csharpImage = il2cpp_image_loaded("Assembly-CSharp.dll");
    Il2CppClass* gameWorldClass = il2cpp_class_from_name(csharpImage, "EFT", "GameWorld");
    void* gameWorldType = GetSystemType(gameWorldClass);

    void* args[] = { gameWorldType };
    void* result = il2cpp_runtime_invoke(findMethod, nullptr, args, nullptr);

    return result;
}

// Recursively find method in class and its parents
const Il2CppMethod* FindMethodRecursive(Il2CppClass* klass, const char* name) {
    if (!klass) return nullptr;
    Il2CppClass* current = klass;
    while (current) {
        void* iter = nullptr;
        while (const Il2CppMethod* m = il2cpp_class_get_methods(current, &iter)) {
            if (strcmp(il2cpp_method_get_name(m), name) == 0) {
                return m;
            }
        }
        current = il2cpp_class_get_parent(current);
    }
    return nullptr;
}

// Recursively find field in class and its parents
void* FindFieldRecursive(Il2CppClass* klass, const char* name) {
    if (!klass) return nullptr;
    Il2CppClass* current = klass;
    while (current) {
        void* iter = nullptr;
        while (void* f = il2cpp_class_get_fields(current, &iter)) {
            const char* fname = il2cpp_field_get_name(f);
            if (fname && strcmp(fname, name) == 0) {
                return f;
            }
        }
        current = il2cpp_class_get_parent(current);
    }
    return nullptr;
}

// ========================================
// Recoverable Patch System
// Reference: Truth source lines 277-315 (patch_method_ret_recoverable/unpatch_method_ret)
// ========================================
namespace PatchSystem
{
    std::vector<PatchData> g_patches;

    bool CreatePatch(const char* name, const Il2CppMethod* method)
    {
        if (!name || !method)
        {
            printf("[-] CreatePatch: Invalid parameters for '%s'\n", name ? name : "NULL");
            return false;
        }

        // Check if already exists
        for (auto& patch : g_patches)
        {
            if (patch.name == name)
            {
                printf("[-] CreatePatch: Patch '%s' already exists\n", name);
                return false;
            }
        }

        void* fn = *(void**)method;
        if (!fn)
        {
            printf("[-] CreatePatch: Method pointer is null for '%s'\n", name);
            return false;
        }

        PatchData patch;
        patch.method = method;
        patch.functionPtr = fn;
        patch.isPatched = false;
        patch.name = name;

        // Save original bytes (16 bytes for safety)
        DWORD oldProtect;
        if (!VirtualProtect(fn, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            printf("[-] CreatePatch: VirtualProtect failed for '%s'\n", name);
            return false;
        }

        memcpy(patch.originalBytes, fn, 16);
        VirtualProtect(fn, 16, oldProtect, &oldProtect);

        g_patches.push_back(patch);
        printf("[+] Created patch: %s @ %p\n", name, fn);

        return true;
    }

    bool EnablePatch(const char* name)
    {
        PatchData* patch = FindPatch(name);
        if (!patch)
        {
            printf("[-] EnablePatch: Patch '%s' not found\n", name);
            return false;
        }

        if (patch->isPatched)
        {
            return true; // Already enabled
        }

        DWORD oldProtect;
        if (!VirtualProtect(patch->functionPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            printf("[-] EnablePatch: VirtualProtect failed for '%s'\n", name);
            return false;
        }

        // Apply RET patch (0xC3)
        *(uint8_t*)patch->functionPtr = 0xC3;

        VirtualProtect(patch->functionPtr, 16, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), patch->functionPtr, 16);

        patch->isPatched = true;
        printf("[+] Enabled: %s\n", name);

        return true;
    }

    bool DisablePatch(const char* name)
    {
        PatchData* patch = FindPatch(name);
        if (!patch)
        {
            printf("[-] DisablePatch: Patch '%s' not found\n", name);
            return false;
        }

        if (!patch->isPatched)
        {
            return true; // Already disabled
        }

        DWORD oldProtect;
        if (!VirtualProtect(patch->functionPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            printf("[-] DisablePatch: VirtualProtect failed for '%s'\n", name);
            return false;
        }

        // Restore original bytes
        memcpy(patch->functionPtr, patch->originalBytes, 16);

        VirtualProtect(patch->functionPtr, 16, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), patch->functionPtr, 16);

        patch->isPatched = false;
        printf("[-] Disabled: %s\n", name);

        return true;
    }

    bool IsPatched(const char* name)
    {
        PatchData* patch = FindPatch(name);
        return patch ? patch->isPatched : false;
    }

    PatchData* FindPatch(const char* name)
    {
        for (auto& patch : g_patches)
        {
            if (patch.name == name)
            {
                return &patch;
            }
        }
        return nullptr;
    }
}

// ========================================
// Feature Patch Functions
// Reference: Truth source original patch implementations
// ========================================
namespace FeaturePatch
{
    // Storage for bool/float getter patches (non-RET patches)
    struct BoolGetterPatch
    {
        void* functionPtr = nullptr;
        unsigned char originalBytes[16] = {};
        bool isPatched = false;
        std::string name;
    };

    struct FloatGetterPatch
    {
        void* functionPtr = nullptr;
        unsigned char originalBytes[16] = {};
        bool isPatched = false;
        std::string name;
    };

    std::vector<BoolGetterPatch> g_boolPatches;
    std::vector<FloatGetterPatch> g_floatPatches;

    // ========================================
    // Initialize All Patches (called once at startup)
    // ========================================
    void InitializeAllPatches(Il2CppImage* image)
    {
        if (!image)
        {
            printf("[-] InitializeAllPatches: image is null\n");
            return;
        }

        printf("[*] Initializing all feature patches...\n");

        // Infinite Stamina - Stamina::Process, Stamina::Consume
        {
            auto klass = (Il2CppClass*)il2cpp_class_from_name(image, "", "Stamina");
            if (klass)
            {
                const Il2CppMethod* processMethod = FindMethod(klass, "Process");
                const Il2CppMethod* consumeMethod = FindMethod(klass, "Consume");

                if (processMethod) PatchSystem::CreatePatch("stamina_process", processMethod);
                if (consumeMethod) PatchSystem::CreatePatch("stamina_consume", consumeMethod);
            }
        }

        // No Recoil - Multiple recoil classes
        {
            auto newRecoilShot = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Animations.NewRecoil", "NewRecoilShotEffect");
            if (newRecoilShot)
            {
                const Il2CppMethod* method = FindMethod(newRecoilShot, "AddRecoilForce");
                if (method) PatchSystem::CreatePatch("new_recoil_shot", method);
            }

            auto newRotationRecoil = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Animations", "NewRotationRecoilProcess");
            if (newRotationRecoil)
            {
                const Il2CppMethod* method = FindMethod(newRotationRecoil, "CalculateRecoil");
                if (method) PatchSystem::CreatePatch("new_rotation_recoil", method);
            }

            auto oldRecoilShot = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Animations", "OldRecoilShotEffect");
            if (oldRecoilShot)
            {
                const Il2CppMethod* method = FindMethod(oldRecoilShot, "AddRecoilForce");
                if (method) PatchSystem::CreatePatch("old_recoil_shot", method);
            }

            auto oldRotationRecoil = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Animations", "OldRotationRecoilProcess");
            if (oldRotationRecoil)
            {
                const Il2CppMethod* method = FindMethod(oldRotationRecoil, "CalculateRecoil");
                if (method) PatchSystem::CreatePatch("old_rotation_recoil", method);
            }

            auto recoilBase = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Animations", "RecoilProcessBase");
            if (recoilBase)
            {
                const Il2CppMethod* method = FindMethod(recoilBase, "CalculateRecoil");
                if (method) PatchSystem::CreatePatch("recoil_base", method);
            }
        }

        // Weapon Durability - Weapon::OnShot
        {
            auto weaponClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.InventoryLogic", "Weapon");
            if (weaponClass)
            {
                const Il2CppMethod* method = FindMethod(weaponClass, "OnShot");
                if (method) PatchSystem::CreatePatch("weapon_onshot", method);
            }
        }

        // Weapon Malfunction - Weapon::get_AllowMalfunction (bool getter)
        {
            auto weaponClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.InventoryLogic", "Weapon");
            if (weaponClass)
            {
                const Il2CppMethod* method = FindMethod(weaponClass, "get_AllowMalfunction");
                if (method)
                {
                    void* fn = *(void**)method;
                    if (fn)
                    {
                        BoolGetterPatch patch;
                        patch.functionPtr = fn;
                        patch.isPatched = false;
                        patch.name = "weapon_malfunction";
                        memcpy(patch.originalBytes, fn, 16);
                        g_boolPatches.push_back(patch);
                        printf("[+] Created bool getter patch: weapon_malfunction @ %p\n", fn);
                    }
                }
            }
        }

        // Weapon Overheat - Weapon::get_AllowOverheat (bool getter)
        {
            auto weaponClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.InventoryLogic", "Weapon");
            if (weaponClass)
            {
                const Il2CppMethod* method = FindMethod(weaponClass, "get_AllowOverheat");
                if (method)
                {
                    void* fn = *(void**)method;
                    if (fn)
                    {
                        BoolGetterPatch patch;
                        patch.functionPtr = fn;
                        patch.isPatched = false;
                        patch.name = "weapon_overheat";
                        memcpy(patch.originalBytes, fn, 16);
                        g_boolPatches.push_back(patch);
                        printf("[+] Created bool getter patch: weapon_overheat @ %p\n", fn);
                    }
                }
            }
        }

        // AI Not Attack - BotMemory methods
        {
            auto botMemoryClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT", "BotMemory");
            if (botMemoryClass)
            {
                const char* methodNames[] = { "Spotted", "SetLastTimeSeeEnemy", "LoseVisionCurrentEnemy", "LastEnemyVisionOld", "set_GoalEnemy" };
                for (int i = 0; i < 5; i++)
                {
                    const Il2CppMethod* method = FindMethod(botMemoryClass, methodNames[i]);
                    if (method)
                    {
                        char patchName[64];
                        sprintf_s(patchName, "ai_attack_%d", i);
                        PatchSystem::CreatePatch(patchName, method);
                    }
                }
            }
        }

        // Breach All Doors - Door::BreachSuccessRoll (bool getter)
        {
            auto doorClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Interactive", "Door");
            if (doorClass)
            {
                const Il2CppMethod* method = FindMethod(doorClass, "BreachSuccessRoll");
                if (method)
                {
                    void* fn = *(void**)method;
                    if (fn)
                    {
                        BoolGetterPatch patch;
                        patch.functionPtr = fn;
                        patch.isPatched = false;
                        patch.name = "breach_doors";
                        memcpy(patch.originalBytes, fn, 16);
                        g_boolPatches.push_back(patch);
                        printf("[+] Created bool getter patch: breach_doors @ %p\n", fn);
                    }
                }
            }
        }

        // Lucky Search - SkillManager::get_AttentionEliteLuckySearchValue (float getter)
        {
            auto skillClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT", "SkillManager");
            if (skillClass)
            {
                const Il2CppMethod* method = FindMethod(skillClass, "get_AttentionEliteLuckySearchValue");
                if (method)
                {
                    void* fn = *(void**)method;
                    if (fn)
                    {
                        FloatGetterPatch patch;
                        patch.functionPtr = fn;
                        patch.isPatched = false;
                        patch.name = "lucky_search";
                        memcpy(patch.originalBytes, fn, 16);
                        g_floatPatches.push_back(patch);
                        printf("[+] Created float getter patch: lucky_search @ %p\n", fn);
                    }
                }
            }
        }

        // God Mode - ActiveHealthController (player status + fall damage)
        {
            auto healthClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.HealthSystem", "ActiveHealthController");
            if (healthClass)
            {
                const char* methodNames[] = { "ChangeHydration", "ChangeEnergy", "HandleFall", "SetEncumbered", "SetOverEncumbered", "AddFatigue" };
                for (int i = 0; i < 6; i++)
                {
                    const Il2CppMethod* method = FindMethod(healthClass, methodNames[i]);
                    if (method)
                    {
                        char patchName[64];
                        sprintf_s(patchName, "godmode_health_%d", i);
                        PatchSystem::CreatePatch(patchName, method);
                    }
                }
            }
        }

        // God Mode - BarbedWire (environment damage)
        {
            auto barbedWireClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Interactive", "BarbedWire");
            if (barbedWireClass)
            {
                const char* methodNames[] = { "AddPenalty", "ProceedDamage" };
                for (int i = 0; i < 2; i++)
                {
                    const Il2CppMethod* method = FindMethod(barbedWireClass, methodNames[i]);
                    if (method)
                    {
                        char patchName[64];
                        sprintf_s(patchName, "godmode_barbed_%d", i);
                        PatchSystem::CreatePatch(patchName, method);
                    }
                }
            }
        }

        // God Mode - Minefield (environment damage)
        {
            auto minefieldClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Interactive", "Minefield");
            if (minefieldClass)
            {
                const char* methodNames[] = { "DealExplosionDamage", "Explode" };
                for (int i = 0; i < 2; i++)
                {
                    const Il2CppMethod* method = FindMethod(minefieldClass, methodNames[i]);
                    if (method)
                    {
                        char patchName[64];
                        sprintf_s(patchName, "godmode_minefield_%d", i);
                        PatchSystem::CreatePatch(patchName, method);
                    }
                }
            }
        }

        // God Mode - SniperFiringZone (environment damage)
        {
            auto sniperClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT.Interactive", "SniperFiringZone");
            if (sniperClass)
            {
                const Il2CppMethod* method = FindMethod(sniperClass, "Shoot");
                if (method) PatchSystem::CreatePatch("godmode_sniper", method);
            }
        }

        printf("[+] All feature patches initialized\n");
    }

    // ========================================
    // Individual Feature Apply Functions
    // ========================================

    void ApplyGodMode(Il2CppImage* image)
    {
        // God mode enables/disables multiple patches:
        // - ActiveHealthController (hydration, energy, fall damage, fatigue, encumbered)
        // - BarbedWire (environment damage)
        // - Minefield (environment damage)
        // - SniperFiringZone (environment damage)
        // UpdateGodMode() handles continuous memory write for damage coefficient

        if (Features::god_mode)
        {
            // Enable all god mode patches
            for (int i = 0; i < 6; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "godmode_health_%d", i);
                PatchSystem::EnablePatch(patchName);
            }
            for (int i = 0; i < 2; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "godmode_barbed_%d", i);
                PatchSystem::EnablePatch(patchName);
            }
            for (int i = 0; i < 2; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "godmode_minefield_%d", i);
                PatchSystem::EnablePatch(patchName);
            }
            PatchSystem::EnablePatch("godmode_sniper");
        }
        else
        {
            // Disable all god mode patches
            for (int i = 0; i < 6; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "godmode_health_%d", i);
                PatchSystem::DisablePatch(patchName);
            }
            for (int i = 0; i < 2; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "godmode_barbed_%d", i);
                PatchSystem::DisablePatch(patchName);
            }
            for (int i = 0; i < 2; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "godmode_minefield_%d", i);
                PatchSystem::DisablePatch(patchName);
            }
            PatchSystem::DisablePatch("godmode_sniper");
        }
    }

    void ApplyInfiniteStamina(Il2CppImage* image)
    {
        if (Features::infinite_stamina)
        {
            PatchSystem::EnablePatch("stamina_process");
            PatchSystem::EnablePatch("stamina_consume");
        }
        else
        {
            PatchSystem::DisablePatch("stamina_process");
            PatchSystem::DisablePatch("stamina_consume");
        }
    }

    void ApplyNoRecoil(Il2CppImage* image)
    {
        if (Features::no_recoil)
        {
            PatchSystem::EnablePatch("new_recoil_shot");
            PatchSystem::EnablePatch("new_rotation_recoil");
            PatchSystem::EnablePatch("old_recoil_shot");
            PatchSystem::EnablePatch("old_rotation_recoil");
            PatchSystem::EnablePatch("recoil_base");
        }
        else
        {
            PatchSystem::DisablePatch("new_recoil_shot");
            PatchSystem::DisablePatch("new_rotation_recoil");
            PatchSystem::DisablePatch("old_recoil_shot");
            PatchSystem::DisablePatch("old_rotation_recoil");
            PatchSystem::DisablePatch("recoil_base");
        }
    }

    void ApplyNoWeaponDurability(Il2CppImage* image)
    {
        if (Features::no_weapon_durability)
        {
            PatchSystem::EnablePatch("weapon_onshot");
        }
        else
        {
            PatchSystem::DisablePatch("weapon_onshot");
        }
    }

    void ApplyNoWeaponMalfunction(Il2CppImage* image)
    {
        // Find bool getter patch
        for (auto& patch : g_boolPatches)
        {
            if (patch.name == "weapon_malfunction")
            {
                if (!patch.functionPtr)
                {
                    printf("[-] weapon_malfunction: functionPtr is null\n");
                    break;
                }

                if (Features::no_weapon_malfunction && !patch.isPatched)
                {
                    PatchBoolGetter(patch.functionPtr, false, nullptr);
                    patch.isPatched = true;
                    printf("[+] Enabled: weapon_malfunction\n");
                }
                else if (!Features::no_weapon_malfunction && patch.isPatched)
                {
                    RestoreBoolGetter(patch.functionPtr, patch.originalBytes);
                    patch.isPatched = false;
                    printf("[-] Disabled: weapon_malfunction\n");
                }
                break;
            }
        }
    }

    void ApplyNoWeaponOverheating(Il2CppImage* image)
    {
        // Find bool getter patch
        for (auto& patch : g_boolPatches)
        {
            if (patch.name == "weapon_overheat")
            {
                if (!patch.functionPtr)
                {
                    printf("[-] weapon_overheat: functionPtr is null\n");
                    break;
                }

                if (Features::no_weapon_overheating && !patch.isPatched)
                {
                    PatchBoolGetter(patch.functionPtr, false, nullptr);
                    patch.isPatched = true;
                    printf("[+] Enabled: weapon_overheat\n");
                }
                else if (!Features::no_weapon_overheating && patch.isPatched)
                {
                    RestoreBoolGetter(patch.functionPtr, patch.originalBytes);
                    patch.isPatched = false;
                    printf("[-] Disabled: weapon_overheat\n");
                }
                break;
            }
        }
    }

    void ApplyAINotAttack(Il2CppImage* image)
    {
        if (Features::ai_not_attack)
        {
            for (int i = 0; i < 5; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "ai_attack_%d", i);
                PatchSystem::EnablePatch(patchName);
            }
        }
        else
        {
            for (int i = 0; i < 5; i++)
            {
                char patchName[64];
                sprintf_s(patchName, "ai_attack_%d", i);
                PatchSystem::DisablePatch(patchName);
            }
        }
    }

    void ApplyBreachAllDoors(Il2CppImage* image)
    {
        // Find bool getter patch
        for (auto& patch : g_boolPatches)
        {
            if (patch.name == "breach_doors")
            {
                if (!patch.functionPtr)
                {
                    printf("[-] breach_doors: functionPtr is null\n");
                    break;
                }

                if (Features::breach_all_doors && !patch.isPatched)
                {
                    PatchBoolGetter(patch.functionPtr, true, nullptr);
                    patch.isPatched = true;
                    printf("[+] Enabled: breach_doors\n");
                }
                else if (!Features::breach_all_doors && patch.isPatched)
                {
                    RestoreBoolGetter(patch.functionPtr, patch.originalBytes);
                    patch.isPatched = false;
                    printf("[-] Disabled: breach_doors\n");
                }
                break;
            }
        }
    }

    void ApplyLuckySearch(Il2CppImage* image)
    {
        // Find float getter patch
        for (auto& patch : g_floatPatches)
        {
            if (patch.name == "lucky_search")
            {
                if (!patch.functionPtr)
                {
                    printf("[-] lucky_search: functionPtr is null\n");
                    break;
                }

                if (Features::lucky_search && !patch.isPatched)
                {
                    PatchFloatGetter(patch.functionPtr, 1.0f, nullptr);
                    patch.isPatched = true;
                    printf("[+] Enabled: lucky_search\n");
                }
                else if (!Features::lucky_search && patch.isPatched)
                {
                    RestoreFloatGetter(patch.functionPtr, patch.originalBytes);
                    patch.isPatched = false;
                    printf("[-] Disabled: lucky_search\n");
                }
                break;
            }
        }
    }

    // ========================================
    // Master Function
    // ========================================
    void ApplyAllEnabledFeatures(Il2CppImage* image)
    {
        ApplyGodMode(image);
        ApplyInfiniteStamina(image);
        ApplyNoRecoil(image);
        ApplyNoWeaponDurability(image);
        ApplyNoWeaponMalfunction(image);
        ApplyNoWeaponOverheating(image);
        ApplyAINotAttack(image);
        ApplyBreachAllDoors(image);
        ApplyLuckySearch(image);
    }
    // ========================================
        // Continuous Update Functions
        // Reference: Truth source main loop continuous updates
        // ========================================

    void UpdateGodMode()
    {
        if (!g_CameraManagerInstance) return;

        // Reference: Truth source offsets
        // CameraManager + 0x18 → EffectsController
        // EffectsController + 0x108 → LocalPlayer
        // LocalPlayer + 0x940 → HealthController
        // HealthController + 0x34 → DamageCoefficient

        uintptr_t effectsController = *(uintptr_t*)((uintptr_t)g_CameraManagerInstance + 0x18);
        if (!effectsController) return;

        uintptr_t localPlayer = *(uintptr_t*)(effectsController + 0x108);
        if (!localPlayer) return;

        uintptr_t healthController = *(uintptr_t*)(localPlayer + 0x940);
        if (!healthController) return;

        // Set damage coefficient based on god mode state
        float damageCoeff = Features::god_mode ? -1.0f : 1.0f;
        *(float*)(healthController + 0x34) = damageCoeff;
    }

    void UpdateOfflinePVE()
    {
        if (!g_TarkovApplicationInstance) return;

        // Reference: Truth source offsets
        // TarkovApplication + 0xD0 → RaidSettings
        // RaidSettings + 0xA8 → SelectedLocation
        // SelectedLocation + 0x32 → ForceOnlineRaid
        // SelectedLocation + 0x33 → ForceOfflineRaid

        uintptr_t raidSettings = *(uintptr_t*)((uintptr_t)g_TarkovApplicationInstance + 0xD0);
        if (!raidSettings) return;

        uintptr_t selectedLocation = *(uintptr_t*)(raidSettings + 0xA8);
        if (!selectedLocation || g_selectedLocation == selectedLocation) return;

        // Force offline PVE raid (permanent patch)
        *(bool*)(selectedLocation + 0x32) = false;  // ForceOnlineRaid = false
        *(bool*)(selectedLocation + 0x33) = true;   // ForceOfflineRaid = true

        g_selectedLocation = selectedLocation;
    }

    // ========================================
    // AI Vacuum - Teleport All Enemies
    // Reference: AiVacuum.txt snippet
    // ========================================
    void TeleportAllEnemiesToMe()
    {
        void* gameWorld = GetGameWorldInstance();
        if (!gameWorld)
        {
            printf("[-] GameWorld not found\n");
            return;
        }

        Il2CppClass* gwClass = il2cpp_object_get_class(gameWorld);
        void* playerList = nullptr;

        // Try AllAlivePlayersList
        const Il2CppMethod* getAlivePlayers = FindMethodRecursive(gwClass, "get_AllAlivePlayersList");
        if (getAlivePlayers) {
            playerList = il2cpp_runtime_invoke(getAlivePlayers, gameWorld, nullptr, nullptr);
        }
        else {
            void* f = FindFieldRecursive(gwClass, "AllAlivePlayersList");
            if (f) il2cpp_field_get_value(gameWorld, f, &playerList);

            if (!playerList) {
                const Il2CppMethod* getReg = FindMethodRecursive(gwClass, "get_RegisteredPlayers");
                if (getReg) playerList = il2cpp_runtime_invoke(getReg, gameWorld, nullptr, nullptr);
            }
        }

        if (!playerList) {
            printf("[-] Player list not found\n");
            return;
        }

        // Read List<T> internal fields
        Il2CppClass* listClass = il2cpp_object_get_class(playerList);
        void* itemsField = FindFieldRecursive(listClass, "_items");
        void* sizeField = FindFieldRecursive(listClass, "_size");

        if (!itemsField || !sizeField) {
            printf("[-] Cannot resolve List structure\n");
            return;
        }

        int32_t count = 0;
        il2cpp_field_get_value(playerList, sizeField, &count);

        void* itemsArray = nullptr;
        il2cpp_field_get_value(playerList, itemsField, &itemsArray);

        if (!itemsArray) {
            printf("[-] Player list array is empty/null\n");
            return;
        }

        if (count < 0 || count > 200) {
            printf("[-] Player count abnormal: %d\n", count);
            return;
        }

        printf("[TP] Player count: %d\n", count);

        // Find local player
        void* localPlayer = nullptr;
        Vector3 myPosition = { 0, 0, 0 };
        void** players = (void**)((uintptr_t)itemsArray + 32);

        for (int i = 0; i < count; i++) {
            void* player = players[i];
            if (!player) continue;

            Il2CppClass* playerClass = il2cpp_object_get_class(player);
            const Il2CppMethod* isYourPlayerMethod = FindMethodRecursive(playerClass, "get_IsYourPlayer");

            if (isYourPlayerMethod) {
                void* result = il2cpp_runtime_invoke(isYourPlayerMethod, player, nullptr, nullptr);
                bool isMe = false;
                if (result) isMe = *(bool*)il2cpp_object_unbox(result);

                if (isMe) {
                    localPlayer = player;
                    const Il2CppMethod* getTransform = FindMethodRecursive(playerClass, "get_transform");
                    if (getTransform) {
                        void* transform = il2cpp_runtime_invoke(getTransform, player, nullptr, nullptr);
                        if (transform) {
                            Il2CppClass* transformClass = il2cpp_object_get_class(transform);
                            const Il2CppMethod* getPos = FindMethodRecursive(transformClass, "get_position");
                            if (getPos) {
                                void* boxedPos = il2cpp_runtime_invoke(getPos, transform, nullptr, nullptr);
                                if (boxedPos) {
                                    myPosition = *(Vector3*)il2cpp_object_unbox(boxedPos);
                                    printf("[TP] Found Self @ %p, Pos: %.2f, %.2f, %.2f\n", player, myPosition.x, myPosition.y, myPosition.z);
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        if (!localPlayer || (myPosition.x == 0 && myPosition.y == 0 && myPosition.z == 0)) {
            printf("[-] Local player not found or coordinates invalid\n");
            return;
        }

        // Teleport enemies
        printf("[TP] Starting enemy teleport...\n");
        for (int i = 0; i < count; i++) {
            void* player = players[i];
            if (!player || player == localPlayer) continue;

            // Stop AI movement
            Il2CppClass* pClass = il2cpp_object_get_class(player);
            const Il2CppMethod* getAIData = FindMethodRecursive(pClass, "get_AIData");
            if (getAIData) {
                void* aiData = il2cpp_runtime_invoke(getAIData, player, nullptr, nullptr);
                if (aiData) {
                    Il2CppClass* aiDataClass = il2cpp_object_get_class(aiData);
                    const Il2CppMethod* getBotOwner = FindMethodRecursive(aiDataClass, "get_BotOwner");
                    if (getBotOwner) {
                        void* botOwner = il2cpp_runtime_invoke(getBotOwner, aiData, nullptr, nullptr);
                        if (botOwner) {
                            Il2CppClass* botOwnerClass = il2cpp_object_get_class(botOwner);
                            const Il2CppMethod* stopMove = FindMethodRecursive(botOwnerClass, "StopMove");
                            if (stopMove) {
                                il2cpp_runtime_invoke(stopMove, botOwner, nullptr, nullptr);
                            }
                        }
                    }
                }
            }

            // Try using Player.Teleport method
            const Il2CppMethod* teleportMethod = nullptr;
            void* iter = nullptr;
            while (const Il2CppMethod* m = il2cpp_class_get_methods(pClass, &iter)) {
                if (strcmp(il2cpp_method_get_name(m), "Teleport") == 0 &&
                    il2cpp_method_get_param_count(m) == 2) {
                    teleportMethod = m;
                    break;
                }
            }

            Vector3 targetPos = myPosition;
            targetPos.z += 1.0f;
            targetPos.y += 0.5f;

            if (teleportMethod) {
                bool force = true;
                void* args[] = { &targetPos, &force };
                il2cpp_runtime_invoke(teleportMethod, player, args, nullptr);
            }
            else {
                const Il2CppMethod* getTrans = FindMethodRecursive(pClass, "get_transform");
                if (getTrans) {
                    void* enemyTrans = il2cpp_runtime_invoke(getTrans, player, nullptr, nullptr);
                    if (enemyTrans) {
                        Il2CppClass* tClass = il2cpp_object_get_class(enemyTrans);
                        const Il2CppMethod* setPos = FindMethodRecursive(tClass, "set_position");
                        if (setPos) {
                            void* posArgs[] = { &targetPos };
                            il2cpp_runtime_invoke(setPos, enemyTrans, posArgs, nullptr);
                        }
                    }
                }
            }
        }
        printf("[TP] Teleport Complete!\n");
    }
}

// ========================================
// Item Spawner Implementation
// Reference: ItemSpawner namespace from guide
// ========================================
namespace ItemSpawner
{
    // ========================================
    // Offsets from NewLuaTableOffsets.txt
    // ========================================
#define OFFSET_EFFECTS_CONTROLLER 0x18
#define OFFSET_LOCAL_PLAYER 0x108
#define OFFSET_INVENTORY_CONTROLLER 0x958
#define OFFSET_INVENTORY 0x100
#define OFFSET_EQUIPMENT 0x18
#define OFFSET_SLOTS 0x80
#define OFFSET_SLOT_BACKPACK 0x60
#define OFFSET_BP_CONTAINED_ITEM 0x48
#define OFFSET_BP_GRIDS 0x78
#define OFFSET_SEARCHABLE_GRID_0 0x20
#define OFFSET_STACK_OBJECTS_COUNT 0x24
#define OFFSET_SPAWNED_IN_SESSION 0x68

// ========================================
// Global Variables
// ========================================
    using InitLevelFn = void* (__fastcall*)(void* self, void* itemFactory, void* config, bool loadBundlesAndCreatePools, void* resources, void* progress, void* ct);
    static InitLevelFn g_OrigInitLevel = nullptr;
    static void* g_InitLevelTarget = nullptr;
    static bool g_InitLevelHooked = false;
    static void* g_InitLevelGateway = nullptr;
    static void* g_CapturedItemFactory = nullptr;
    static const Il2CppMethod* g_CreateItemMethod = nullptr;

    // ========================================
    // Forward Declarations
    // ========================================
    bool TryAddToEquipmentSlot(void* item, int slotId);
    bool ResolveCreateItemMethod();
    std::string GenerateMongoId();

    // ========================================
    // Helper Functions
    // ========================================

    // Hook handler for GameWorld.InitLevel
    static void* __fastcall InitLevel_Hook(void* self, void* itemFactory, void* config, bool loadBundlesAndCreatePools, void* resources, void* progress, void* ct)
    {
        if (itemFactory && !g_CapturedItemFactory)
        {
            g_CapturedItemFactory = itemFactory;
            printf("[Spawner] Captured ItemFactory via InitLevel arg @ %p\n", itemFactory);
        }
        return g_OrigInitLevel ? g_OrigInitLevel(self, itemFactory, config, loadBundlesAndCreatePools, resources, progress, ct) : nullptr;
    }

    // Hook a virtual method via vtable entry match
    bool HookVTable(void* instance, void* targetFn, void* detour, void** origOut)
    {
        if (!instance || !targetFn || !detour || !origOut) return false;
        void** vtable = *(void***)instance;
        if (!vtable) return false;

        for (int i = 0; i < 512; ++i)
        {
            if (vtable[i] == targetFn)
            {
                DWORD oldProtect;
                if (!VirtualProtect(&vtable[i], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
                    return false;
                *origOut = vtable[i];
                vtable[i] = detour;
                VirtualProtect(&vtable[i], sizeof(void*), oldProtect, &oldProtect);
                FlushInstructionCache(GetCurrentProcess(), &vtable[i], sizeof(void*));
                return true;
            }
        }
        return false;
    }

    // Simple 64-bit absolute jump writer
    void WriteAbsJump(void* src, void* dst)
    {
        uint8_t patch[12] = { 0x48, 0xB8 };
        memcpy(patch + 2, &dst, sizeof(void*));
        patch[10] = 0xFF;
        patch[11] = 0xE0;
        DWORD old;
        if (VirtualProtect(src, sizeof(patch), PAGE_EXECUTE_READWRITE, &old))
        {
            memcpy(src, patch, sizeof(patch));
            VirtualProtect(src, sizeof(patch), old, &old);
            FlushInstructionCache(GetCurrentProcess(), src, sizeof(patch));
        }
    }

    bool InstallDirectDetour(void* fn)
    {
        if (!fn || g_InitLevelGateway) return false;

        const size_t stolen = 16;
        g_InitLevelGateway = VirtualAlloc(nullptr, stolen + 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_InitLevelGateway) return false;

        memcpy(g_InitLevelGateway, fn, stolen);
        void* jumpBack = (uint8_t*)fn + stolen;
        WriteAbsJump((uint8_t*)g_InitLevelGateway + stolen, jumpBack);

        g_OrigInitLevel = (InitLevelFn)g_InitLevelGateway;
        WriteAbsJump(fn, (void*)&InitLevel_Hook);
        printf("[Spawner] Direct detour installed @ %p gateway=%p (stolen=%zu)\n", fn, g_InitLevelGateway, stolen);
        return true;
    }

    void TryHookInitLevel(Il2CppImage* image)
    {
        if (g_InitLevelHooked || g_InitLevelGateway)
            return;

        // Prefer direct detour on method code
        if (image && il2cpp_class_from_name && il2cpp_class_get_methods && il2cpp_method_get_name && il2cpp_method_get_param_count)
        {
            Il2CppClass* gwClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT", "GameWorld");
            const Il2CppMethod* targetMethod = nullptr;
            void* iter = nullptr;
            while (gwClass && (targetMethod = il2cpp_class_get_methods(gwClass, &iter)))
            {
                const char* nm = il2cpp_method_get_name(targetMethod);
                if (nm && strcmp(nm, "InitLevel") == 0 && il2cpp_method_get_param_count && il2cpp_method_get_param_count(targetMethod) == 6)
                    break;
                targetMethod = nullptr;
            }

            if (targetMethod)
            {
                void* fn = nullptr;
                int spins = 0;
                while (!(fn = *(void**)targetMethod) && spins < 50)
                {
                    Sleep(20);
                    spins++;
                }
                if (fn && InstallDirectDetour(fn))
                {
                    g_InitLevelTarget = fn;
                    g_InitLevelHooked = true;
                    return;
                }
                else
                {
                    printf("[-] Direct detour install failed for InitLevel\n");
                }
            }
        }

        // Fallback: vtable hook on live instance
        void* gwInstance = GetGameWorldInstance();
        if (!gwInstance) return;

        Il2CppClass* gwClass = il2cpp_object_get_class ? il2cpp_object_get_class(gwInstance) : nullptr;
        if (!gwClass && image && il2cpp_class_from_name)
            gwClass = (Il2CppClass*)il2cpp_class_from_name(image, "EFT", "GameWorld");
        if (!gwClass || !il2cpp_class_get_methods || !il2cpp_method_get_name || !il2cpp_method_get_param_count)
            return;

        const Il2CppMethod* targetMethod = nullptr;
        void* iter = nullptr;
        while (const Il2CppMethod* m = il2cpp_class_get_methods(gwClass, &iter))
        {
            const char* nm = il2cpp_method_get_name(m);
            if (nm && strcmp(nm, "InitLevel") == 0 && il2cpp_method_get_param_count && il2cpp_method_get_param_count(m) == 6)
            {
                targetMethod = m;
                break;
            }
        }
        if (!targetMethod) return;

        void* fn = nullptr;
        int spins = 0;
        while (!(fn = *(void**)targetMethod) && spins < 50)
        {
            Sleep(50);
            spins++;
        }
        if (!fn) return;

        if (HookVTable(gwInstance, fn, (void*)&InitLevel_Hook, (void**)&g_OrigInitLevel))
        {
            g_InitLevelTarget = fn;
            g_InitLevelHooked = true;
            printf("[Spawner] Hooked GameWorld.InitLevel via vtable @ %p\n", fn);
        }
    }

    std::mt19937& Rng()
    {
        static std::mt19937 rng(static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        return rng;
    }

    std::string GenerateMongoId()
    {
        static const char hex[] = "0123456789abcdef";
        std::uniform_int_distribution<int> dist(0, 15);
        std::string out;
        out.reserve(24);
        for (int i = 0; i < 24; ++i)
        {
            out.push_back(static_cast<char>(hex[dist(Rng())]));
        }
        return out;
    }

    bool IsValidTpl(const std::string& tpl)
    {
        if (tpl.size() != 24) return false;
        return std::all_of(tpl.begin(), tpl.end(), [](char c)
            {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            });
    }

    bool ResolveCreateItemMethod()
    {
        if (g_CreateItemMethod) return true;
        if (!g_CapturedItemFactory || !il2cpp_object_get_class) return false;

        Il2CppClass* cls = il2cpp_object_get_class(g_CapturedItemFactory);
        void* it = nullptr;
        while (const Il2CppMethod* m = il2cpp_class_get_methods(cls, &it))
        {
            const char* nm = il2cpp_method_get_name(m);
            if (!nm || strcmp(nm, "CreateItem")) continue;
            if (il2cpp_method_get_param_count && il2cpp_method_get_param_count(m) != 3) continue;
            if (il2cpp_method_get_flags)
            {
                uint32_t ifl = 0, fl = il2cpp_method_get_flags(m, &ifl);
                if (fl & 0x0010) continue; // skip static
            }
            int spins = 0;
            while (!*(void**)m && spins++ < 50) Sleep(20);
            if (!*(void**)m) continue;

            g_CreateItemMethod = m;
            printf("[Spawner] CreateItem (instance) @%p fn=%p\n", m, *(void**)m);
            return true;
        }
        return false;
    }

    bool TryAddToBackpack(void* item)
    {
        if (!item) return false;
        if (!g_CameraManagerInstance)
        {
            printf("[-] No CameraManager, cannot add to backpack\n");
            return false;
        }

        uintptr_t effectsController = *(uintptr_t*)((uintptr_t)g_CameraManagerInstance + OFFSET_EFFECTS_CONTROLLER);
        uintptr_t localPlayer = effectsController ? *(uintptr_t*)(effectsController + OFFSET_LOCAL_PLAYER) : 0;
        uintptr_t invController = localPlayer ? *(uintptr_t*)(localPlayer + OFFSET_INVENTORY_CONTROLLER) : 0;
        uintptr_t inventory = invController ? *(uintptr_t*)(invController + OFFSET_INVENTORY) : 0;
        uintptr_t equipment = inventory ? *(uintptr_t*)(inventory + OFFSET_EQUIPMENT) : 0;
        uintptr_t slots = equipment ? *(uintptr_t*)(equipment + OFFSET_SLOTS) : 0;
        uintptr_t backpackSlot = slots ? *(uintptr_t*)(slots + OFFSET_SLOT_BACKPACK) : 0;

        if (!backpackSlot)
        {
            printf("[-] Backpack slot not found\n");
            return false;
        }

        uintptr_t backpackItem = *(uintptr_t*)((uintptr_t)backpackSlot + OFFSET_BP_CONTAINED_ITEM);
        if (!backpackItem)
        {
            printf("[Spawner] No backpack found, spawning raid backpack...\n");

            // Spawn raid backpack
            if (!g_CapturedItemFactory || !ResolveCreateItemMethod())
            {
                printf("[-] Cannot create backpack - ItemFactory not ready\n");
                return false;
            }

            std::string backpackTpl = "5df8a4d786f77412672a1e3b"; // Raid backpack
            std::string mongo = GenerateMongoId();
            Il2CppObject* mongoStr = il2cpp_string_new(mongo.c_str());
            Il2CppObject* tplStr = il2cpp_string_new(backpackTpl.c_str());
            void* argsCreate[] = { mongoStr, tplStr, nullptr };
            void* exc = nullptr;
            void* backpack = il2cpp_runtime_invoke(g_CreateItemMethod, g_CapturedItemFactory, argsCreate, &exc);

            if (exc || !backpack)
            {
                printf("[-] Failed to create backpack\n");
                return false;
            }

            *(int32_t*)((uintptr_t)backpack + OFFSET_STACK_OBJECTS_COUNT) = 1;
            *(uint8_t*)((uintptr_t)backpack + OFFSET_SPAWNED_IN_SESSION) = 1;

            // Add backpack to backpack slot (slot ID 4)
            if (!TryAddToEquipmentSlot(backpack, 4))
            {
                printf("[-] Failed to equip backpack\n");
                return false;
            }

            printf("[+] Raid backpack spawned and equipped\n");

            // Re-fetch backpack item pointer after equipping
            backpackItem = *(uintptr_t*)((uintptr_t)backpackSlot + OFFSET_BP_CONTAINED_ITEM);
            if (!backpackItem)
            {
                printf("[-] Backpack still null after spawn\n");
                return false;
            }
        }

        uintptr_t grids = *(uintptr_t*)(backpackItem + OFFSET_BP_GRIDS);
        uintptr_t grid0 = grids ? *(uintptr_t*)(grids + OFFSET_SEARCHABLE_GRID_0) : 0;
        if (!grid0)
        {
            printf("[-] Grid0 not found, cannot AddAnywhere\n");
            return false;
        }

        const Il2CppMethod* addAnywhere = FindMethodRecursive(il2cpp_object_get_class((void*)grid0), "AddAnywhere");
        if (!addAnywhere)
        {
            printf("[-] AddAnywhere not found on Grid\n");
            return false;
        }

        uint32_t pc = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(addAnywhere) : 0;
        if (pc != 2)
        {
            printf("[-] AddAnywhere param count %u not supported (expected 2)\n", pc);
            return false;
        }

        int32_t errorHandling = 0;
        void* args[] = { item, &errorHandling };
        void* exc = nullptr;
        void* ret = il2cpp_runtime_invoke(addAnywhere, (void*)grid0, args, &exc);
        if (exc)
        {
            printf("[-] AddAnywhere threw exception %p\n", exc);
            return false;
        }

        bool ok = true;
        if (ret && il2cpp_object_unbox)
            ok = *(bool*)il2cpp_object_unbox(ret);
        else if (!ret)
            ok = false;

        if (ok)
        {
            *(int32_t*)((uintptr_t)item + OFFSET_STACK_OBJECTS_COUNT) = 1;
            *(uint8_t*)((uintptr_t)item + OFFSET_SPAWNED_IN_SESSION) = 1;
            printf("[+] Item added via Grid.AddAnywhere\n");
            return true;
        }

        printf("[-] AddAnywhere returned false\n");
        return false;
    }

    bool TryAddToEquipmentSlot(void* item, int slotId)
    {
        if (!item) return false;
        if (!g_CameraManagerInstance) return false;

        uintptr_t effectsController = *(uintptr_t*)((uintptr_t)g_CameraManagerInstance + OFFSET_EFFECTS_CONTROLLER);
        uintptr_t localPlayer = effectsController ? *(uintptr_t*)(effectsController + OFFSET_LOCAL_PLAYER) : 0;
        uintptr_t invController = localPlayer ? *(uintptr_t*)(localPlayer + OFFSET_INVENTORY_CONTROLLER) : 0;
        uintptr_t inventory = invController ? *(uintptr_t*)(invController + OFFSET_INVENTORY) : 0;
        uintptr_t equipment = inventory ? *(uintptr_t*)(inventory + OFFSET_EQUIPMENT) : 0;
        if (!equipment) return false;

        // Resolve Equipment.GetSlot(EquipmentSlot)
        Il2CppClass* eqClass = il2cpp_object_get_class ? il2cpp_object_get_class((void*)equipment) : nullptr;
        if (!eqClass || !il2cpp_class_get_methods || !il2cpp_method_get_name) return false;

        const Il2CppMethod* getSlot = nullptr;
        void* iter = nullptr;
        while (const Il2CppMethod* m = il2cpp_class_get_methods(eqClass, &iter))
        {
            const char* nm = il2cpp_method_get_name(m);
            if (nm && strcmp(nm, "GetSlot") == 0)
            {
                uint32_t pc = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(m) : 0;
                if (pc == 1)
                {
                    getSlot = m;
                    break;
                }
            }
        }
        if (!getSlot) return false;

        int32_t slotEnum = slotId;
        void* argsSlot[] = { &slotEnum };
        void* exc = nullptr;
        void* slotObj = il2cpp_runtime_invoke(getSlot, (void*)equipment, argsSlot, &exc);
        if (exc || !slotObj) return false;

        Il2CppClass* slotClass = il2cpp_object_get_class ? il2cpp_object_get_class(slotObj) : nullptr;
        if (!slotClass || !il2cpp_class_get_methods || !il2cpp_method_get_name) return false;

        // Check if slot already has an item - don't spawn if occupied
        auto findMethodByName = [&](const char* name) -> const Il2CppMethod*
            {
                void* it2 = nullptr;
                while (const Il2CppMethod* m = il2cpp_class_get_methods(slotClass, &it2))
                {
                    const char* nm = il2cpp_method_get_name(m);
                    if (nm && strcmp(nm, name) == 0)
                        return m;
                }
                return nullptr;
            };

        const Il2CppMethod* getContained = findMethodByName("get_ContainedItem");
        if (getContained)
        {
            void* exc = nullptr;
            void* existing = il2cpp_runtime_invoke(getContained, slotObj, nullptr, &exc);
            if (!exc && existing)
            {
                printf("[-] Slot already occupied, cannot spawn item\n");
                return false;
            }
        }

        const Il2CppMethod* addMethod = nullptr;
        iter = nullptr;
        while (const Il2CppMethod* m = il2cpp_class_get_methods(slotClass, &iter))
        {
            const char* nm = il2cpp_method_get_name(m);
            if (nm && strcmp(nm, "Add") == 0)
            {
                uint32_t pc = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(m) : 0;
                if (pc == 3 || pc == 2)
                {
                    addMethod = m;
                    break;
                }
            }
        }
        if (!addMethod) return false;

        auto tryAdd = [&]() -> bool
            {
                uint32_t pcAdd = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(addMethod) : 0;
                void* excAdd = nullptr;
                void* ret = nullptr;
                if (pcAdd == 3)
                {
                    bool simulate = false;
                    bool check = false;
                    void* argsAdd[] = { item, &simulate, &check };
                    ret = il2cpp_runtime_invoke(addMethod, slotObj, argsAdd, &excAdd);
                }
                else if (pcAdd == 2)
                {
                    bool simulate = false;
                    void* argsAdd[] = { item, &simulate };
                    ret = il2cpp_runtime_invoke(addMethod, slotObj, argsAdd, &excAdd);
                }
                else
                {
                    return false;
                }

                if (excAdd) return false;
                bool ok = true;
                if (ret && il2cpp_object_unbox)
                    ok = *(bool*)il2cpp_object_unbox(ret);
                else if (!ret)
                    ok = false;
                return ok;
            };

        if (tryAdd())
        {
            *(int32_t*)((uintptr_t)item + OFFSET_STACK_OBJECTS_COUNT) = 1;
            *(uint8_t*)((uintptr_t)item + OFFSET_SPAWNED_IN_SESSION) = 1;
            return true;
        }

        // Fallback: remove existing item then add again
        const Il2CppMethod* getContained2 = findMethodByName("get_ContainedItem");
        void* contained = nullptr;
        if (getContained2)
        {
            void* exc = nullptr;
            contained = il2cpp_runtime_invoke(getContained2, slotObj, nullptr, &exc);
            if (exc) contained = nullptr;
        }

        if (contained)
        {
            const Il2CppMethod* rem = findMethodByName("RemoveWithoutRestrictions");
            if (!rem) rem = findMethodByName("Remove");
            if (rem)
            {
                uint32_t pcRem = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(rem) : 0;
                void* excRem = nullptr;
                if (pcRem == 1)
                {
                    void* argsR[] = { contained };
                    il2cpp_runtime_invoke(rem, slotObj, argsR, &excRem);
                }
                else if (pcRem == 2)
                {
                    bool simulate = false;
                    void* argsR[] = { contained, &simulate };
                    il2cpp_runtime_invoke(rem, slotObj, argsR, &excRem);
                }
                if (excRem) contained = nullptr;
            }
        }

        if (tryAdd())
        {
            *(int32_t*)((uintptr_t)item + OFFSET_STACK_OBJECTS_COUNT) = 1;
            *(uint8_t*)((uintptr_t)item + OFFSET_SPAWNED_IN_SESSION) = 1;
            return true;
        }

        // Last resort: set_ContainedItem directly
        const Il2CppMethod* setContained = findMethodByName("set_ContainedItem");
        if (setContained && il2cpp_method_get_param_count && il2cpp_method_get_param_count(setContained) == 1)
        {
            void* excSet = nullptr;
            void* argsSet[] = { item };
            il2cpp_runtime_invoke(setContained, slotObj, argsSet, &excSet);
            if (!excSet)
            {
                *(int32_t*)((uintptr_t)item + OFFSET_STACK_OBJECTS_COUNT) = 1;
                *(uint8_t*)((uintptr_t)item + OFFSET_SPAWNED_IN_SESSION) = 1;
                return true;
            }
        }

        return false;
    }

    // ========================================
    // Public API Functions
    // ========================================

    void ProcessQueue(Il2CppImage* image)
    {
        TryHookInitLevel(image);
    }

    void RequestSpawn(const std::string& tplId, int count)
    {
        if (!IsValidTpl(tplId))
        {
            printf("[-] Invalid template id: %s\n", tplId.c_str());
            return;
        }

        if (!g_CapturedItemFactory)
        {
            printf("[-] ItemFactory instance not captured yet (InitLevel not called?)\n");
            return;
        }

        if (!ResolveCreateItemMethod())
        {
            printf("[-] CreateItem method not found\n");
            return;
        }

        if (!il2cpp_string_new || !il2cpp_runtime_invoke)
        {
            printf("[-] IL2CPP API missing for spawn\n");
            return;
        }

        for (int i = 0; i < count; ++i)
        {
            printf("[Spawner] invoking CreateItem this=%p fn=%p\n", g_CapturedItemFactory, *(void**)g_CreateItemMethod);
            std::string mongo = GenerateMongoId();
            Il2CppObject* mongoStr = il2cpp_string_new(mongo.c_str());
            Il2CppObject* tplStr = il2cpp_string_new(tplId.c_str());
            void* args[] = { mongoStr, tplStr, nullptr };
            void* exc = nullptr;
            void* res = il2cpp_runtime_invoke(g_CreateItemMethod, g_CapturedItemFactory, args, &exc);
            if (exc || !res)
            {
                printf("[-] CreateItem failed tpl=%s exc=%p res=%p\n", tplId.c_str(), exc, res);
                continue;
            }

            printf("[+] Spawned item tpl=%s res=%p\n", tplId.c_str(), res);
            *(int32_t*)((uintptr_t)res + OFFSET_STACK_OBJECTS_COUNT) = 1;
            *(uint8_t*)((uintptr_t)res + OFFSET_SPAWNED_IN_SESSION) = 1;
            TryAddToBackpack(res);
        }
    }

    void RequestSpawnToSlot(const std::string& tplId, int slotId)
    {
        if (!IsValidTpl(tplId))
        {
            printf("[-] Invalid template id: %s\n", tplId.c_str());
            return;
        }

        if (!g_CapturedItemFactory)
        {
            printf("[-] ItemFactory instance not captured yet (InitLevel not called?)\n");
            return;
        }

        if (!ResolveCreateItemMethod())
        {
            printf("[-] CreateItem method not found\n");
            return;
        }

        if (!il2cpp_string_new || !il2cpp_runtime_invoke)
        {
            printf("[-] IL2CPP API missing for spawn\n");
            return;
        }

        printf("[Spawner] invoking CreateItem this=%p fn=%p\n", g_CapturedItemFactory, *(void**)g_CreateItemMethod);
        std::string mongo = GenerateMongoId();
        Il2CppObject* mongoStr = il2cpp_string_new(mongo.c_str());
        Il2CppObject* tplStr = il2cpp_string_new(tplId.c_str());
        void* args[] = { mongoStr, tplStr, nullptr };
        void* exc = nullptr;
        void* res = il2cpp_runtime_invoke(g_CreateItemMethod, g_CapturedItemFactory, args, &exc);
        if (exc || !res)
        {
            printf("[-] CreateItem failed tpl=%s exc=%p res=%p\n", tplId.c_str(), exc, res);
            return;
        }

        printf("[+] Spawned item tpl=%s res=%p\n", tplId.c_str(), res);
        *(int32_t*)((uintptr_t)res + OFFSET_STACK_OBJECTS_COUNT) = 1;
        *(uint8_t*)((uintptr_t)res + OFFSET_SPAWNED_IN_SESSION) = 1;
        TryAddToEquipmentSlot(res, slotId);
    }
}