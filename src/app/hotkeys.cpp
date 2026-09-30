// ============================================================================
// Atalhos globais — implementacao (ver hotkeys.h).
// ============================================================================
#include "app/hotkeys.h"

#include "core/log.h"

namespace soundint {
namespace {

constexpr char kChannel[] = "hotkey";

// MOD_NOREPEAT: segurar a tecla nao dispara em rajada (Win7+).
constexpr UINT kNoRepeat = 0x4000;

}  // namespace

Hotkeys::~Hotkeys()
{
    unregister();
}

bool Hotkeys::registerHotkeys(HWND hwnd, const std::vector<HotkeyBinding>& bindings)
{
    unregister();
    hwnd_ = hwnd;

    bool any = false;
    for (const HotkeyBinding& binding : bindings) {
        if (!binding.enabled || binding.vk == 0) {
            continue;
        }
        const int index = static_cast<int>(bound_.size());
        const UINT modifiers = binding.modifiers | kNoRepeat;
        if (!RegisterHotKey(hwnd_, index, modifiers, binding.vk)) {
            SI_LOG_WARN(kChannel, L"atalho em uso ou invalido: " + binding.id);
            continue;
        }
        bound_.push_back(binding);
        any = true;
    }

    SI_LOG_INFO(kChannel, L"atalhos registrados: " + std::to_wstring(bound_.size()));
    return any;
}

void Hotkeys::unregister()
{
    for (size_t i = 0; i < bound_.size(); ++i) {
        UnregisterHotKey(hwnd_, static_cast<int>(i));
    }
    bound_.clear();
    hwnd_ = nullptr;
}

std::wstring Hotkeys::idForIndex(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= bound_.size()) {
        return L"";
    }
    return bound_[static_cast<size_t>(index)].id;
}

}  // namespace soundint
