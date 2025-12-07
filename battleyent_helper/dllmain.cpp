#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include "Advapi32Patch.h"
#include "Overlay.h"
#include "DX11Hook.h"
#include "Menu.h"
#include "MenuRenderer.h"
#include "Features.h"

// =================================================================
// 0. IL2CPP Structs and Function Type Definitions
// =================================================================
typedef void (*Il2CppMethodPointer)();
struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppThread;
struct Il2CppClass;
struct Il2CppMethod;
struct Il2CppImage;
struct Il2CppObject;
struct Il2CppField;
struct Il2CppArray;

// IL2CPP API Function Pointer Type Definitions
using il2cpp_get_root_domain_prot = Il2CppDomain * (*)();
static il2cpp_get_root_domain_prot il2cpp_get_root_domain = nullptr;

using il2cpp_thread_attach_prot = Il2CppThread * (*)(Il2CppDomain*);
static il2cpp_thread_attach_prot il2cpp_thread_attach = nullptr;

using il2cpp_domain_get_assemblies_prot = const Il2CppAssembly** (*)(Il2CppDomain*, size_t*);
static il2cpp_domain_get_assemblies_prot il2cpp_domain_get_assemblies = nullptr;

using il2cpp_class_from_name_prot = Il2CppClass * (*)(const Il2CppImage*, const char*, const char*);
il2cpp_class_from_name_prot il2cpp_class_from_name = nullptr;

using il2cpp_class_get_methods_prot = const Il2CppMethod* (*)(Il2CppClass*, void**);
il2cpp_class_get_methods_prot il2cpp_class_get_methods = nullptr;

using il2cpp_method_get_name_prot = const char* (*)(const Il2CppMethod*);
il2cpp_method_get_name_prot il2cpp_method_get_name = nullptr;

using il2cpp_assembly_get_image_prot = const Il2CppImage* (*)(const Il2CppAssembly*);
static il2cpp_assembly_get_image_prot il2cpp_assembly_get_image = nullptr;

using il2cpp_image_get_name_prot = const char* (*)(const Il2CppImage*);
static il2cpp_image_get_name_prot il2cpp_image_get_name = nullptr;

using il2cpp_runtime_invoke_prot = void* (*)(const Il2CppMethod*, void*, void**, void**);
static il2cpp_runtime_invoke_prot il2cpp_runtime_invoke = nullptr;

// =================================================================
// 1. Global Variables
// =================================================================
std::vector<const Il2CppMethod*> found_methods;
std::vector<const Il2CppMethod*> error_screen_methods{};
std::mutex log_mutex;

void* g_TarkovApplicationInstance = nullptr;
void* g_CameraManagerInstance = nullptr;
uintptr_t g_selectedLocation = 0;

#define STATIC_FIELDS_OFFSET 0xB8

// =================================================================
// 2. DX11 Present Hook Implementation
// =================================================================

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

        Overlay::RenderFrame();
    }

    // Call original Present
    return oPresent(pSwapChain, SyncInterval, Flags);
}

// =================================================================
// 3. Utility Functions (IL2CPP Helpers)
// =================================================================

// Thread-safe logging
void log_info(const char* fmt, ...)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

/**
 * @brief Find methods by name
 */
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

/**
 * @brief Load IL2CPP image by name
 */
Il2CppImage* il2cpp_image_loaded(const char* image_name)
{
    Il2CppDomain* domain = il2cpp_get_root_domain();
    if (!domain) return nullptr;

    size_t size = 0;
    const Il2CppAssembly* const* asmbl = il2cpp_domain_get_assemblies(domain, &size);
    if (!asmbl) return nullptr;

    for (size_t i = 0; i < size; i++)
    {
        const Il2CppImage* img = il2cpp_assembly_get_image(asmbl[i]);
        if (!img) continue;

        const char* nm = il2cpp_image_get_name(img);
        if (!nm) continue;

        if (_stricmp(nm, image_name) == 0)
            return (Il2CppImage*)img;
    }

    return nullptr;
}

