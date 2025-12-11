#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include "Utils/iat_hook.h"      // REPLACED: Advapi32Patch.h with iat_hook.h
#include "Overlay/Overlay.h"
#include "Utils/DX11Hook.h"
#include "Menu/Menu.h"
#include "Menu/MenuRenderer.h"
#include "Features/Features.h"
#include "Core/LocationUnlock.h"
#include "Core/IL2CPP_API.h"


// for logs and config
static char g_dllDirectory[MAX_PATH] = { 0 };

void InitializeDllDirectory()
{
    if (g_dllDirectory[0] != 0)
        return; // already initialized

    HMODULE hModule = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&InitializeDllDirectory,
        &hModule
    );

    char fullPath[MAX_PATH];
    GetModuleFileNameA(hModule, fullPath, MAX_PATH);

    // remove filename, keep directory
    char* lastSlash = strrchr(fullPath, '\\');
    if (lastSlash)
    {
        size_t len = lastSlash - fullPath + 1;
        strncpy_s(g_dllDirectory, fullPath, len);
        g_dllDirectory[len] = '\0';
    }
}


const char* GetDllDirectory()
{
    if (g_dllDirectory[0] == 0)
        InitializeDllDirectory();
    return g_dllDirectory;
}


// =================================================================
// 0. Logging System
// =================================================================
static FILE* g_logFile = nullptr;
static std::mutex g_logMutex;

void WriteLog(const char* format, ...)
{
    std::lock_guard<std::mutex> lock(g_logMutex);

    va_list args;
    va_start(args, format);

    // Write to console
    vprintf(format, args);

    // Write to file
    if (g_logFile)
    {
        va_list args_copy;
        va_copy(args_copy, args);
        vfprintf(g_logFile, format, args_copy);
        fflush(g_logFile); // Flush immediately in case of crash
        va_end(args_copy);
    }

    va_end(args);
}

void InitLogging()
{
    InitializeDllDirectory(); // ensure directory available

    char logPath[MAX_PATH];
    snprintf(logPath, MAX_PATH, "%slog.txt", GetDllDirectory());

    g_logFile = fopen(logPath, "w");
    if (g_logFile)
    {
        WriteLog("[+] Logging initialized: %s\n", logPath);
    }
}


