// ============================================================================
// Mensagens WM_APP do shell (Track H). Ondas posteriores (UI, modais,
// settings) devem consumir estas constantes em vez de redefinir valores.
// ============================================================================
#pragma once

#include <windows.h>

namespace soundint {

// EventBus aciona a wakeup via PostMessage(hwnd, WM_APP_WAKEUP); a main
// thread despacha com EventBus::pump().
inline constexpr UINT WM_APP_WAKEUP = WM_APP + 1;

// Pedido de exibicao de UI vindo da bandeja, de atalho ou da 2a instancia.
// wParam = tag ShowRequest.
inline constexpr UINT WM_APP_SHOW_REQUEST = WM_APP + 2;

// Callback do icone da bandeja (NIF uCallbackMessage, NOTIFYICON_VERSION_4):
// LOWORD(lParam) = notificacao (NIN_SELECT, WM_CONTEXTMENU, ...).
inline constexpr UINT WM_APP_TRAY = WM_APP + 3;

// Tags de WM_APP_SHOW_REQUEST.
enum ShowRequest : WPARAM {
    kShowRequestMain = 1,      // segunda instancia / "Abrir" -> janela principal
    kShowRequestSettings = 2,  // abrir a tela de configuracoes
    kShowRequestMixer = 3,     // flyout do mixer (clique esquerdo na tray/atalho)
};

}  // namespace soundint
