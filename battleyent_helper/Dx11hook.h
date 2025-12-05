#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>

/*
 * MINIMAL DX11 HOOK - NO EXTERNAL LIBRARIES
 *
 * This uses manual vtable hooking - no MinHook, no Kiero needed.
 * Just pure Windows API and pattern scanning.
 */

namespace DX11Hook
{
    // Function pointer type for Present
    typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

    // Original Present function
    static Present_t oPresent = nullptr;

    // Your hooked Present function (defined in dllmain.cpp)
    HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

    // Manual vtable hook
    inline bool HookVTable(void** vtable, int index, void* hookFunction, void** originalFunction)
    {
        DWORD oldProtect;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        *originalFunction = vtable[index];
        vtable[index] = hookFunction;

        VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);
        return true;
    }

    // Get SwapChain from game
    static IDXGISwapChain* GetSwapChain()
    {
        // Create dummy device to get vtable
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = GetForegroundWindow();
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* pSwapChain = nullptr;
        ID3D11Device* pDevice = nullptr;
        ID3D11DeviceContext* pContext = nullptr;

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

        if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            &featureLevel,
            1,
            D3D11_SDK_VERSION,
            &sd,
            &pSwapChain,
            &pDevice,
            nullptr,
            &pContext)))
        {
            return nullptr;
        }

        // Get the vtable
        void** pSwapChainVTable = *(void***)pSwapChain;

        // Hook Present (index 8)
        if (HookVTable(pSwapChainVTable, 8, (void*)hkPresent, (void**)&oPresent))
        {
            std::printf("[+] Present hooked at vtable[8]\n");
        }
        else
        {
            std::printf("[-] Failed to hook Present\n");
        }

        // Clean up dummy objects
        if (pContext) pContext->Release();
        if (pDevice) pDevice->Release();

        return pSwapChain;
    }

    // Initialize the hook
    static bool Initialize()
    {
        std::printf("[*] Initializing DX11 hook...\n");

        IDXGISwapChain* pSwapChain = GetSwapChain();
        if (!pSwapChain)
        {
            std::printf("[-] Failed to get SwapChain\n");
            return false;
        }

        // The hook is already installed by GetSwapChain
        std::printf("[+] DX11 hook initialized\n");

        // Don't release - we need the hook to persist
        // pSwapChain->Release();

        return true;
    }
}