void CloseLogging()
{
    if (g_logFile)
    {
        WriteLog("[*] Closing log file\n");
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}



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

// Offsets for game structures (version 1.0.0.2.42157)
#define OFFSET_RAID_SETTINGS 0xD0
#define OFFSET_SELECTED_LOCATION 0xA8
#define OFFSET_FORCE_ONLINE_RAID 0x32
#define OFFSET_FORCE_OFFLINE_RAID 0x33
#define OFFSET_EFFECTS_CONTROLLER 0x18
#define OFFSET_LOCAL_PLAYER 0x108
#define OFFSET_HEALTH_CONTROLLER 0x940
#define OFFSET_DAMAGE_COEFF 0x34

// =================================================================
// 2. DX11 Present Hook Implementation
// =================================================================

HRESULT __stdcall DX11Hook::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    try
    {
        // First call - initialize overlay
        if (!Overlay::IsInitialized() && !DX11Hook::g_isInitializing.load())
        {
            DX11Hook::g_isInitializing.store(true);
            if (!Overlay::IsInitialized()) // Double-check after acquiring guard
            {
                if (Overlay::Init(pSwapChain))
                {
                    WriteLog("[+] ImGui overlay initialized from Present hook\n");
                }
                else
                {
                    WriteLog("[-] Failed to initialize ImGui overlay\n");
                }
            }
            DX11Hook::g_isInitializing.store(false);
        }

        // Render overlay every frame
        if (Overlay::IsInitialized())
        {
            Overlay::BeginFrame();

            // Render hint text (always visible)
            MenuRenderer::RenderHintText();

            // Render main menu (if open)
            MenuRenderer::RenderMainMenu();

            Overlay::RenderFrame();
        }
    }
    catch (const std::exception& e)
    {
        WriteLog("[!] EXCEPTION IN PRESENT HOOK: %s\n", e.what());
        CloseLogging();
    }
    catch (...)
    {
        WriteLog("[!] UNKNOWN EXCEPTION IN PRESENT HOOK\n");
        CloseLogging();
    }

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
    WriteLog("\n");
    va_end(args);
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

// CFG FILE FOR THE SLEEP, WILL PROBABLY UTILIZE IT FOR MORE LATER

int LoadInitDelaySeconds()
{
    InitializeDllDirectory();

    char cfgPath[MAX_PATH];
    snprintf(cfgPath, MAX_PATH, "%sconfig.ini", GetDllDirectory());

    // If file does NOT exist → create with default 25
    if (GetFileAttributesA(cfgPath) == INVALID_FILE_ATTRIBUTES)
    {
        FILE* f = fopen(cfgPath, "w");
        if (f)
        {
            fprintf(f, "init_delay_seconds=25\n");
            fclose(f);
        }
        return 25;
    }

    // Read existing file
    FILE* f = fopen(cfgPath, "r");
    if (!f) return 25;

    int delay = 25;
    fscanf_s(f, "init_delay_seconds=%d", &delay);
    fclose(f);

    if (delay < 1) delay = 1;
    if (delay > 120) delay = 120;

    return delay;
}





// =================================================================
// 6. Main Execution
// =================================================================

void start()
{
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);

    // Initialize logging
    InitLogging();

    WriteLog("========================================\n");
    WriteLog("  Tegridy Farms - PvE Bypass\n");
    WriteLog("  Version: 1.0.0.2.42157\n");
    WriteLog("========================================\n\n");
    WriteLog("[+] IAT Hook BEService bypass applied successfully\n\n");

    // Patch UnityPlayer BEService check IMMEDIATELY
    WriteLog("[*] Step 1: Patching UnityPlayer BEService check...\n");

    HMODULE hUnityPlayer = nullptr;
    while (!(hUnityPlayer = GetModuleHandleA("UnityPlayer.dll")))
    {
        Sleep(1);
    }
    WriteLog("[+] UnityPlayer.dll found at 0x%p\n", hUnityPlayer);

    uintptr_t beCheckOffset = 0x876C60;
    void* beCheckFunc = (void*)((uintptr_t)hUnityPlayer + beCheckOffset);

    WriteLog("[*] Patching BE Service check at 0x%p...\n", beCheckFunc);

    // Patch: mov al, 1; ret (returns true = service running)
    DWORD oldProtect;
    if (VirtualProtect(beCheckFunc, 3, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        uint8_t* patch = (uint8_t*)beCheckFunc;
        patch[0] = 0xB0;  // mov al, 1
        patch[1] = 0x01;
        patch[2] = 0xC3;  // ret
        VirtualProtect(beCheckFunc, 3, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), beCheckFunc, 3);
        WriteLog("[+] BE Service check BYPASSED!\n");
    }
    else
    {
        WriteLog("[-] Failed to patch UnityPlayer BEService check!\n");
    }

    // Initialize IL2CPP
    WriteLog("\n[*] Step 2: Initializing IL2CPP...\n");

    HMODULE il2cpp = nullptr;
    while (!(il2cpp = GetModuleHandleA("GameAssembly.dll")))
        Sleep(2000);
    WriteLog("[+] GameAssembly.dll Found\n");

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
    il2cpp_object_get_class = (il2cpp_object_get_class_prot)GetProcAddress(il2cpp, "il2cpp_object_get_class");
    il2cpp_class_get_type = (il2cpp_class_get_type_prot)GetProcAddress(il2cpp, "il2cpp_class_get_type");
    il2cpp_type_get_object = (il2cpp_type_get_object_prot)GetProcAddress(il2cpp, "il2cpp_type_get_object");
    il2cpp_class_get_parent = (il2cpp_class_get_parent_prot)GetProcAddress(il2cpp, "il2cpp_class_get_parent");
    il2cpp_class_get_fields = (il2cpp_class_get_fields_prot)GetProcAddress(il2cpp, "il2cpp_class_get_fields");
    il2cpp_field_get_name = (il2cpp_field_get_name_prot)GetProcAddress(il2cpp, "il2cpp_field_get_name");
    il2cpp_field_get_value = (il2cpp_field_get_value_prot)GetProcAddress(il2cpp, "il2cpp_field_get_value");
    il2cpp_method_get_param_count = (il2cpp_method_get_param_count_prot)GetProcAddress(il2cpp, "il2cpp_method_get_param_count");
    il2cpp_object_unbox = (il2cpp_object_unbox_prot)GetProcAddress(il2cpp, "il2cpp_object_unbox");
    il2cpp_string_new = (il2cpp_string_new_prot)GetProcAddress(il2cpp, "il2cpp_string_new");
    il2cpp_method_get_flags = (il2cpp_method_get_flags_prot)GetProcAddress(il2cpp, "il2cpp_method_get_flags");

    int initDelay = LoadInitDelaySeconds();

    WriteLog("[*] Waiting %d seconds for game to fully initialize...\n", initDelay);
    Sleep(initDelay * 1000);

    Il2CppDomain* domain = il2cpp_get_root_domain();
    il2cpp_thread_attach(domain);
    WriteLog("[+] Attached to IL2CPP domain\n");

    Sleep(1000);

    // Find Assembly-CSharp
    WriteLog("\n[*] Step 3: Loading Assembly-CSharp...\n");

    Il2CppImage* image = nullptr;
    while (!(image = il2cpp_image_loaded("Assembly-CSharp.dll")))
        Sleep(2000);
    WriteLog("[+] Assembly-CSharp.dll Found\n");

    Sleep(1000);

    // Patch BattlEye
    WriteLog("\n[*] Step 4: Patching BattlEye initialization...\n");

    auto tarkovApp = il2cpp_class_from_name(image, "EFT", "TarkovApplication");
    if (!tarkovApp)
    {
        WriteLog("[-] TarkovApplication Not Found\n");
        return;
    }
    WriteLog("[+] TarkovApplication Found\n");

    Sleep(1000);

    const Il2CppMethod* battleyeMethod = wait_for_battleye_init(tarkovApp);
    if (battleyeMethod)
    {
        patch_method_ret_safe(battleyeMethod);
        WriteLog("[+] Battleye init patched\n");
    }

    WriteLog("[+] BEClient_x64.dll prevented from loading via IAT hook\n");

    Sleep(1000);

    // Patch error screen
    WriteLog("\n[*] Step 5: Patching error screen...\n");

    auto preloaderUI = il2cpp_class_from_name(image, "EFT.UI", "PreloaderUI");
    if (preloaderUI)
    {
        find_show_error_screen(preloaderUI);
        for (auto m : error_screen_methods)
        {
            patch_method_ret_safe(m);
            WriteLog("[+] ShowErrorScreen --> Patched\n");
        }
    }

    Sleep(1000);

    // Initialize all feature patches (create them, don't enable yet)
    WriteLog("\n[*] Step 6: Initializing feature patches...\n");
    FeaturePatch::InitializeAllPatches(image);

    // Initialize Item Spawner InitLevel hook
    WriteLog("\n[*] Step 6b: Initializing Item Spawner hook...\n");
    ItemSpawner::ProcessQueue(image);

    // CRITICAL: Apply location unlock patches ONCE at startup
    WriteLog("\n[*] Step 6c: Applying permanent location unlock patches...\n");
    LocationUnlock::ApplyUnlockLocations(image);

    Sleep(1000);

    // Initialize DX11 Hook
    WriteLog("\n[*] Step 7: Initializing DX11 hook for ImGui...\n");

    if (DX11Hook::Initialize())
    {
        WriteLog("[+] DX11 hook initialized\n");
    }
    else
    {
        WriteLog("[-] Failed to initialize DX11 hook\n");
    }

    Sleep(1000);
    system("cls");
    // Clear console and show clean header
    WriteLog("\n========================================\n");
    WriteLog("  Tegridy Farms - Active\n");
    WriteLog("========================================\n\n");
    WriteLog("[INFO] Press INSERT to open menu\n");
    WriteLog("[INFO] Press END to exit safely\n\n");

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

    // Track printed instances
    static bool tarkovAppPrinted = false;
    static uintptr_t lastPrintedCM = 0;

    while (!(GetAsyncKeyState(VK_END) & 0x8000))
    {
        // Check menu input (INSERT key)
        Menu::CheckInput();

        // Track TarkovApplication instance (print once)
        if (!g_TarkovApplicationInstance)
        {
            g_TarkovApplicationInstance = get_tarkov_application_instance(tarkovApp);
            if (g_TarkovApplicationInstance && !tarkovAppPrinted)
            {
                WriteLog("[+] TarkovApplication = 0x%llX\n", (uintptr_t)g_TarkovApplicationInstance);
                tarkovAppPrinted = true;
            }
        }

        // Track CameraManager instance (print only when changed)
        void* cm = get_camera_manager_instance(image);
        if (cm && cm != g_CameraManagerInstance)
        {
            g_CameraManagerInstance = cm;
            if ((uintptr_t)cm != lastPrintedCM)
            {
                WriteLog("[+] CameraManager = 0x%llX\n\n", (uintptr_t)cm);
                lastPrintedCM = (uintptr_t)cm;
            }
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

        // Initialize patches when CameraManager exists (game is ready)
        if (cm && !patchesInitialized)
        {
            WriteLog("[*] Game ready - features can now be toggled via menu\n\n");
            printf("[+] Location unlock patches applied (PERMANENT)\n");
            printf("[!] Instructions:\n");
            printf("[!] 2. Go to raid selection - you should see all locations\n");
            printf("[!] 3. If 'Next' button isn't visible, click on any map, click Next and then go Back \n");
            printf("[!] 5. DLC locations (Town, Suburbs) will NOT load - they're not in the game files\n");
            printf("[!] 6. Terminal will load infinitely - the mission doesn't exists\n");
            printf("[!] 7. Ground Zero (the tutorial one) will invalidate raid\n");
            printf("[!] 8. If you have a route undiscovered, you won't be able to load in! (Labirynth is always unlocked)\n");
            patchesInitialized = true;
        }

        // Apply feature changes only when initialized AND something changed
        if (cm && patchesInitialized && featuresChanged)
        {
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

        // CRITICAL: Update selected location when TarkovApplication is ready
        // This monitors for location changes and applies unlock + offline flags
        if (g_TarkovApplicationInstance)
        {
            LocationUnlock::UpdateRaidSelectedLocation(image);
        }

        // Keep ItemSpawner hook alive
        ItemSpawner::ProcessQueue(image);

        Sleep(100);
    }

    // END key pressed
    WriteLog("\n[!] END key detected - terminating process...\n");
    WriteLog("[!] No telemetry will be sent.\n");
    CloseLogging();
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

        // Install IAT Hook for BEService bypass
        InstallBEServiceBypass();

        // Start main thread
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)start, nullptr, 0, nullptr);
    }

    return TRUE;
}