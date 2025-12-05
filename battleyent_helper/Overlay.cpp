#include "Overlay.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <iostream>

// Global state
bool g_OverlayInitialized = false;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND g_GameWindow = nullptr;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Original WndProc
static WNDPROC g_OriginalWndProc = nullptr;

// Custom WndProc to intercept input for ImGui
LRESULT CALLBACK CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Let ImGui handle input
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // Pass to original WndProc
    return CallWindowProc(g_OriginalWndProc, hWnd, msg, wParam, lParam);
}

namespace Overlay
{
    void CreateRenderTarget(IDXGISwapChain* pSwapChain)
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
        if (pBackBuffer)
        {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
    }

    void CleanupRenderTarget()
    {
        if (g_mainRenderTargetView)
        {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }
    }

    bool Init(IDXGISwapChain* pSwapChain)
    {
        if (g_OverlayInitialized)
            return true;

        if (!pSwapChain)
        {
            printf("[-] SwapChain is null, cannot initialize overlay\n");
            return false;
        }

        printf("[*] Initializing ImGui overlay...\n");

        // Get Device and Context from SwapChain
        HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
        if (FAILED(hr) || !g_pd3dDevice)
        {
            printf("[-] Failed to get D3D11 Device from SwapChain\n");
            return false;
        }

        g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);

        // Get game window from SwapChain
        DXGI_SWAP_CHAIN_DESC sd;
        pSwapChain->GetDesc(&sd);
        g_GameWindow = sd.OutputWindow;

        if (!g_GameWindow)
        {
            printf("[-] Failed to get game window handle\n");
            return false;
        }

        // Create render target
        CreateRenderTarget(pSwapChain);

        // Setup ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        // Configure ImGui
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr; // Disable imgui.ini

        // Setup ImGui style
        ImGui::StyleColorsDark();

        // Customize colors for Tarkov theme
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 3.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.95f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.5f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.5f, 0.2f, 0.8f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.6f, 0.3f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.1f, 0.4f, 0.1f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.35f, 0.15f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.3f, 0.6f, 0.3f, 1.0f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.2f, 0.5f, 0.2f, 1.0f);

        // Initialize Win32 backend
        if (!ImGui_ImplWin32_Init(g_GameWindow))
        {
            printf("[-] Failed to initialize ImGui Win32 backend\n");
            return false;
        }

        // Initialize DX11 backend
        if (!ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext))
        {
            printf("[-] Failed to initialize ImGui DX11 backend\n");
            ImGui_ImplWin32_Shutdown();
            return false;
        }

        // Hook WndProc to intercept input
        g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(g_GameWindow, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);

        g_OverlayInitialized = true;
        printf("[+] ImGui overlay initialized successfully!\n");
        printf("[*] Press INSERT to toggle menu\n");

        return true;
    }

    void BeginFrame()
    {
        if (!g_OverlayInitialized)
            return;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void RenderFrame()
    {
        if (!g_OverlayInitialized)
            return;

        // Render ImGui
        ImGui::Render();

        // Set render target
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);

        // Render draw data
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void Shutdown()
    {
        if (!g_OverlayInitialized)
            return;

        printf("[*] Shutting down ImGui overlay...\n");

        // Restore original WndProc
        if (g_GameWindow && g_OriginalWndProc)
        {
            SetWindowLongPtr(g_GameWindow, GWLP_WNDPROC, (LONG_PTR)g_OriginalWndProc);
        }

        // Cleanup ImGui
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        // Cleanup render target
        CleanupRenderTarget();

        // Release device and context
        if (g_pd3dDeviceContext)
        {
            g_pd3dDeviceContext->Release();
            g_pd3dDeviceContext = nullptr;
        }

        if (g_pd3dDevice)
        {
            g_pd3dDevice->Release();
            g_pd3dDevice = nullptr;
        }

        g_OverlayInitialized = false;
        printf("[+] ImGui overlay shutdown complete\n");
    }

    bool IsInitialized()
    {
        return g_OverlayInitialized;
    }

    HWND GetGameWindow()
    {
        return g_GameWindow;
    }
}