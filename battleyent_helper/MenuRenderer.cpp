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

        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Tegridy Farms");
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
            ImGui::SetTooltip("Invincibility - no damage taken");
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
            ImGui::SetTooltip("Never run out of stamina");
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
            ImGui::SetTooltip("Open any door without keys");
        }

        // Lucky Search
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
            ImGui::SetTooltip("Eliminate weapon recoil");
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
            ImGui::SetTooltip("Weapons never lose durability");
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
            ImGui::SetTooltip("Weapons never jam");
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
            ImGui::SetTooltip("Weapons never overheat");
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
            ImGui::SetTooltip("AI cannot see or attack you");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Additional AI features coming soon...");

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

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
}