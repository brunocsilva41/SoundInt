// ============================================================================
// Icone da bandeja (tray) — Track H. Shell_NotifyIconW com
// NOTIFYICON_VERSION_4; o callback chega como WM_APP_TRAY na janela oculta.
// ============================================================================
#pragma once

#include <windows.h>

namespace soundint {

class Tray {
public:
    Tray() = default;
    ~Tray();

    Tray(const Tray&) = delete;
    Tray& operator=(const Tray&) = delete;

    // Adiciona o icone (tooltip "SoundInt") e ativa a versao 4 das notificacoes.
    bool init(HWND hwnd);

    // Remove o icone (shutdown).
    void remove();

    // Consome WM_APP_TRAY. Retorna true se a mensagem foi tratada.
    bool handleMessage(HWND hwnd, WPARAM wParam, LPARAM lParam);

    bool present() const { return added_; }

private:
    void showMenu(HWND hwnd);
    void showRequest(HWND hwnd, WPARAM tag);

    HWND hwnd_ = nullptr;
    bool added_ = false;
};

}  // namespace soundint
