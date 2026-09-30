// ============================================================================
// Seam de integracao da janela de Configuracoes (Track K).
// Wave 3 (M) liga os callbacks do SettingsHost e chama init/show/hide.
// API estavel: nao muda sem relatorio ao integrador.
// ============================================================================
#pragma once

#include <functional>
#include <string>

namespace soundint::ui::settings {

// Callbacks injetados pelo integrador. Nulos = funcionalidade indisponivel
// (as paginas tratas null com mensagem propria, sem crash).
struct SettingsHost {
    // Assincrono: o integrador chama o callback quando terminar (update module).
    std::function<void(std::function<void(bool found, const std::wstring& info)>)>
        checkForUpdates;
    std::function<void()> hotkeysChanged;  // apos editar atalhos (Wave 3 re-registra)
    std::function<void(const std::wstring& profileName)> applyProfile;  // roteia o perfil
};

// Injeta os callbacks (pode ser chamado antes de show()). Reconfiguravel.
void init(SettingsHost host);

// Abre centralizada na work area do monitor ativo (cria a janela sob demanda).
void show();
void hide();
bool isVisible();
void shutdown();

// HKCU\Software\Microsoft\Windows\CurrentVersion\Run "SoundInt" = caminho do
// exe. Usado pela pagina Geral e pelo boot (Wave 3 chama applyStartup no start
// se settings.startWithWindows). Nao toca no registro: apenas grava/remove.
bool applyStartup(bool enabled);

// --- Auxiliares puros (testados sem janela e sem registro) -------------------

// Monta o valor da chave Run a partir do caminho do exe (entre aspas).
std::wstring startupCommandLine(const std::wstring& exePath);

// Caminho do executavel em uso ja formatado para o registro (GetModuleFileNameW;
// nao grava nada).
std::wstring startupValue();

// Normaliza a chave das regras: trim das pontas + minusculas.
std::wstring normalizeProcessName(const std::wstring& processName);

}  // namespace soundint::ui::settings
