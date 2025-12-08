#include "MenuRenderer.h"
#include "Features.h"
#include "imgui.h"

namespace MenuRenderer
{
    void RenderHintText()
    {
        // Always show hint in top-right corner
        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 180, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(170, 50), ImGuiCond_Always);

        ImGui::Begin("##Hint", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground);

        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.7f), "INSERT - Toggle Menu");
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.7f), "END - Exit Game");

        ImGui::End();
    }

    void RenderPlayerTab()
    {
        ImGui::BeginChild("PlayerTab", ImVec2(0, 0), false);

        // God Mode
        if (ImGui::Checkbox("God Mode", &Features::god_mode))
        {
            printf("[TOGGLE] God Mode: %s\n", Features::god_mode ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("No DMG, No Fall DMG, No Environment DMG, No Thirst, No Hunger, No Fatigue.");
        }

        // Infinite Stamina
        if (ImGui::Checkbox("Infinite Stamina", &Features::infinite_stamina))
        {
            printf("[TOGGLE] Infinite Stamina: %s\n", Features::infinite_stamina ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Crackhead energy, limitless.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Breach All Doors
        if (ImGui::Checkbox("Breach All Doors", &Features::breach_all_doors))
        {
            printf("[TOGGLE] Breach All Doors: %s\n", Features::breach_all_doors ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Kick down all doors like they do in the FBI.");
        }

        ImGui::EndChild();
    }

    void RenderCombatTab()
    {
        ImGui::BeginChild("CombatTab", ImVec2(0, 0), false);

        // No Recoil
        if (ImGui::Checkbox("No Recoil", &Features::no_recoil))
        {
            printf("[TOGGLE] No Recoil: %s\n", Features::no_recoil ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pretty self explanatory.");
        }

        // No Weapon Durability
        if (ImGui::Checkbox("No Weapon Durability", &Features::no_weapon_durability))
        {
            printf("[TOGGLE] No Weapon Durability: %s\n", Features::no_weapon_durability ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pretty self explanatory.");
        }

        // No Weapon Malfunction
        if (ImGui::Checkbox("No Weapon Malfunction", &Features::no_weapon_malfunction))
        {
            printf("[TOGGLE] No Weapon Malfunction: %s\n", Features::no_weapon_malfunction ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pretty self explanatory.");
        }

        // No Weapon Overheating
        if (ImGui::Checkbox("No Weapon Overheating", &Features::no_weapon_overheating))
        {
            printf("[TOGGLE] No Weapon Overheating: %s\n", Features::no_weapon_overheating ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Ice ice baby.");
        }

        ImGui::EndChild();
    }

    void RenderAITab()
    {
        ImGui::BeginChild("AITab", ImVec2(0, 0), false);

        // AI Not Attack
        if (ImGui::Checkbox("AI Ignore", &Features::ai_not_attack))
        {
            printf("[TOGGLE] AI Ignore: %s\n", Features::ai_not_attack ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("AI cannot see or attack you.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Teleport AI to Me button
        if (ImGui::Button("Teleport AI"))
        {
            FeaturePatch::TeleportAllEnemiesToMe();
            printf("[ACTION] Teleport AI triggered\n");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Teleport all AI to your location.");
        }

        ImGui::EndChild();
    }

    void RenderVisualTab()
    {
        ImGui::BeginChild("VisualTab", ImVec2(0, 0), false);

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Visual Features (Coming Soon)");
        ImGui::Spacing();

        // Planned features (disabled)
        ImGui::BeginDisabled();

        ImGui::Checkbox("Thermal Vision", &Features::thermal_vision);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pretty self explanatory.");
        }

        ImGui::Checkbox("Night Vision", &Features::night_vision);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pretty self explanatory.");
        }

        ImGui::Checkbox("Player ESP", &Features::player_esp);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("See PMCs, Scavs, Bosses through walls.");
        }

        ImGui::Checkbox("Loot ESP", &Features::loot_esp);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("See valuable loot through walls");
        }

        if (Features::loot_esp)
        {
            ImGui::Indent();
            ImGui::SliderFloat("Min Item Value", &Features::loot_esp_min_value, 1000.0f, 100000.0f, "%.0f");
            ImGui::Unindent();
        }

        ImGui::EndDisabled();

        ImGui::EndChild();
    }

    void RenderLootTab()
    {
        ImGui::BeginChild("LootTab", ImVec2(0, 0), false);

        // Lucky Search (moved from Player tab)
        if (ImGui::Checkbox("Lucky Search", &Features::lucky_search))
        {
            printf("[TOGGLE] Lucky Search: %s\n", Features::lucky_search ? "ON" : "OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Instantly search whatever you open.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Additional Features (Coming Soon)");
        ImGui::Spacing();

        // Planned features (disabled)
        ImGui::BeginDisabled();

        ImGui::Checkbox("Item Spawner", &Features::item_spawner);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Spawn items. (Coming Soon)");
        }

        ImGui::Checkbox("Loot Vacuum", &Features::loot_vacuum);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Teleport nearby loot to your position. (Coming Soon)");
        }

        ImGui::EndDisabled();

        ImGui::EndChild();
    }

    void RenderMainMenu()
    {
        // Only render if menu is open
        if (!Features::menu_open)
            return;

        // Set window size and position
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

        // Main window
        if (ImGui::Begin("Tegridy Farms", &Features::menu_open, ImGuiWindowFlags_NoCollapse))
        {
            // Tab bar
            if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("PLAYER"))
                {
                    RenderPlayerTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("COMBAT"))
                {
                    RenderCombatTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("AI"))
                {
                    RenderAITab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("VISUAL"))
                {
                    RenderVisualTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("LOOT"))
                {
                    RenderLootTab();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
}