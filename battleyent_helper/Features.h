#pragma once
#include "il2api.h"
#include <vector>
#include <string>
#include <unordered_map>

// ========================================
// Recoverable Patch System
// Reference: RecoverablePatchesAiTeleAndAdvapiSource.txt
// ========================================
namespace PatchSystem
{
    struct PatchBackup {
        void* address;
        std::vector<uint8_t> originalBytes;
        std::vector<uint8_t> patchedBytes;
        bool isPatched;
        std::string featureName;
    };

    extern std::unordered_map<std::string, PatchBackup> g_patchBackups;

    // Core patch functions
    void CreatePatch(const char* name, void* address, const std::vector<uint8_t>& patchBytes);
    void EnablePatch(const char* name);
    void DisablePatch(const char* name);
    bool IsPatched(const char* name);

    // Convenience functions for common patches
    void CreateRETPatch(const char* name, const Il2CppMethod* method);
    void CreateBoolGetterPatch(const char* name, void* methodPtr, bool returnValue);
    void CreateFloatGetterPatch(const char* name, void* methodPtr, float returnValue);
}

// ========================================
// Feature Patch Functions
// ========================================
namespace FeaturePatch
{
    // ========================================
    // Player Features
    // ========================================
    void ApplyGodMode(Il2CppImage* image);
    void ApplyInfiniteStamina(Il2CppImage* image);
    void ApplyNoFallDamage(Il2CppImage* image);
    void ApplyNoEnergyDrain(Il2CppImage* image);
    void ApplyNoHydrationDrain(Il2CppImage* image);
    void ApplyNoFatigue(Il2CppImage* image);
    void ApplyNoEnvironmentDamage(Il2CppImage* image); // Combined: barbed wire + mines + sniper zones
    void ApplyBreachAllDoors(Il2CppImage* image);
    void ApplyInstantSearch(Il2CppImage* image);
    void ApplyAIIgnore(Il2CppImage* image); // Combined: AI tracking + AI attacks

    // ========================================
    // Combat Features (kept for backwards compat, not used in new menu)
    // ========================================
    void ApplyNoRecoil(Il2CppImage* image);
    void ApplyNoWeaponDurability(Il2CppImage* image);
    void ApplyNoWeaponMalfunction(Il2CppImage* image);
    void ApplyNoWeaponOverheating(Il2CppImage* image);

    // ========================================
    // Master Function - Apply All Enabled Features
    // ========================================
    void ApplyAllEnabledFeatures(Il2CppImage* image);

    // ========================================
    // Continuous Update Functions (run in main loop)
    // ========================================
    void UpdateRaidSelectedLocation(Il2CppImage* image);
    void UpdateGodMode(Il2CppImage* image);
    void UpdateThermalNVG(); // Reference: ThermalNvgSource.txt
}

// ========================================
// Visual Features
// Reference: ThermalNvgSource.txt, PlayerESP.cs
// ========================================
namespace ThermalVision
{
    void UpdateThermalNVG();
}

namespace ESPFeatures
{
    struct Vector3 {
        float x, y, z;
    };

    struct ESPPlayer {
        void* playerPtr;
        Vector3 worldPos;
        Vector3 headWorldPos;
        Vector3 screenPos;
        Vector3 headScreenPos;
        std::string name;
        float distance;
        bool isAI;
        bool isBoss;
        bool isOnScreen;
    };

    extern std::vector<ESPPlayer> espPlayers;

    void UpdatePlayerESP(Il2CppImage* image);
    void RenderPlayerESP(); // Called from Overlay.cpp

    // Helper functions
    Vector3 WorldToScreen(Vector3 worldPos);
    bool IsScreenPointVisible(Vector3 screenPoint);
    Vector3 GetPlayerPosition(void* player);
    Vector3 GetPlayerHeadPosition(void* player);
    bool IsPlayerAI(void* player);
    bool IsPlayerBoss(void* player);
    std::string GetPlayerName(void* player);
}

// ========================================
// Loot Features
// Reference: Main.cs, LuaCheatEngineTable.txt, NewLuaOffsets.txt
// ========================================
namespace ItemDuping
{
    struct BackpackItem {
        void* itemPtr;
        std::string name;
        uintptr_t stackAddress;
        int currentStack;
    };

    extern std::vector<BackpackItem> scannedItems;
    extern int selectedIndex;

    void ScanBackpack();
    void DupeSelectedItem();
    void SetStackAmount(int amount);
    
    // Helper
    void ReadIL2CPPString(uintptr_t stringPtr, char* buffer, size_t maxLen);
}

// ========================================
// AI Features
// Reference: RecoverablePatchesAiTeleAndAdvapiSource.txt, NewLuaOffsets.txt
// ========================================
namespace AIFeatures
{
    struct WaveData {
        int spawnID;
        int countMin;
        int countMax;
        int timeMin;
        int timeMax;
    };

    extern WaveData waveEditor[8];

    void TeleportAllAI();
    void ApplyWaveEdits();
}

// ========================================
// Patch Helper Functions (legacy support)
// ========================================
namespace PatchHelpers
{
    void PatchMethodsWithLogging(Il2CppClass* klass, const char* namespaceName, const char* className, const std::vector<std::string>& methodNames);
    void PatchBoolGetter(void* methodPtr, bool returnValue);
    void PatchFloatGetter(void* methodPtr, float returnValue);
}