/**
 * @brief Find single IL2CPP method by name
 */
const Il2CppMethod* FindMethod(Il2CppClass* klass, const char* methodName)
{
    if (!klass || !methodName) return nullptr;

    void* iter = nullptr;
    const Il2CppMethod* method;

    while ((method = il2cpp_class_get_methods(klass, &iter)))
    {
        const char* name = il2cpp_method_get_name(method);
        if (name && strcmp(name, methodName) == 0)
        {
            return method;
        }
    }

    return nullptr;
}

/**
 * @brief Patch bool getter to return specific value
 */
void PatchBoolGetter(void* methodPtr, bool returnValue, unsigned char* savedBytes)
{
    if (!methodPtr) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    // Save original bytes
    if (savedBytes)
    {
        memcpy(savedBytes, methodPtr, 16);
    }

    uint8_t* code = (uint8_t*)methodPtr;
    code[0] = 0xB0;                 // mov al, imm8
    code[1] = returnValue ? 1 : 0;
    code[2] = 0xC3;                 // ret

    VirtualProtect(methodPtr, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 16);
}

/**
 * @brief Restore bool getter original bytes
 */
void RestoreBoolGetter(void* methodPtr, unsigned char* savedBytes)
{
    if (!methodPtr || !savedBytes) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    memcpy(methodPtr, savedBytes, 16);

    VirtualProtect(methodPtr, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 16);
}

/**
 * @brief Patch float getter to return specific value
 */
void PatchFloatGetter(void* methodPtr, float returnValue, unsigned char* savedBytes)
{
    if (!methodPtr) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 32, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    // Save original bytes
    if (savedBytes)
    {
        memcpy(savedBytes, methodPtr, 16);
    }

    static float forced_value = 0.0f;
    forced_value = returnValue;

    uint8_t stub[32];
    memset(stub, 0x90, sizeof(stub));
    stub[0] = 0x48;
    stub[1] = 0xB8;
    uint64_t addr = (uint64_t)&forced_value;
    memcpy(&stub[2], &addr, sizeof(addr));
    int idx = 2 + (int)sizeof(addr);
    stub[idx++] = 0xF3;
    stub[idx++] = 0x0F;
    stub[idx++] = 0x10;
    stub[idx++] = 0x00;
    stub[idx++] = 0xC3;

    memcpy(methodPtr, stub, idx);

    VirtualProtect(methodPtr, 32, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 32);
}

/**
 * @brief Restore float getter original bytes
 */
void RestoreFloatGetter(void* methodPtr, unsigned char* savedBytes)
{
    if (!methodPtr || !savedBytes) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    memcpy(methodPtr, savedBytes, 16);

    VirtualProtect(methodPtr, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 16);
}

/**
 * @brief Safe version with waiting
 */
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
        code[0] = 0xC3;
        VirtualProtect(fn, 16, oldProtect, &oldProtect);
    }
}

// =================================================================
// 4. BattlEye Functions
// =================================================================

const Il2CppMethod* find_battleye_init(Il2CppClass* main_application)
{
    if (!main_application) return nullptr;

    void* iter = nullptr;
    const Il2CppMethod* method;

    while ((method = il2cpp_class_get_methods(main_application, &iter)))
    {
        const char* name = il2cpp_method_get_name(method);
        if (!name) continue;

        // Check for obfuscated name
        if ((unsigned char)name[0] == 0xEE &&
            (unsigned char)name[1] == 0x80 &&
            (unsigned char)name[2] == 0x81)
            return method;

        // Check for plain text name
        if (strcmp(name, "ValidateAnticheat") == 0)
            return method;
    }

    return nullptr;
}

const Il2CppMethod* wait_for_battleye_init(Il2CppClass* main_application)
{
    const Il2CppMethod* method = nullptr;
    while (!(method = find_battleye_init(main_application)))
        Sleep(200);
    return method;
}

