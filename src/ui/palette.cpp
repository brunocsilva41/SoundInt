// ============================================================================
// Resolucao de tema (Track D) — implementacao.
// ============================================================================
#include "ui/palette.h"

#include <cwchar>

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

namespace {

// Fator extra do modo de captura (SOUNDINT_CAPTURE_SCALE): multiplica a
// escala para o app renderizar nItido em monitores 100% (equivalente a
// fotografar num monitor HiDPI). Usado apenas pelo script de prints do
// README; sem a variavel, fator = 1 (comportamento normal).
float captureScaleFactor()
{
    static const float factor = []() {
        wchar_t value[16]{};
        const DWORD len =
            GetEnvironmentVariableW(L"SOUNDINT_CAPTURE_SCALE", value, 16);
        if (len == 0 || len >= 16) {
            return 1.0f;
        }
        const float parsed = std::wcstof(value, nullptr);
        return (parsed > 0.25f && parsed <= 4.0f) ? parsed : 1.0f;
    }();
    return factor;
}

}  // namespace

float windowDpiScale(HWND hwnd)
{
    if (hwnd == nullptr) {
        return 1.0f;
    }
    const UINT dpi = GetDpiForWindow(hwnd);
    if (dpi == 0) {
        return 1.0f;
    }
    return (static_cast<float>(dpi) / 96.0f) * captureScaleFactor();
}

}  // namespace soundint::ui
