#pragma once
#include <Windows.h>
#include <cstdint>

// Forward declarations
struct Il2CppImage;
struct Il2CppClass;
struct Il2CppMethod;

// ========================================
// Location Unlock System
// Permanent patches for unlocking all maps
// ========================================
namespace LocationUnlock
{
    // Apply all location unlock patches (permanent, non-toggleable)
    void ApplyUnlockLocations(Il2CppImage* image);

    // Update selected location fields when it changes
    void UpdateRaidSelectedLocation(Il2CppImage* image);

    // Force unlock location fields by direct memory write
    void ForceUnlockLocationFields(void* location);

    // Runtime update to unlock routes/exits (call every frame)
    void UpdateLocationRoutes(Il2CppImage* image);
}

// Global tracking for selected location
extern uintptr_t g_selectedLocation;

// Offsets for location structures
#define OFFSET_RAID_SETTINGS 0xD0
#define OFFSET_SELECTED_LOCATION 0xA8
#define OFFSET_FORCE_ONLINE_RAID 0x32
#define OFFSET_FORCE_OFFLINE_RAID 0x33