void find_show_error_screen(Il2CppClass* preloader_ui)
{
    error_screen_methods.clear();

    void* iter = nullptr;
    const Il2CppMethod* method;

    while ((method = il2cpp_class_get_methods(preloader_ui, &iter)))
    {
        const char* name = il2cpp_method_get_name(method);
        if (!name) continue;

        if (strcmp(name, "ShowErrorScreen") == 0)
        {
            error_screen_methods.push_back(method);
        }
    }
}

// =================================================================
// 5. Game Instance Functions
// =================================================================

void* get_tarkov_application_instance(Il2CppClass* klass)
{
    uintptr_t static_fields_ptr = *(uintptr_t*)((uintptr_t)klass + STATIC_FIELDS_OFFSET);
    if (!static_fields_ptr) return nullptr;

    uintptr_t* static_fields = (uintptr_t*)static_fields_ptr;
    for (int i = 0; i < 32; i++)
    {
        uintptr_t candidate = static_fields[i];
        if (!candidate) continue;

        Il2CppClass* objClass = *(Il2CppClass**)candidate;
        if (objClass == klass)
            return (void*)candidate;
    }

    return nullptr;
}

void* get_camera_manager_instance(Il2CppImage* image)
{
    auto cameraManagerClass = il2cpp_class_from_name(image, "EFT.CameraControl", "CameraManager");
    if (!cameraManagerClass) return nullptr;

    uintptr_t static_fields_ptr = *(uintptr_t*)((uintptr_t)cameraManagerClass + STATIC_FIELDS_OFFSET);
    if (!static_fields_ptr) return nullptr;

    uintptr_t* static_fields = (uintptr_t*)static_fields_ptr;
    for (int i = 0; i < 16; i++)
    {
        uintptr_t candidate = static_fields[i];
        if (!candidate) continue;

        Il2CppClass* objClass = *(Il2CppClass**)candidate;
        if (objClass == cameraManagerClass)
            return (void*)candidate;
    }

    return nullptr;
}

// =================================================================
// 6. Main Execution
// =================================================================

