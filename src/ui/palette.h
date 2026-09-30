// ============================================================================
// Resolucao de tema (Track D). O contrato (design_tokens.h) expoe duas
// paletas fixas; aqui elas sao escolhidas a partir da preferencia do usuario
// (Settings::theme: 0 = sistema, 1 = claro, 2 = escuro).
// ============================================================================
#pragma once

#include "ui/design_tokens.h"

namespace soundint::ui {

// Tema escuro do Windows (HKCU ...\Personalize\AppsUseLightTheme).
// Ausencia da chave = claro (default).
bool isSystemDark();

// 0 = segue o sistema, 1 = claro, 2 = escuro; qualquer outro valor = sistema.
tokens::Palette resolvePalette(int theme);

// Escala DPI da janela (1.0 = 96 DPI) — apoio para a onda 2.
float windowDpiScale(HWND hwnd);

}  // namespace soundint::ui
