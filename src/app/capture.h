// ============================================================================
// Modo de captura de telas (--capture <tela>): boot identico ao normal, mas
// ao final abre a janela pedida para o script tools/capture_screens.ps1
// fotografar os prints do README. Nao faz parte do fluxo do usuario.
//
// Alvos: settings:<general|rules|profiles|hotkeys|updates|about> | mixer |
//        modal:newapp | modal:newdevice | traymenu
// ============================================================================
#pragma once

#include <string>

#include <windows.h>

namespace soundint {

// Extrai o alvo de "--capture <tela>" da linha de comando. Vazio = modo normal.
std::wstring parseCaptureTarget(const wchar_t* cmdLine);

// Abre a janela do alvo. Chamado ao fim do boot, na thread da UI, com a
// janela oculta de mensagens do app (tray/hotkeys).
void openCaptureTarget(const std::wstring& target, HWND messageWindow);

}  // namespace soundint
