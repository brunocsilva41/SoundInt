// ============================================================================
// Shell minimo da Wave 0 — apenas para provar o pipeline de build.
// O Track H (app-shell) substitui integralmente este arquivo.
// ============================================================================
#include <windows.h>

#include "soundint/version.h"

namespace {

constexpr wchar_t kWindowClass[] = L"SoundIntMainWindow";
constexpr UINT WM_APP_WAKEUP = WM_APP + 1;  // wakeup do EventBus

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_APP_WAKEUP:
            // Track H: EventBus::pump() aqui.
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    HWND window = CreateWindowExW(0, kWindowClass, L"SoundInt " SOUNDINT_VERSION_STRING,
                                  0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (window) {
        DestroyWindow(window);
    }
    return 0;
}
