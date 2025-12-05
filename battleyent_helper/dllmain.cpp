#include <Windows.h>
#include <iostream>
#include <vector>
#include <cstdarg>
#include "il2api.h"
#include "menu.h"
#include "features.h"
#include "DX11Hook.h"
#include "Overlay.h"
#include "MenuRenderer.h"

// ========================================
// BEService Bypass Patch
// Reference: RecoverablePatchesAiTeleAndAdvapiSource.txt lines 783-818
// ========================================

bool PatchMemory(void* address, const unsigned char* data, size_t size)
{
    if (!address || !data) return false;

    DWORD oldProtect;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    memcpy(address, data, size);
    VirtualProtect(address, size, oldProtect, &oldProtect);

    return true;
}

bool PatchAdvapi32()
{
    HMODULE hAdvapi = GetModuleHandleA("Advapi32.dll");
    if (!hAdvapi)
    {
        printf("[-] Failed to get Advapi32.dll handle\n");
        return false;
    }

    FARPROC pOpenServiceA = GetProcAddress(hAdvapi, "OpenServiceA");
    FARPROC pQueryServiceStatusEx = GetProcAddress(hAdvapi, "QueryServiceStatusEx");

    if (!pOpenServiceA || !pQueryServiceStatusEx)
    {
        printf("[-] Failed to get BEService functions\n");
        return false;
    }

    // Patch OpenServiceA to always return success (mov al, 1; ret)
    const unsigned char patchOpenServiceA[] = { 0xB0, 0x01, 0xC3 };

    // Patch QueryServiceStatusEx to set SERVICE_STOPPED and return success
    const unsigned char patchQueryServiceStatusEx[] = {
        0x41, 0xC7, 0x40, 0x04, 0x04, 0x00, 0x00, 0x00,  // mov dword ptr [r8+4], 4 (SERVICE_STOPPED)
        0xB0, 0x01,                                       // mov al, 1
        0xC3                                              // ret
    };

    if (!PatchMemory(reinterpret_cast<void*>(pOpenServiceA),
        patchOpenServiceA,
        sizeof(patchOpenServiceA)))
    {
        printf("[-] Failed to patch OpenServiceA\n");
        return false;
    }

    if (!PatchMemory(reinterpret_cast<void*>(pQueryServiceStatusEx),
        patchQueryServiceStatusEx,
        sizeof(patchQueryServiceStatusEx)))
    {
        printf("[-] Failed to patch QueryServiceStatusEx\n");
        return false;
    }

    printf("[+] BEService bypass patches applied\n");
    return true;
}

// ========================================
// Present Hook Implementation
// ========================================

HRESULT __stdcall DX11Hook::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    // First call - initialize overlay
    if (!Overlay::IsInitialized())
    {
        if (Overlay::Init(pSwapChain))
        {
            printf("[+] ImGui overlay initialized from Present hook\n");
        }
    }

    // Render overlay every frame
    if (Overlay::IsInitialized())
    {
        Overlay::BeginFrame();

        // Render hint text (always visible)
        MenuRenderer::RenderHintText();

        // Render main menu (only if menu_open)
        MenuRenderer::RenderMainMenu();

        // Render ESP BEFORE ImGui finishes frame
        // Reference: PlayerESP.cs rendering pattern adapted to ImGui
        if (Features::ai_esp)
        {
            ESPFeatures::RenderPlayerESP();
        }

        Overlay::RenderFrame();
    }

    // Call original Present
    return oPresent(pSwapChain, SyncInterval, Flags);
}

// ========================================
// IL2CPP Function Pointer Definitions
// ========================================

il2cpp_get_root_domain_prot il2cpp_get_root_domain = nullptr;
il2cpp_thread_attach_prot il2cpp_thread_attach = nullptr;
il2cpp_domain_get_assemblies_prot il2cpp_domain_get_assemblies = nullptr;
il2cpp_class_from_name_prot il2cpp_class_from_name = nullptr;
il2cpp_class_get_methods_prot il2cpp_class_get_methods = nullptr;
il2cpp_method_get_name_prot il2cpp_method_get_name = nullptr;
il2cpp_assembly_get_image_prot il2cpp_assembly_get_image = nullptr;
il2cpp_image_get_name_prot il2cpp_image_get_name = nullptr;
il2cpp_runtime_invoke_prot il2cpp_runtime_invoke = nullptr;

