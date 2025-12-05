#include "MenuRenderer.h"
#include "Menu.h"
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
        ImGui::Checkbox("God Mode", &Features::god_mode);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Invincibility - no damage taken");
        }

        // Infinite Stamina
        ImGui::Checkbox("Infinite Stamina", &Features::infinite_stamina);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Never run out of stamina");
        }

        // No Fall Damage
        ImGui::Checkbox("No Fall Damage", &Features::no_fall_damage);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Survive any fall height");
        }

        // No Energy Drain
        ImGui::Checkbox("No Energy Drain", &Features::no_energy_drain);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Food meter never depletes");
        }

        // No Hydration Drain
        ImGui::Checkbox("No Hydration Drain", &Features::no_hydration_drain);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Water meter never depletes");
        }

        // No Fatigue
        ImGui::Checkbox("No Fatigue", &Features::no_fatigue);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Never get overweight/encumbered");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // No Environment Damage (Combined feature)
        ImGui::Checkbox("No Environment Damage", &Features::no_environment_damage);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("No damage from barbed wire, mines, or sniper zones");
        }

        // AI Ignore (Combined feature)
        ImGui::Checkbox("AI Ignore", &Features::ai_ignore);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("AI cannot see or attack you");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Breach All Doors
        ImGui::Checkbox("Breach All Doors", &Features::breach_all_doors);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Open any door without keys");
        }

        // Instant Search
        ImGui::Checkbox("Instant Search", &Features::instant_search);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Search containers instantly");
        }

        ImGui::EndChild();
    }

    void RenderVisualTab()
    {
        ImGui::BeginChild("VisualTab", ImVec2(0, 0), false);

        // Night Vision
        ImGui::Checkbox("Night Vision", &Features::night_vision);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Enable night vision mode");
        }

        // Thermal Vision
        ImGui::Checkbox("Thermal Vision", &Features::thermal_vision);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Enable clean thermal vision (no noise/blur)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // AI ESP
        ImGui::Checkbox("AI ESP", &Features::ai_esp);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show AI players with colored boxes");
        }

        // Item ESP (not implemented)
        ImGui::BeginDisabled();
        ImGui::Checkbox("Item ESP", &Features::item_esp);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Not implemented)");

        // Extract ESP (not implemented)
        ImGui::BeginDisabled();
        ImGui::Checkbox("Extract ESP", &Features::extract_esp);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Not implemented)");

        // Item Name Filter (for future item ESP)
        ImGui::Spacing();
        ImGui::Text("Item Name Filter:");
        ImGui::InputText("##ItemFilter", Features::item_name_filter, sizeof(Features::item_name_filter));

        ImGui::EndChild();
    }

    void RenderLootTab()
    {
        ImGui::BeginChild("LootTab", ImVec2(0, 0), false);

        ImGui::Text("Item Duping");
        ImGui::Spacing();

        // Scan Backpack Button
        if (ImGui::Button("Scan Backpack", ImVec2(200, 30)))
        {
            ItemDuping::ScanBackpack();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Scan your backpack for items to duplicate");
        }

        ImGui::Spacing();

        // Item Selection Dropdown
        if (!ItemDuping::scannedItems.empty())
        {
            std::string preview = "Select Item...";
            if (ItemDuping::selectedIndex >= 0 && 
                ItemDuping::selectedIndex < (int)ItemDuping::scannedItems.size())
            {
                preview = ItemDuping::scannedItems[ItemDuping::selectedIndex].name;
            }

            if (ImGui::BeginCombo("##ItemSelect", preview.c_str()))
            {
                for (int i = 0; i < (int)ItemDuping::scannedItems.size(); i++)
                {
                    bool isSelected = (ItemDuping::selectedIndex == i);
                    std::string label = ItemDuping::scannedItems[i].name + 
                                       " (Stack: " + 
                                       std::to_string(ItemDuping::scannedItems[i].currentStack) + ")";
                    
                    if (ImGui::Selectable(label.c_str(), isSelected))
                    {
                        ItemDuping::selectedIndex = i;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();

            // Dupe Selected Item Button
            if (ImGui::Button("Dupe Selected Item", ImVec2(200, 30)))
            {
                ItemDuping::DupeSelectedItem();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Duplicate the selected item (sets stack to 2)");
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // Custom Stack Amount
            static int customStack = 2;
            ImGui::Text("Custom Stack Amount:");
            ImGui::SetNextItemWidth(200);
            ImGui::InputInt("##StackAmount", &customStack, 1, 10);
            
            if (ImGui::Button("Set Stack Amount", ImVec2(200, 30)))
            {
                ItemDuping::SetStackAmount(customStack);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Set custom stack amount for selected item");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Scan your backpack to see items");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Loot Vacuum (not implemented)
        ImGui::BeginDisabled();
        if (ImGui::Button("Loot Vacuum", ImVec2(200, 30)))
        {
            // Not implemented
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Not implemented)");

        ImGui::EndChild();
    }

    void RenderAITab()
    {
        ImGui::BeginChild("AITab", ImVec2(0, 0), false);

        // Teleport All AI Button
        if (ImGui::Button("Teleport All AI to Me", ImVec2(200, 30)))
        {
            AIFeatures::TeleportAllAI();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Teleport all AI to your position");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Wave Spawn Editor
        ImGui::Text("Wave Spawn Editor");
        ImGui::Spacing();

        // Wave Spawn Enabled Toggle
        ImGui::Checkbox("Enable Wave Spawn Modifications", &Features::wave_spawn_enabled);
        ImGui::Spacing();

        if (Features::wave_spawn_enabled)
        {
            // Create a scrollable region for wave editing
            ImGui::BeginChild("WaveEditor", ImVec2(0, 400), true);

            for (int i = 0; i < 8; i++)
            {
                ImGui::PushID(i);
                
                ImGui::Text("Wave %d", i + 1);
                ImGui::Separator();

                ImGui::Text("Spawn ID:");
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("##SpawnID", &AIFeatures::waveEditor[i].spawnID);

                ImGui::Text("Count:");
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("##CountMin", &AIFeatures::waveEditor[i].countMin);
                ImGui::SameLine();
                ImGui::Text("to");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("##CountMax", &AIFeatures::waveEditor[i].countMax);

                ImGui::Text("Time (seconds):");
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("##TimeMin", &AIFeatures::waveEditor[i].timeMin);
                ImGui::SameLine();
                ImGui::Text("to");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("##TimeMax", &AIFeatures::waveEditor[i].timeMax);

                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::PopID();
            }

            ImGui::EndChild();

            ImGui::Spacing();

            // Apply Wave Edits Button
            if (ImGui::Button("Apply Wave Edits", ImVec2(200, 30)))
            {
                AIFeatures::ApplyWaveEdits();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Apply wave spawn modifications to current raid");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                "Enable wave spawn modifications to edit waves");
        }

        ImGui::EndChild();
    }

    void RenderMainMenu()
    {
        // Only render if menu is open
        if (!Features::menu_open)
            return;

        // Set window size and position
        ImGui::SetNextWindowSize(ImVec2(700, 550), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

        // Main window
        if (ImGui::Begin("Tegridy Farms", &Features::menu_open, ImGuiWindowFlags_NoCollapse))
        {
            // Tab bar (no header, no separator before tabs)
            if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("PLAYER"))
                {
                    RenderPlayerTab();
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
