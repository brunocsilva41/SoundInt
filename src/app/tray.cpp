// ============================================================================
// Icone da bandeja — implementacao (ver tray.h).
// ============================================================================
#include "app/tray.h"

#include <shellapi.h>

#include "app/messages.h"
#include "core/log.h"

namespace soundint {
namespace {

constexpr UINT kTrayIconId = 1;
constexpr UINT kCmdOpen = 100;
constexpr UINT kCmdSettings = 101;
constexpr UINT kCmdExit = 102;

constexpr char kChannel[] = "tray";

}  // namespace

Tray::~Tray()
{
    remove();
}

bool Tray::init(HWND hwnd)
{
    if (hwnd == nullptr) {
        return false;
    }
    hwnd_ = hwnd;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);  // Wave 4 troca o icone
    lstrcpynW(nid.szTip, L"SoundInt", static_cast<int>(ARRAYSIZE(nid.szTip)));

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        SI_LOG_ERROR(kChannel, L"falha ao adicionar o icone da bandeja");
        hwnd_ = nullptr;
        return false;
    }
    added_ = true;

    nid.uVersion = NOTIFYICON_VERSION_4;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &nid)) {
        SI_LOG_WARN(kChannel, L"NOTIFYICON_VERSION_4 indisponivel; usando v3");
    }
    return true;
}

void Tray::remove()
{
    if (!added_) {
        return;
    }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);

    added_ = false;
    hwnd_ = nullptr;
}

bool Tray::handleMessage(HWND hwnd, WPARAM /*wParam*/, LPARAM lParam)
{
    if (!added_) {
        return false;
    }

    // NOTIFYICON_VERSION_4: LOWORD(lParam) = notificacao.
    const UINT notification = LOWORD(static_cast<DWORD_PTR>(lParam));
    switch (notification) {
        case WM_LBUTTONDBLCLK:
        case NIN_SELECT:
        case NIN_KEYSELECT:
            showRequest(hwnd, kShowRequestMixer);  // clique esquerdo = mixer
            return true;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            showMenu(hwnd);
            return true;
        default:
            return false;  // balloon/interacao da onda 2
    }
}

void Tray::showMenu(HWND hwnd)
{
    POINT point{};
    if (!GetCursorPos(&point)) {
        point.x = 0;
        point.y = 0;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    AppendMenuW(menu, MF_STRING, kCmdOpen, L"Mixer");
    AppendMenuW(menu, MF_STRING, kCmdSettings, L"Configurações");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdExit, L"Sair");
    SetMenuDefaultItem(menu, kCmdOpen, FALSE);

    const int command =
        TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                         point.x, point.y, hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);  // limpa o resto de desenho do menu

    switch (command) {
        case kCmdOpen:
            showRequest(hwnd, kShowRequestMixer);
            break;
        case kCmdSettings:
            showRequest(hwnd, kShowRequestSettings);
            break;
        case kCmdExit:
            // wParam != 0 = saida explicita (ignora close-to-tray).
            PostMessageW(hwnd, WM_CLOSE, 1, 0);
            break;
        default:
            break;  // menu cancelado (0)
    }
}

void Tray::showRequest(HWND hwnd, WPARAM tag)
{
    PostMessageW(hwnd, WM_APP_SHOW_REQUEST, tag, 0);
}

}  // namespace soundint