std::vector<const Il2CppMethod*> found_methods;
std::vector<const Il2CppMethod*> error_screen_methods{};

void* g_TarkovApplicationInstance = nullptr;
void* g_CameraManagerInstance = nullptr;

// Raid location tracking
uintptr_t g_selectedLocation = 0;

// ========================================
// Static Fields Offset
// ========================================
#define STATIC_FIELDS_OFFSET 0xB8

// ========================================
// Helper Functions
// ========================================

void find_methods_by_names(Il2CppClass* klass, const std::vector<std::string>& names)
{
    found_methods.clear();
    if (!klass) return;

    void* iter = nullptr;
    const Il2CppMethod* method;

    while ((method = il2cpp_class_get_methods(klass, &iter)))
    {
        const char* mname = il2cpp_method_get_name(method);
        if (!mname) continue;

        for (const auto& target : names)
        {
            if (strcmp(mname, target.c_str()) == 0)
            {
                found_methods.push_back(method);
            }
        }
    }
}

Il2CppImage* il2cpp_image_loaded(const char* image_name)
{
    size_t assemblyCount;
    const Il2CppAssembly** assemblies = il2cpp_domain_get_assemblies(il2cpp_get_root_domain(), &assemblyCount);

    for (size_t i = 0; i < assemblyCount; i++)
    {
        const Il2CppImage* image = il2cpp_assembly_get_image(assemblies[i]);
        if (image && strcmp(il2cpp_image_get_name(image), image_name) == 0)
        {
            return const_cast<Il2CppImage*>(image);
        }
    }

    return nullptr;
}

void* get_tarkov_application_instance(Il2CppClass* tarkovApp)
{
    if (!tarkovApp) return nullptr;

    void* staticFields = *(void**)((uintptr_t)tarkovApp + STATIC_FIELDS_OFFSET);
    if (!staticFields) return nullptr;

    for (int i = 0; i < 32; i++)
    {
        void** fieldPtr = (void**)((uintptr_t)staticFields + (i * sizeof(void*)));
        void* candidate = *fieldPtr;

        if (candidate && !IsBadReadPtr(candidate, 8))
        {
            void** vtable = *(void***)candidate;
            if (vtable && !IsBadReadPtr(vtable, 8))
            {
                return candidate;
            }
        }
    }

    return nullptr;
}

void* get_camera_manager_instance(Il2CppImage* image)
{
    if (!image) return nullptr;

    Il2CppClass* klass = il2cpp_class_from_name(image, "EFT", "CameraManager");
    if (!klass) return nullptr;

    void* staticFields = *(void**)((uintptr_t)klass + STATIC_FIELDS_OFFSET);
    if (!staticFields) return nullptr;

    for (int i = 0; i < 16; i++)
    {
        void** fieldPtr = (void**)((uintptr_t)staticFields + (i * sizeof(void*)));
        void* candidate = *fieldPtr;

        if (candidate && !IsBadReadPtr(candidate, 8))
        {
            void** vtable = *(void***)candidate;
            if (vtable && !IsBadReadPtr(vtable, 8))
            {
                return candidate;
            }
        }
    }

    return nullptr;
}

const Il2CppMethod* wait_for_battleye_init(Il2CppClass* tarkovApp)
{
    if (!tarkovApp) return nullptr;

    const Il2CppMethod* method = nullptr;
    void* iter = nullptr;

    while ((method = il2cpp_class_get_methods(tarkovApp, &iter)))
    {
        const char* name = il2cpp_method_get_name(method);
        if (name && strstr(name, "RunValidation"))
        {
            return method;
        }
    }

    return nullptr;
}

void find_show_error_screen(Il2CppClass* preloaderUI)
{
    error_screen_methods.clear();
    if (!preloaderUI) return;

    void* iter = nullptr;
    const Il2CppMethod* method;

    while ((method = il2cpp_class_get_methods(preloaderUI, &iter)))
    {
        const char* name = il2cpp_method_get_name(method);
        if (name && strstr(name, "ShowErrorScreen"))
        {
            error_screen_methods.push_back(method);
        }
    }
}

