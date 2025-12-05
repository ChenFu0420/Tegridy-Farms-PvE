#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

// Forward declarations
struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;

namespace Overlay
{
    // Initialize ImGui with DX11 SwapChain
    bool Init(IDXGISwapChain* pSwapChain);

    // Start a new ImGui frame
    void BeginFrame();

    // Render ImGui draw data to screen
    void RenderFrame();

    // Cleanup and shutdown
    void Shutdown();

    // Check if overlay is initialized
    bool IsInitialized();

    // Get game window handle
    HWND GetGameWindow();
}

// Global state
extern bool g_OverlayInitialized;
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern ID3D11RenderTargetView* g_mainRenderTargetView;
extern HWND g_GameWindow;