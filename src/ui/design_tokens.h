// ============================================================================
// CONTRACT — design tokens (paleta, espacamento, tipografia, icones).
// Header-only, CONGELADO na Wave 0. O tema (light/dark) e resolvido pelos
// consumidores chamando lightPalette()/darkPalette().
// ============================================================================
#pragma once

#include "ui/renderer.h"

namespace soundint::ui::tokens {

// ---------------------------------------------------------------------------
// Tipografia
// ---------------------------------------------------------------------------
inline constexpr wchar_t kFontUi[] = L"Segoe UI";
inline constexpr wchar_t kFontIcons[] = L"Segoe Fluent Icons";

constexpr float kFontCaption = 12.f;
constexpr float kFontBody = 14.f;
constexpr float kFontSubtitle = 16.f;
constexpr float kFontTitle = 20.f;

constexpr int kWeightRegular = 400;
constexpr int kWeightSemibold = 600;

// ---------------------------------------------------------------------------
// Espacamento e forma (grid 4px)
// ---------------------------------------------------------------------------
constexpr float kGrid = 4.f;
constexpr float kSpace1 = 4.f;
constexpr float kSpace2 = 8.f;
constexpr float kSpace3 = 12.f;
constexpr float kSpace4 = 16.f;
constexpr float kSpace5 = 24.f;
constexpr float kSpace6 = 32.f;

constexpr float kRadiusSm = 4.f;
constexpr float kRadiusMd = 8.f;
constexpr float kRadiusLg = 12.f;

constexpr float kControlHeight = 32.f;
constexpr float kRowHeight = 40.f;
constexpr float kWindowMargin = 16.f;

// ---------------------------------------------------------------------------
// Movimento
// ---------------------------------------------------------------------------
constexpr int kAnimMs = 120;

// ---------------------------------------------------------------------------
// Icones (Segoe Fluent Icons)
// ---------------------------------------------------------------------------
inline constexpr wchar_t kIconSpeaker[] = L"\uE767";        // Volume
inline constexpr wchar_t kIconSpeakerMute[] = L"\uE74F";    // Mute
inline constexpr wchar_t kIconHeadphones[] = L"\uE7F1";     // Headphones
inline constexpr wchar_t kIconSettings[] = L"\uE713";       // Settings
inline constexpr wchar_t kIconCheck[] = L"\uE73E";          // CheckMark
inline constexpr wchar_t kIconClose[] = L"\uE711";          // Cancel
inline constexpr wchar_t kIconChevronDown[] = L"\uE70D";    // ChevronDown
inline constexpr wchar_t kIconRefresh[] = L"\uE72C";        // Refresh
inline constexpr wchar_t kIconWarning[] = L"\uE7BA";        // Warning
inline constexpr wchar_t kIconInfo[] = L"\uE946";           // Info
inline constexpr wchar_t kIconDownload[] = L"\uE896";       // Download
inline constexpr wchar_t kIconKeyboard[] = L"\uE765";       // Keyboard

// ---------------------------------------------------------------------------
// Paletas
// ---------------------------------------------------------------------------
struct Palette {
    Color windowBg;
    Color surface;
    Color surfaceAlt;
    Color border;
    Color textPrimary;
    Color textSecondary;
    Color accent;
    Color accentFg;
    Color danger;
    Color success;
    Color hover;
    Color pressed;
    Color overlayScrim;   // fundo do modal
};

inline Palette lightPalette()
{
    Palette p;
    p.windowBg = Color::rgb(249, 249, 249);
    p.surface = Color::rgb(255, 255, 255);
    p.surfaceAlt = Color::rgb(243, 243, 243);
    p.border = Color::rgb(214, 214, 214);
    p.textPrimary = Color::rgb(24, 24, 24);
    p.textSecondary = Color::rgb(96, 96, 96);
    p.accent = Color::rgb(0, 103, 192);
    p.accentFg = Color::rgb(255, 255, 255);
    p.danger = Color::rgb(196, 43, 28);
    p.success = Color::rgb(16, 124, 16);
    p.hover = Color::rgb(0, 0, 0, 26);    // 10% preto
    p.pressed = Color::rgb(0, 0, 0, 51);  // 20% preto
    p.overlayScrim = Color::rgb(0, 0, 0, 90);
    return p;
}

inline Palette darkPalette()
{
    Palette p;
    p.windowBg = Color::rgb(32, 32, 32);
    p.surface = Color::rgb(44, 44, 44);
    p.surfaceAlt = Color::rgb(56, 56, 56);
    p.border = Color::rgb(74, 74, 74);
    p.textPrimary = Color::rgb(243, 243, 243);
    p.textSecondary = Color::rgb(166, 166, 166);
    p.accent = Color::rgb(76, 160, 255);
    p.accentFg = Color::rgb(16, 16, 16);
    p.danger = Color::rgb(255, 99, 71);
    p.success = Color::rgb(126, 231, 126);
    p.hover = Color::rgb(255, 255, 255, 26);
    p.pressed = Color::rgb(255, 255, 255, 51);
    p.overlayScrim = Color::rgb(0, 0, 0, 140);
    return p;
}

}  // namespace soundint::ui::tokens