void patch_method_ret_safe(const Il2CppMethod* method)
{
    if (!method) return;

    void* fn = nullptr;
    while (!(fn = *(void**)method))
    {
        Sleep(50);
    }

    DWORD oldProtect;
    if (VirtualProtect(fn, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        uint8_t* code = (uint8_t*)fn;
        code[0] = 0xC3; // RET
        VirtualProtect(fn, 16, oldProtect, &oldProtect);
    }
}

// ========================================
// Main Start Function
// ========================================

void start()
{
    // Allocate console for logging
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    printf("========================================\n");
    printf("  Tegridy Farms - PvE Bypass\n");
    printf("  Version: 1.0\n");
    printf("========================================\n\n");
    printf("[+] BEService bypass applied in DllMain\n\n");

    // ========================================
    // Step 1: Wait for GameAssembly.dll
    // ========================================
    printf("[*] Step 1: Waiting for GameAssembly.dll...\n");
    HMODULE il2cpp = nullptr;
    while (!(il2cpp = GetModuleHandleA("GameAssembly.dll")))
    {
        Sleep(2000);
    }
    printf("[+] GameAssembly.dll loaded at 0x%llX\n\n", (uintptr_t)il2cpp);

    // ========================================
    // Step 2: Load IL2CPP APIs
    // ========================================
    printf("[*] Step 2: Loading IL2CPP APIs...\n");
    il2cpp_get_root_domain = (il2cpp_get_root_domain_prot)GetProcAddress(il2cpp, "il2cpp_domain_get");
    il2cpp_thread_attach = (il2cpp_thread_attach_prot)GetProcAddress(il2cpp, "il2cpp_thread_attach");
    il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_prot)GetProcAddress(il2cpp, "il2cpp_domain_get_assemblies");
    il2cpp_class_from_name = (il2cpp_class_from_name_prot)GetProcAddress(il2cpp, "il2cpp_class_from_name");
    il2cpp_class_get_methods = (il2cpp_class_get_methods_prot)GetProcAddress(il2cpp, "il2cpp_class_get_methods");
    il2cpp_method_get_name = (il2cpp_method_get_name_prot)GetProcAddress(il2cpp, "il2cpp_method_get_name");
    il2cpp_assembly_get_image = (il2cpp_assembly_get_image_prot)GetProcAddress(il2cpp, "il2cpp_assembly_get_image");
    il2cpp_image_get_name = (il2cpp_image_get_name_prot)GetProcAddress(il2cpp, "il2cpp_image_get_name");
    il2cpp_runtime_invoke = (il2cpp_runtime_invoke_prot)GetProcAddress(il2cpp, "il2cpp_runtime_invoke");

    if (!il2cpp_get_root_domain || !il2cpp_thread_attach || !il2cpp_domain_get_assemblies)
    {
        printf("[-] Failed to load IL2CPP APIs\n");
        return;
    }
    printf("[+] IL2CPP APIs loaded successfully\n\n");

    // Wait for IL2CPP to fully initialize
    Sleep(15000);

    // ========================================
    // Step 3: Attach to IL2CPP Domain
    // ========================================
    printf("[*] Step 3: Attaching to IL2CPP domain...\n");
    Il2CppDomain* domain = il2cpp_get_root_domain();
    if (!domain)
    {
        printf("[-] Failed to get IL2CPP root domain\n");
        return;
    }
    il2cpp_thread_attach(domain);
    printf("[+] Attached to IL2CPP domain\n\n");

    Sleep(1000);

    // ========================================
    // Step 4: Wait for Assembly-CSharp.dll
    // ========================================
    printf("[*] Step 4: Waiting for Assembly-CSharp.dll...\n");
    Il2CppImage* image = nullptr;
    while (!(image = il2cpp_image_loaded("Assembly-CSharp.dll")))
    {
        Sleep(2000);
    }
    printf("[+] Assembly-CSharp.dll loaded\n\n");

    Sleep(1000);

    // ========================================
    // Step 5: Find TarkovApplication Class
    // ========================================
    printf("[*] Step 5: Finding TarkovApplication class...\n");
    Il2CppClass* tarkovApp = il2cpp_class_from_name(image, "EFT", "TarkovApplication");
    if (!tarkovApp)
    {
        printf("[-] TarkovApplication class not found\n");
        return;
    }
    printf("[+] TarkovApplication class found\n\n");

    Sleep(1000);

    // ========================================
    // Step 6: Patch BattlEye Initialization
    // ========================================
    printf("[*] Step 6: Patching BattlEye initialization...\n");
    const Il2CppMethod* battleyeMethod = wait_for_battleye_init(tarkovApp);
    if (battleyeMethod)
    {
        patch_method_ret_safe(battleyeMethod);
        printf("[+] BattlEye init method patched\n");
    }
    else
    {
        printf("[-] BattlEye init method not found\n");
    }

    // Try to unload BEClient if it's loaded
    HMODULE beclient = GetModuleHandleA("BEClient_x64.dll");
    if (beclient)
    {
        FreeLibrary(beclient);
        printf("[+] BEClient_x64.dll unloaded\n");
    }
    printf("\n");

    Sleep(1000);

    // ========================================
    // Step 7: Patch Error Screens
    // ========================================
    printf("[*] Step 7: Patching error screens...\n");
    Il2CppClass* preloaderUI = il2cpp_class_from_name(image, "EFT.UI", "PreloaderUI");
    if (preloaderUI)
    {
        find_show_error_screen(preloaderUI);
        for (auto method : error_screen_methods)
        {
            patch_method_ret_safe(method);
        }
        printf("[+] Error screens patched\n");
    }
    printf("\n");

    // ========================================
    // Step 8: Initialize DX11 Hook for ImGui Overlay
    // ========================================
    printf("[*] Step 8: Initializing DX11 hook for overlay...\n");
    Sleep(10000);

    if (DX11Hook::Initialize())
    {
        printf("[+] DX11 hook ready\n");
    }
    else
    {
        printf("[-] Failed to initialize DX11 hook\n");
    }
    printf("\n");

    // ========================================
    // Ready Banner
    // ========================================
    printf("========================================\n");
    printf("  Tegridy Farms - Ready!\n");
    printf("========================================\n\n");
    printf("Controls:\n");
    printf("  INSERT - Toggle menu\n");
    printf("  END    - Emergency exit\n\n");
    printf("[*] Scanning for TarkovApplication + CameraManager...\n\n");

    // ========================================
    // Main Loop - Monitor and Apply Patches
    // ========================================
    bool patchesApplied = false;
    float nextRaidUpdateTime = 0.0f;
    const float raidUpdateInterval = 2.0f; // Check raid location every 2 seconds

    while (true)
    {
        // Check menu input
        Menu::CheckInput();

        // Check for END key press (panic key)
        if (GetAsyncKeyState(VK_END) & 0x8000)
        {
            printf("\n[!] END key detected - Initiating safe shutdown...\n");
            Sleep(500);
            printf("[*] Terminating game process...\n");
            TerminateProcess(GetCurrentProcess(), 0);
            return;
        }

        // Track instances
        if (!g_TarkovApplicationInstance)
        {
            g_TarkovApplicationInstance = get_tarkov_application_instance(tarkovApp);
            if (g_TarkovApplicationInstance)
            {
                printf("[+] TarkovApplication = 0x%llX\n", (uintptr_t)g_TarkovApplicationInstance);
            }
        }

        void* cm = get_camera_manager_instance(image);
        if (cm && cm != g_CameraManagerInstance)
        {
            g_CameraManagerInstance = cm;
        }

        // Track previous feature states to detect changes
        static bool prev_god_mode = false;
        static bool prev_infinite_stamina = false;
        static bool prev_no_fall_damage = false;
        static bool prev_no_energy_drain = false;
        static bool prev_no_hydration_drain = false;
        static bool prev_no_fatigue = false;
        static bool prev_no_environment_damage = false;
        static bool prev_breach_all_doors = false;
        static bool prev_instant_search = false;
        static bool prev_ai_ignore = false;
        static bool prev_no_recoil = false;
        static bool prev_no_weapon_durability = false;
        static bool prev_no_weapon_malfunction = false;
        static bool prev_no_weapon_overheating = false;

        // Check if any feature toggle has changed
        bool featuresChanged =
            (Features::god_mode != prev_god_mode) ||
            (Features::infinite_stamina != prev_infinite_stamina) ||
            (Features::no_fall_damage != prev_no_fall_damage) ||
            (Features::no_energy_drain != prev_no_energy_drain) ||
            (Features::no_hydration_drain != prev_no_hydration_drain) ||
            (Features::no_fatigue != prev_no_fatigue) ||
            (Features::no_environment_damage != prev_no_environment_damage) ||
            (Features::breach_all_doors != prev_breach_all_doors) ||
            (Features::instant_search != prev_instant_search) ||
            (Features::ai_ignore != prev_ai_ignore) ||
            (Features::no_recoil != prev_no_recoil) ||
            (Features::no_weapon_durability != prev_no_weapon_durability) ||
            (Features::no_weapon_malfunction != prev_no_weapon_malfunction) ||
            (Features::no_weapon_overheating != prev_no_weapon_overheating);

        // Apply features only when CameraManager exists and (first time OR features changed)
        if (cm && (!patchesApplied || featuresChanged))
        {
            if (!patchesApplied)
            {
                printf("[*] Game ready - applying feature patches...\n");
            }
            else if (featuresChanged)
            {
                printf("[*] Feature toggle changed - reapplying patches...\n");
            }

            FeaturePatch::ApplyAllEnabledFeatures(image);

            // Update previous states
            prev_god_mode = Features::god_mode;
            prev_infinite_stamina = Features::infinite_stamina;
            prev_no_fall_damage = Features::no_fall_damage;
            prev_no_energy_drain = Features::no_energy_drain;
            prev_no_hydration_drain = Features::no_hydration_drain;
            prev_no_fatigue = Features::no_fatigue;
            prev_no_environment_damage = Features::no_environment_damage;
            prev_breach_all_doors = Features::breach_all_doors;
            prev_instant_search = Features::instant_search;
            prev_ai_ignore = Features::ai_ignore;
            prev_no_recoil = Features::no_recoil;
            prev_no_weapon_durability = Features::no_weapon_durability;
            prev_no_weapon_malfunction = Features::no_weapon_malfunction;
            prev_no_weapon_overheating = Features::no_weapon_overheating;

            if (!patchesApplied)
            {
                patchesApplied = true;
                printf("\n[+] Patches Applied\n\n");
            }
        }

        // Update raid/SelectedLocation periodically (every 2 seconds)
        float currentTime = GetTickCount64() / 1000.0f;
        if (currentTime >= nextRaidUpdateTime)
        {
            FeaturePatch::UpdateRaidSelectedLocation(image);
            nextRaidUpdateTime = currentTime + raidUpdateInterval;
        }

        // Update god mode (continuously set damage coefficient if enabled)
        // This runs every frame (100ms) to maintain invulnerability
        FeaturePatch::UpdateGodMode(image);

        // Update thermal/NVG (continuous write to ThermalVision struct)
        // Reference: ThermalNvgSource.txt
        if (cm && (Features::night_vision || Features::thermal_vision))
        {
            ThermalVision::UpdateThermalNVG();
        }

        // Update AI ESP data (scan players, calculate screen positions)
        // Reference: PlayerESP.cs UpdatePlayerESP pattern
        if (cm && Features::ai_esp)
        {
            ESPFeatures::UpdatePlayerESP(image);
        }

        // Print CameraManager at bottom for easy copy/paste to CE
        // This runs after all updates so it stays at bottom of console
        static uintptr_t lastPrintedCM = 0;
        if (cm && (uintptr_t)cm != lastPrintedCM)
        {
            printf("\n========================================\n");
            printf("[CM] CameraManager = 0x%llX (copy for CE)\n", (uintptr_t)cm);
            printf("========================================\n\n");
            lastPrintedCM = (uintptr_t)cm;
        }

        Sleep(100);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        
        // Patch Advapi32 immediately (CRITICAL - must be before CreateThread)
        // Reference: RecoverablePatchesAiTeleAndAdvapiSource.txt lines 1289-1293
        if (!PatchAdvapi32())
        {
            return FALSE; // Fail DLL load if BEService patch fails
        }
        
        // Start main thread
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)start, nullptr, 0, nullptr);
    }
    return TRUE;
}