void start()
{
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);

    printf("========================================\n");
    printf("  Tegridy Farms - PvE Bypass\n");
    printf("  Version: 1.0.0.2.42157\n");
    printf("========================================\n\n");
    printf("[+] Thread-freezing Advapi32 patches applied successfully\n\n");

    // Initialize IL2CPP
    printf("[*] Step 1: Initializing IL2CPP...\n");

    HMODULE il2cpp = nullptr;
    while (!(il2cpp = GetModuleHandleA("GameAssembly.dll")))
        Sleep(2000);
    printf("[+] GameAssembly.dll Found\n");

    // Resolve IL2CPP functions
    il2cpp_get_root_domain = (il2cpp_get_root_domain_prot)GetProcAddress(il2cpp, "il2cpp_domain_get");
    il2cpp_thread_attach = (il2cpp_thread_attach_prot)GetProcAddress(il2cpp, "il2cpp_thread_attach");
    il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_prot)GetProcAddress(il2cpp, "il2cpp_domain_get_assemblies");
    il2cpp_class_from_name = (il2cpp_class_from_name_prot)GetProcAddress(il2cpp, "il2cpp_class_from_name");
    il2cpp_class_get_methods = (il2cpp_class_get_methods_prot)GetProcAddress(il2cpp, "il2cpp_class_get_methods");
    il2cpp_method_get_name = (il2cpp_method_get_name_prot)GetProcAddress(il2cpp, "il2cpp_method_get_name");
    il2cpp_assembly_get_image = (il2cpp_assembly_get_image_prot)GetProcAddress(il2cpp, "il2cpp_assembly_get_image");
    il2cpp_image_get_name = (il2cpp_image_get_name_prot)GetProcAddress(il2cpp, "il2cpp_image_get_name");
    il2cpp_runtime_invoke = (il2cpp_runtime_invoke_prot)GetProcAddress(il2cpp, "il2cpp_runtime_invoke");

    Sleep(15000);

    Il2CppDomain* domain = il2cpp_get_root_domain();
    il2cpp_thread_attach(domain);
    printf("[+] Attached to IL2CPP domain\n");

    Sleep(1000);

    // Find Assembly-CSharp
    printf("\n[*] Step 2: Loading Assembly-CSharp...\n");

    Il2CppImage* image = nullptr;
    while (!(image = il2cpp_image_loaded("Assembly-CSharp.dll")))
        Sleep(2000);
    printf("[+] Assembly-CSharp.dll Found\n");

    Sleep(1000);

    // Patch BattlEye
    printf("\n[*] Step 3: Patching BattlEye initialization...\n");

    auto tarkovApp = il2cpp_class_from_name(image, "EFT", "TarkovApplication");
    if (!tarkovApp)
    {
        printf("[-] TarkovApplication Not Found\n");
        return;
    }
    printf("[+] TarkovApplication Found\n");

    Sleep(1000);

    const Il2CppMethod* battleyeMethod = wait_for_battleye_init(tarkovApp);
    if (battleyeMethod)
    {
        patch_method_ret_safe(battleyeMethod);
        printf("[+] Battleye init patched\n");
    }

    printf("[+] BEClient_x64.dll prevented from loading via Advapi32 bypass\n");

    Sleep(1000);

    // Patch error screen
    printf("\n[*] Step 4: Patching error screen...\n");

    auto preloaderUI = il2cpp_class_from_name(image, "EFT.UI", "PreloaderUI");
    if (preloaderUI)
    {
        find_show_error_screen(preloaderUI);
        for (auto m : error_screen_methods)
        {
            patch_method_ret_safe(m);
            printf("[+] ShowErrorScreen --> Patched\n");
        }
    }

    Sleep(1000);

    // Initialize all feature patches (create them, don't enable yet)
    printf("\n[*] Step 5: Initializing feature patches...\n");
    FeaturePatch::InitializeAllPatches(image);

    Sleep(1000);

    // Initialize DX11 Hook
    printf("\n[*] Step 6: Initializing DX11 hook for ImGui...\n");
    Sleep(5000);

    if (DX11Hook::Initialize())
    {
        printf("[+] DX11 hook initialized\n");
    }
    else
    {
        printf("[-] Failed to initialize DX11 hook\n");
    }

    Sleep(1000);

    // Complete
    printf("\n========================================\n");
    printf("  All patches applied successfully!\n");
    printf("========================================\n\n");
    printf("[SUCCESS] You can now play offline PVE\n");
    printf("[INFO] Press INSERT to open menu\n");
    printf("[INFO] Press END key to terminate safely\n\n");

    printf("[*] Scanning for game instances...\n\n");

    // Main loop
    bool patchesInitialized = false;

    // Track previous feature states for change detection
    bool prev_god_mode = false;
    bool prev_infinite_stamina = false;
    bool prev_no_recoil = false;
    bool prev_no_weapon_durability = false;
    bool prev_no_weapon_malfunction = false;
    bool prev_no_weapon_overheating = false;
    bool prev_ai_not_attack = false;
    bool prev_breach_all_doors = false;
    bool prev_lucky_search = false;

    while (!(GetAsyncKeyState(VK_END) & 0x8000))
    {
        // Check menu input (INSERT key)
        Menu::CheckInput();

        // Track TarkovApplication instance
        if (!g_TarkovApplicationInstance)
        {
            g_TarkovApplicationInstance = get_tarkov_application_instance(tarkovApp);
            if (g_TarkovApplicationInstance)
            {
                printf("[+] TarkovApplication = 0x%llX\n", (uintptr_t)g_TarkovApplicationInstance);
            }
        }

        // Track CameraManager instance
        void* cm = get_camera_manager_instance(image);
        if (cm && cm != g_CameraManagerInstance)
        {
            g_CameraManagerInstance = cm;
            printf("[+] CameraManager = 0x%llX\n", (uintptr_t)g_CameraManagerInstance);
        }

        // Check if any feature toggle has changed
        bool featuresChanged =
            (Features::god_mode != prev_god_mode) ||
            (Features::infinite_stamina != prev_infinite_stamina) ||
            (Features::no_recoil != prev_no_recoil) ||
            (Features::no_weapon_durability != prev_no_weapon_durability) ||
            (Features::no_weapon_malfunction != prev_no_weapon_malfunction) ||
            (Features::no_weapon_overheating != prev_no_weapon_overheating) ||
            (Features::ai_not_attack != prev_ai_not_attack) ||
            (Features::breach_all_doors != prev_breach_all_doors) ||
            (Features::lucky_search != prev_lucky_search);

        // Apply features when CameraManager exists and (first time OR features changed)
        if (cm && (!patchesInitialized || featuresChanged))
        {
            if (!patchesInitialized)
            {
                printf("[*] Game ready - features can now be toggled via menu\n");
                patchesInitialized = true;
            }

            // Apply all enabled features
            FeaturePatch::ApplyAllEnabledFeatures(image);

            // Update previous states
            prev_god_mode = Features::god_mode;
            prev_infinite_stamina = Features::infinite_stamina;
            prev_no_recoil = Features::no_recoil;
            prev_no_weapon_durability = Features::no_weapon_durability;
            prev_no_weapon_malfunction = Features::no_weapon_malfunction;
            prev_no_weapon_overheating = Features::no_weapon_overheating;
            prev_ai_not_attack = Features::ai_not_attack;
            prev_breach_all_doors = Features::breach_all_doors;
            prev_lucky_search = Features::lucky_search;
        }

        // Continuous updates (run every frame)
        if (cm)
        {
            FeaturePatch::UpdateGodMode();
        }

        // Update offline PVE (every frame, but only writes when location changes)
        FeaturePatch::UpdateOfflinePVE();

        // Print CameraManager periodically for CE debugging
        static DWORD lastPrintTime = 0;
        DWORD currentTime = GetTickCount();
        if (cm && (currentTime - lastPrintTime) > 10000) // Every 10 seconds
        {
            printf("\n========================================\n");
            printf("[CM] CameraManager = 0x%llX (copy for CE)\n", (uintptr_t)cm);
            printf("========================================\n\n");
            lastPrintTime = currentTime;
        }

        Sleep(100);
    }

    // END key pressed
    printf("\n[!] END key detected - terminating process...\n");
    printf("[!] No telemetry will be sent.\n");
    Sleep(1000);

    TerminateProcess(GetCurrentProcess(), 0);
}

// =================================================================
// 7. DLL Entry Point
// =================================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        // ========================================
        // CRITICAL: Apply thread-freezing BEService bypass
        // ========================================

        if (!Advapi32Patch::PerformPreInjectionChecks())
        {
            // Show error message to user
            MessageBoxA(NULL,
                "BEService bypass failed!\n\n"
                "The injection has been aborted for safety.\n"
                "This prevents potential detection by BattlEye.\n\n"
                "Possible reasons:\n"
                "1. BattlEye is already active\n"
                "2. BEClient_x64.dll is already loaded\n"
                "3. Thread-freezing patch failed\n\n"
                "Try restarting the game and injecting again.",
                "Tegridy Farms - Injection Failed",
                MB_ICONERROR | MB_OK);

            return FALSE;  // Abort injection completely
        }

        printf("[SECURITY] All safety checks passed - starting main cheat thread\n");

        // Start main thread only if bypass succeeded
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)start, nullptr, 0, nullptr);
    }

    return TRUE;
}