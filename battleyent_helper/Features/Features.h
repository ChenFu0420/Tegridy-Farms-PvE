#pragma once
#include <Windows.h>
#include <vector>
#include <string>

// Forward declarations
struct Il2CppImage;
struct Il2CppMethod;
struct Il2CppClass;

// ========================================
// Feature Toggle States
// These will be modified by ImGui checkboxes
// ========================================
namespace Features
{
    // Toggleable features
    extern bool god_mode;
    extern bool infinite_stamina;
    extern bool no_recoil;
    extern bool no_weapon_durability;
    extern bool no_weapon_malfunction;
    extern bool no_weapon_overheating;
    extern bool ai_not_attack;
    extern bool breach_all_doors;
    extern bool lucky_search;

    // Planned features (placeholders)
    extern bool thermal_vision;
    extern bool night_vision;
    extern bool player_esp;
    extern bool loot_esp;
    extern float loot_esp_min_value;
    extern bool item_spawner;
    extern bool loot_vacuum;

    // Menu state
    extern bool menu_open;
}

// ========================================
// Recoverable Patch System
// Reference: Truth source patch_method_ret_recoverable/unpatch_method_ret
// ========================================
namespace PatchSystem
{
    struct PatchData
    {
        const Il2CppMethod* method = nullptr;
        void* functionPtr = nullptr;
        unsigned char originalBytes[16] = {};
        bool isPatched = false;
        std::string name;
    };

    // Storage for all patches
    extern std::vector<PatchData> g_patches;

    // Create a recoverable patch (saves original bytes, doesn't apply yet)
    bool CreatePatch(const char* name, const Il2CppMethod* method);

    // Enable patch (write 0xC3)
    bool EnablePatch(const char* name);

    // Disable patch (restore original bytes)
    bool DisablePatch(const char* name);

    // Check if patch is currently enabled
    bool IsPatched(const char* name);

    // Find patch by name
    PatchData* FindPatch(const char* name);
}

// ========================================
// Feature Patch Functions
// Each function checks Features:: toggle and enables/disables accordingly
// ========================================
namespace FeaturePatch
{
    // Initialize all patches (create them, don't enable yet)
    void InitializeAllPatches(Il2CppImage* image);

    // Apply/unapply based on current toggle states
    void ApplyGodMode(Il2CppImage* image);
    void ApplyInfiniteStamina(Il2CppImage* image);
    void ApplyNoRecoil(Il2CppImage* image);
    void ApplyNoWeaponDurability(Il2CppImage* image);
    void ApplyNoWeaponMalfunction(Il2CppImage* image);
    void ApplyNoWeaponOverheating(Il2CppImage* image);
    void ApplyAINotAttack(Il2CppImage* image);
    void ApplyBreachAllDoors(Il2CppImage* image);
    void ApplyLuckySearch(Il2CppImage* image);

    // Master function - checks all toggles and applies changes
    void ApplyAllEnabledFeatures(Il2CppImage* image);

    // Continuous updates (run every frame)
    void UpdateGodMode();  // God mode uses continuous memory write, not patch
    void UpdateOfflinePVE();  // Force offline PVE raid

    // AI Vacuum
    void TeleportAllEnemiesToMe();
}

// ========================================
// Item Spawner System
// Reference: ItemSpawner namespace from guide
// ========================================
namespace ItemSpawner
{
    // Initialize InitLevel hook (call during startup after IL2CPP init)
    void ProcessQueue(Il2CppImage* image);

    // Spawn item to backpack
    void RequestSpawn(const std::string& tplId, int count);

    // Spawn item to equipment slot
    void RequestSpawnToSlot(const std::string& tplId, int slotId);
}