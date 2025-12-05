#pragma once

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
    void RenderEnvironmentTab();
    void RenderLootTab();
    void RenderUtilityTab();
}