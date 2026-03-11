#include <windows.h>
#include "Dx11App.h"

Dx11App g_app;
int g_width = 1280;
int g_height = 720;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        g_width = LOWORD(lParam);
        g_height = HIWORD(lParam);
        g_app.OnResize(g_width, g_height);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(
    _In_ HINSTANCE hInst,
    _In_opt_ HINSTANCE hPrevInst,
    _In_ PWSTR pCmdLine,
    _In_ int nCmdShow)
{
    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"App";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"App",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_width, g_height,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, SW_SHOW);

    if (!g_app.Init(hwnd, g_width, g_height))
        return -1;

    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_app.Render();
        }
    }

    g_app.Cleanup();
    return 0;
}
