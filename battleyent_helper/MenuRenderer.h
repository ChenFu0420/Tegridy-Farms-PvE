#pragma once

// ========================================
// ImGui Menu Rendering
// Reference: MenuRenderer from reference files
// ========================================
namespace MenuRenderer
{
    // Render the main menu window with tabs
    void RenderMainMenu();

    // Render top-right hint text (always visible)
    void RenderHintText();

    // Individual tab renderers
    void RenderPlayerTab();
    void RenderCombatTab();
    void RenderAITab();
    void RenderVisualTab();
    void RenderLootTab();
}