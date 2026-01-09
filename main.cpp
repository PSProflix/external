#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "Memory.h"
#include "Vector.h"
#include "offsets/offsets.hpp"
#include "offsets/client_dll.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

namespace offsets = cs2_dumper::offsets::client_dll;
namespace schema = cs2_dumper::schemas::client_dll;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int main() {
    Memory mem("cs2.exe");
    if (!mem.processHandle) {
        std::cout << "Waiting for CS2..." << std::endl;
        while (!mem.processHandle) {
            mem = Memory("cs2.exe");
            Sleep(1000);
        }
    }
    std::cout << "CS2 Found!" << std::endl;

    uintptr_t client = mem.GetModuleAddress("client.dll");
    if (!client) {
        std::cout << "Failed to find client.dll" << std::endl;
        return 1;
    }

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"Overlay", NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, wc.lpszClassName, L"ESP Overlay", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    const MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    CreateRenderTarget();
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (GetAsyncKeyState(VK_END) & 1) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        uintptr_t localPlayerPawn = mem.Read<uintptr_t>(client + offsets::dwLocalPlayerPawn);
        
        if (localPlayerPawn) {
            int localTeam = mem.Read<int>(localPlayerPawn + schema::C_BaseEntity::m_iTeamNum);
            uintptr_t entityList = mem.Read<uintptr_t>(client + offsets::dwEntityList);
            Matrix4x4 viewMatrix = mem.Read<Matrix4x4>(client + offsets::dwViewMatrix);

            ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 10), ImColor(255, 255, 255), "ESP Active");
            
            ImGui::GetBackgroundDrawList()->AddLine(ImVec2(screenWidth / 2 - 10, screenHeight / 2), ImVec2(screenWidth / 2 + 10, screenHeight / 2), ImColor(255, 255, 255), 1.0f);
            ImGui::GetBackgroundDrawList()->AddLine(ImVec2(screenWidth / 2, screenHeight / 2 - 10), ImVec2(screenWidth / 2, screenHeight / 2 + 10), ImColor(255, 255, 255), 1.0f);

            int entitiesFound = 0;
            int drawnEntities = 0;
            
            Vector3 firstEntityPos = { 0, 0, 0 };
            float firstEntityW = 0;
            Vector2 firstEntityScreen = { 0, 0 };
            uintptr_t firstSceneNode = 0;
            bool firstW2S = false;

            for (int i = 1; i < 512; i++) {
                uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
                if (!listEntry) continue;

                uintptr_t controller = mem.Read<uintptr_t>(listEntry + 120 * (i & 0x1FF));
                if (!controller) continue;

                uintptr_t pawnHandle = mem.Read<uintptr_t>(controller + schema::CCSPlayerController::m_hPlayerPawn);
                if (!pawnHandle) continue;

                uintptr_t pListEntry = mem.Read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
                if (!pListEntry) continue;

                uintptr_t pawn = mem.Read<uintptr_t>(pListEntry + 120 * (pawnHandle & 0x1FF));
                if (!pawn || pawn == localPlayerPawn) continue;

                int health = mem.Read<int>(pawn + schema::C_BaseEntity::m_iHealth);
                entitiesFound++;

                if (health <= 0 || health > 100) continue;

                int team = mem.Read<int>(pawn + schema::C_BaseEntity::m_iTeamNum);
                
                uintptr_t sceneNode = mem.Read<uintptr_t>(pawn + schema::C_BaseEntity::m_pGameSceneNode);
                if (!sceneNode) continue;

                Vector3 origin = mem.Read<Vector3>(sceneNode + schema::CGameSceneNode::m_vecAbsOrigin);
                Vector3 head = origin + Vector3{ 0, 0, 72.f }; 

                if (entitiesFound == 1) {
                    firstSceneNode = sceneNode;
                    firstEntityPos = origin;
                    firstEntityW = viewMatrix.m[3][0] * origin.x + viewMatrix.m[3][1] * origin.y + viewMatrix.m[3][2] * origin.z + viewMatrix.m[3][3];
                }

                Vector2 screenOrigin, screenHead;
                bool success = WorldToScreen(origin, screenOrigin, viewMatrix, screenWidth, screenHeight) &&
                               WorldToScreen(head, screenHead, viewMatrix, screenWidth, screenHeight);

                if (entitiesFound == 1) {
                    firstW2S = success;
                    firstEntityScreen = screenOrigin;
                }

                if (success) {
                    drawnEntities++;
                    float height = screenOrigin.y - screenHead.y;
                    float width = height / 2.0f;
                    float x = screenHead.x - width / 2.0f;

                    ImColor color = (team == localTeam) ? ImColor(0, 255, 0) : ImColor(255, 0, 0);
                    ImGui::GetBackgroundDrawList()->AddRect(ImVec2(x, screenHead.y), ImVec2(x + width, screenOrigin.y), color);
                }
            }
            
            char buf[128];
            sprintf_s(buf, "Entities: %d", entitiesFound);
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 55), ImColor(255, 255, 255), buf);
            sprintf_s(buf, "Drawn: %d", drawnEntities);
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 100), ImColor(255, 255, 255), buf);

            if (entitiesFound > 0) {
                sprintf_s(buf, "SN: 0x%llX | Pos: %.1f %.1f %.1f", firstSceneNode, firstEntityPos.x, firstEntityPos.y, firstEntityPos.z);
                ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 115), ImColor(255, 255, 255), buf);
                sprintf_s(buf, "W: %.2f | OK: %d | LP: 0x%llX", firstEntityW, firstW2S, localPlayerPawn);
                ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 130), ImColor(255, 255, 255), buf);
                sprintf_s(buf, "VM[0][0]: %.2f | EL: 0x%llX", viewMatrix.m[0][0], entityList);
                ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 145), ImColor(255, 255, 255), buf);
            }
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }

    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
