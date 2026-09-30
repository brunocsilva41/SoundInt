// ============================================================================
// Resolucao de tema (Track D) — implementacao.
// ============================================================================
#include "ui/palette.h"

namespace soundint::ui {

namespace {

constexpr wchar_t kPersonalizeKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

}  // namespace

bool isSystemDark()
{
    DWORD value = 1;   // default: claro
    DWORD size = sizeof(value);
    const LSTATUS status =
        RegGetValueW(HKEY_CURRENT_USER, kPersonalizeKey, L"AppsUseLightTheme",
                     RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status != ERROR_SUCCESS) {
        return false;   // chave ausente/ilegivel => tema claro
    }
    return value == 0;
}

tokens::Palette resolvePalette(int theme)
{
    switch (theme) {
    case 1:
        return tokens::lightPalette();
    case 2:
        return tokens::darkPalette();
    default:
        return isSystemDark() ? tokens::darkPalette() : tokens::lightPalette();
    }
}

float windowDpiScale(HWND hwnd)
{
    if (hwnd == nullptr) {
        return 1.0f;
    }
    const UINT dpi = GetDpiForWindow(hwnd);
    if (dpi == 0) {
        return 1.0f;
    }
    return static_cast<float>(dpi) / 96.0f;
}

}  // namespace soundint::ui
