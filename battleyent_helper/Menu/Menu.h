#pragma once
#include <Windows.h>

// ========================================
// Menu Control Functions
// ========================================
namespace Menu
{
    // Toggle menu visibility (INSERT key)
    void Toggle();

    // Check if menu key was pressed (call in main loop)
    void CheckInput();
}