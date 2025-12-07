#include "Menu.h"
#include "Features.h"
#include <cstdio>

// ========================================
// Menu Control Functions
// ========================================

namespace Menu
{
    void Toggle()
    {
        Features::menu_open = !Features::menu_open;

        if (Features::menu_open)
        {
            printf("[*] Menu opened (INSERT to close)\n");
        }
        else
        {
            printf("[*] Menu closed\n");
        }
    }

    void CheckInput()
    {
        // Check for INSERT key (toggle menu)
        static bool insertPressed = false;
        if (GetAsyncKeyState(VK_INSERT) & 0x8000)
        {
            if (!insertPressed)
            {
                Toggle();
                insertPressed = true;
            }
        }
        else
        {
            insertPressed = false;
        }
    }
}