#include "Features.h"
#include <cstdio>
#include <cstring>

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