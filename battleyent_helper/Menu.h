#pragma once
#include <Windows.h>
#include <vector>
#include <string>

// ========================================
// Feature Toggle States - NEW 4-TAB STRUCTURE
// [PLAYER] [VISUAL] [LOOT] [AI]
// ========================================
namespace Features
{
    // ========================================
    // PLAYER TAB
    // ========================================
    extern bool god_mode;
    extern bool infinite_stamina;
    extern bool no_fall_damage;
    extern bool no_energy_drain;
    extern bool no_hydration_drain;
    extern bool no_fatigue;
    extern bool no_environment_damage;    // Combined: barbed wire + mines + sniper zones
    extern bool breach_all_doors;
    extern bool instant_search;
    extern bool ai_ignore;                // Combined: AI tracking + AI attacks

    // ========================================
    // VISUAL TAB
    // ========================================
    extern bool night_vision;             // Reference: ThermalNvgSource.txt
    extern bool thermal_vision;           // Reference: ThermalNvgSource.txt
    extern bool ai_esp;                   // Reference: PlayerESP.cs
    extern bool item_esp;                 // Not implemented
    extern bool extract_esp;              // Not implemented
    extern char item_name_filter[256];    // Filter for item ESP

    // ========================================
    // LOOT TAB
    // ========================================
    // Item duping state is in ItemDuping namespace

    // ========================================
    // AI TAB
    // ========================================
    extern bool wave_spawn_enabled;

    // ========================================
    // LEGACY FEATURES (kept for backwards compatibility, not in new menu)
    // ========================================
    extern bool no_recoil;
    extern bool no_weapon_durability;
    extern bool no_weapon_malfunction;
    extern bool no_weapon_overheating;
    extern bool instant_ads;
    extern bool disable_ai_tracking;      // Part of ai_ignore
    extern bool disable_ai_attacks;       // Part of ai_ignore
    extern bool no_barbed_wire_damage;    // Part of no_environment_damage
    extern bool no_minefield_damage;      // Part of no_environment_damage
    extern bool no_sniper_zones;          // Part of no_environment_damage
    extern bool lucky_search;             // Renamed to instant_search

    // ========================================
    // Menu State
    // ========================================
    extern bool show_menu;
    extern bool menu_open;
}

// ========================================
// Menu Control Functions (No rendering, just state)
// ========================================
namespace Menu
{
    // Toggle menu visibility
    void Toggle();

    // Check if menu key was pressed
    void CheckInput();
}
