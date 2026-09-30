// ============================================================================
// Atalhos globais (RegisterHotKey) — Track H.
// ============================================================================
#pragma once

#include "core/types.h"

#include <windows.h>

#include <string>
#include <vector>

namespace soundint {

class Hotkeys {
public:
    Hotkeys() = default;
    ~Hotkeys();

    Hotkeys(const Hotkeys&) = delete;
    Hotkeys& operator=(const Hotkeys&) = delete;

    // Registra cada binding habilitado com vk != 0 usando o indice 0..n-1
    // (que e o wParam de WM_HOTKEY). Erros individuais viram log e sao pulados.
    bool registerHotkeys(HWND hwnd, const std::vector<HotkeyBinding>& bindings);

    // Desregistra tudo (shutdown).
    void unregister();

    // Id do binding para o indice recebido em WM_HOTKEY ("" se invalido).
    std::wstring idForIndex(int index) const;

    size_t registeredCount() const { return bound_.size(); }

private:
    HWND hwnd_ = nullptr;
    std::vector<HotkeyBinding> bound_;
};

}  // namespace soundint
