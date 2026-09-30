// ============================================================================
// i18n - tabelas pt-BR/en compiladas em memoria (sem arquivo em runtime).
// Substitui o stub da Wave 0; a API e contrato (ui/i18n.h) nao muda.
// Chaves desconhecidas devolvem a propria chave; idioma "auto" resolve pelo
// locale do sistema (GetUserDefaultUILanguage).
// ============================================================================
#include "ui/i18n.h"

#include "soundint/version.h"

#include <windows.h>

#include <atomic>
#include <string>
#include <string_view>
#include <unordered_map>

namespace soundint::ui {
namespace {

using Table = std::unordered_map<std::wstring_view, std::wstring>;

// Codigos aceitos por setLanguage (indice -> codigo canonico).
const wchar_t* const kCodes[] = {L"auto", L"pt-BR", L"en"};

// 0 = auto, 1 = pt-BR, 2 = en.
std::atomic<int> g_requested{0};

// Concatena prefixo largo + sufixo estreito (versao de version.h).
std::wstring concat(const wchar_t* prefix, const char* suffix)
{
    std::wstring out = prefix;
    if (suffix != nullptr) {
        while (*suffix != '\0') {
            out.push_back(static_cast<wchar_t>(*suffix));
            ++suffix;
        }
    }
    return out;
}

// Comparacao ASCII case-insensitive de literais terminados em nulo.
bool equalsIgnoreCase(const wchar_t* a, const wchar_t* b)
{
    while (*a != L'\0' && *b != L'\0') {
        wchar_t ca = *a;
        wchar_t cb = *b;
        if (ca >= L'A' && ca <= L'Z') {
            ca = static_cast<wchar_t>(ca - L'A' + L'a');
        }
        if (cb >= L'A' && cb <= L'Z') {
            cb = static_cast<wchar_t>(cb - L'A' + L'a');
        }
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == L'\0' && *b == L'\0';
}

// Indice em kCodes; codigo invalido/nulo -> auto.
int indexOfLanguage(const wchar_t* code)
{
    if (code == nullptr) {
        return 0;
    }
    if (equalsIgnoreCase(code, L"pt-br")) {
        return 1;
    }
    if (equalsIgnoreCase(code, L"en")) {
        return 2;
    }
    if (equalsIgnoreCase(code, L"auto")) {
        return 0;
    }
    return 0;  // invalido -> auto
}

// Tabela pt-BR ----------------------------------------------------------------
const Table& ptTable()
{
    static const Table table = {
        // --- obrigatorias: comuns / modais (outros tracks) -------------------
        {L"common.apply", L"Aplicar"},
        {L"common.cancel", L"Cancelar"},
        {L"common.close", L"Fechar"},

        {L"modal.newDevice.title", L"Novo dispositivo de \u00e1udio"},
        {L"modal.newDevice.primary", L"Tornar sa\u00edda padr\u00e3o"},
        {L"modal.newDevice.secondary", L"Agora n\u00e3o"},
        {L"modal.newDevice.remember", L"Ao conectar, usar automaticamente"},

        {L"modal.newApp.title", L"Novo app come\u00e7ou a tocar"},
        {L"modal.newApp.outputLabel", L"Sa\u00edda"},
        {L"modal.newApp.systemDefault", L"Padr\u00e3o do sistema"},
        {L"modal.newApp.remember", L"Lembrar escolha para este app"},

        {L"mixer.title", L"Mixer"},
        {L"mixer.outputs", L"Sa\u00eddas"},
        {L"mixer.apps", L"Tocando agora"},
        {L"mixer.systemSounds", L"Sons do sistema"},
        {L"mixer.empty", L"Nenhum app est\u00e1 tocando."},
        {L"mixer.defaultOutputLabel", L"Padr\u00e3o do sistema"},
        {L"mixer.volume", L"Volume"},
        {L"mixer.mute", L"Silenciar"},

        // --- janela de settings ---------------------------------------------
        {L"settings.title", L"Configura\u00e7\u00f5es"},
        {L"settings.nav.general", L"Geral"},
        {L"settings.nav.rules", L"Regras"},
        {L"settings.nav.profiles", L"Perfis"},
        {L"settings.nav.hotkeys", L"Atalhos"},
        {L"settings.nav.updates", L"Atualiza\u00e7\u00f5es"},
        {L"settings.nav.about", L"Sobre"},

        // --- pagina Geral ----------------------------------------------------
        {L"settings.general.title", L"Geral"},
        {L"settings.general.popupOnNewDevice",
         L"Avisar quando um novo dispositivo de \u00e1udio for conectado"},
        {L"settings.general.popupOnNewApp", L"Avisar quando um novo app come\u00e7ar a tocar"},
        {L"settings.general.rememberNewAppChoice", L"Lembrar a escolha de sa\u00edda por app"},
        {L"settings.general.closeToTray", L"Fechar vai para a bandeja"},
        {L"settings.general.startWithWindows", L"Iniciar junto com o Windows"},
        {L"settings.general.theme", L"Tema"},
        {L"settings.general.theme.system", L"Sistema"},
        {L"settings.general.theme.light", L"Claro"},
        {L"settings.general.theme.dark", L"Escuro"},
        {L"settings.general.language", L"Idioma"},
        {L"settings.general.language.auto", L"Autom\u00e1tico"},
        {L"settings.general.language.pt", L"Portugu\u00eas"},
        {L"settings.general.language.en", L"English"},

        // --- pagina Regras ---------------------------------------------------
        {L"settings.rules.title", L"Regras por app"},
        {L"settings.rules.hint",
         L"Digite o execut\u00e1vel (ex.: spotify.exe) e clique em Adicionar."},
        {L"settings.rules.placeholder", L"nome do processo"},
        {L"settings.rules.add", L"Adicionar"},
        {L"settings.rules.systemDefault", L"Padr\u00e3o do sistema"},
        {L"settings.rules.remove", L"Remover regra"},
        {L"settings.rules.enabled", L"Regra ativa"},
        {L"settings.rules.empty", L"Nenhuma regra cadastrada."},
        {L"settings.rules.invalid", L"Informe um nome como \"spotify.exe\"."},
        {L"settings.rules.added", L"Regra salva."},

        // --- pagina Perfis ---------------------------------------------------
        {L"settings.profiles.title", L"Perfis"},
        {L"settings.profiles.placeholder", L"nome do perfil"},
        {L"settings.profiles.create", L"Criar"},
        {L"settings.profiles.empty", L"Nenhum perfil criado."},
        {L"settings.profiles.remove", L"Remover perfil"},
        {L"settings.profiles.selectHint", L"Selecione um perfil para editar as rotas."},
        {L"settings.profiles.overrides", L"Rotas do perfil"},
        {L"settings.profiles.overrideProcess", L"Processo"},
        {L"settings.profiles.overrideDevice", L"Dispositivo (deviceId)"},
        {L"settings.profiles.overrideProcessPlaceholder", L"foo.exe"},
        {L"settings.profiles.overrideDevicePlaceholder", L"vazio = padr\u00e3o do sistema"},
        {L"settings.profiles.addOverride", L"Adicionar rota"},
        {L"settings.profiles.removeOverride", L"Remover rota"},
        {L"settings.profiles.invalid", L"Informe um nome para o perfil."},
        {L"settings.profiles.invalidOverride", L"Informe processo (ex.: foo.exe)."},
        {L"settings.profiles.noRouter", L"Roteamento indispon\u00edvel no momento."},

        // --- pagina Atalhos --------------------------------------------------
        {L"settings.hotkeys.title", L"Atalhos globais"},
        {L"settings.hotkeys.record", L"Gravar"},
        {L"settings.hotkeys.recording", L"Pressione a combina\u00e7\u00e3o\u2026"},
        {L"settings.hotkeys.none", L"N\u00e3o atribu\u00eddo"},
        {L"settings.hotkeys.hint", L"Gravando: Esc cancela, Delete desativa o atalho."},
        {L"settings.hotkeys.mixer", L"Mixer"},
        {L"settings.hotkeys.cycleOutput", L"Pr\u00f3xima sa\u00edda"},
        {L"settings.hotkeys.profile", L"Perfil"},
        {L"settings.hotkeys.empty", L"Nenhum atalho configurado."},

        // --- pagina Atualizacoes --------------------------------------------
        {L"settings.updates.title", L"Atualiza\u00e7\u00f5es"},
        {L"settings.updates.autoCheck", L"Verificar atualiza\u00e7\u00f5es automaticamente"},
        {L"settings.updates.betaChannel", L"Receber atualiza\u00e7\u00f5es beta"},
        {L"settings.updates.checkNow", L"Verificar agora"},
        {L"settings.updates.checking", L"Verificando\u2026"},
        {L"settings.updates.available", L"Atualiza\u00e7\u00e3o dispon\u00edvel:"},
        {L"settings.updates.upToDate", L"Voc\u00ea est\u00e1 na vers\u00e3o mais recente."},
        {L"settings.updates.current",
         concat(L"Vers\u00e3o instalada: ", SOUNDINT_VERSION_STRING)},
        {L"settings.updates.noService", L"Verifica\u00e7\u00e3o indispon\u00edvel."},

        // --- pagina Sobre ----------------------------------------------------
        {L"settings.about.title", L"Sobre"},
        {L"settings.about.version", concat(L"Vers\u00e3o ", SOUNDINT_VERSION_STRING)},
        {L"settings.about.license", L"Licen\u00e7a MIT"},
        {L"settings.about.repo", L"Reposit\u00f3rio"},
        {L"settings.about.reportIssue", L"Reportar problema"},
        {L"settings.about.openLogs", L"Abrir pasta de logs"},
        {L"settings.about.credits", L"Cr\u00e9ditos"},
        {L"settings.about.creditsEarTrumpet", L"EarTrumpet \u2014 File-New-Project"},
    };
    return table;
}

// Tabela en -------------------------------------------------------------------
const Table& enTable()
{
    static const Table table = {
        // --- required: common / modals (other tracks) ------------------------
        {L"common.apply", L"Apply"},
        {L"common.cancel", L"Cancel"},
        {L"common.close", L"Close"},

        {L"modal.newDevice.title", L"New audio device"},
        {L"modal.newDevice.primary", L"Set as default output"},
        {L"modal.newDevice.secondary", L"Not now"},
        {L"modal.newDevice.remember", L"Use automatically when connected"},

        {L"modal.newApp.title", L"A new app started playing"},
        {L"modal.newApp.outputLabel", L"Output"},
        {L"modal.newApp.systemDefault", L"System default"},
        {L"modal.newApp.remember", L"Remember choice for this app"},

        {L"mixer.title", L"Mixer"},
        {L"mixer.outputs", L"Outputs"},
        {L"mixer.apps", L"Playing now"},
        {L"mixer.systemSounds", L"System sounds"},
        {L"mixer.empty", L"No app is playing."},
        {L"mixer.defaultOutputLabel", L"System default"},
        {L"mixer.volume", L"Volume"},
        {L"mixer.mute", L"Mute"},

        // --- settings window -------------------------------------------------
        {L"settings.title", L"Settings"},
        {L"settings.nav.general", L"General"},
        {L"settings.nav.rules", L"Rules"},
        {L"settings.nav.profiles", L"Profiles"},
        {L"settings.nav.hotkeys", L"Shortcuts"},
        {L"settings.nav.updates", L"Updates"},
        {L"settings.nav.about", L"About"},

        // --- General page ----------------------------------------------------
        {L"settings.general.title", L"General"},
        {L"settings.general.popupOnNewDevice", L"Notify when a new audio device is connected"},
        {L"settings.general.popupOnNewApp", L"Notify when a new app starts playing"},
        {L"settings.general.rememberNewAppChoice", L"Remember output choice per app"},
        {L"settings.general.closeToTray", L"Close button minimizes to tray"},
        {L"settings.general.startWithWindows", L"Start with Windows"},
        {L"settings.general.theme", L"Theme"},
        {L"settings.general.theme.system", L"System"},
        {L"settings.general.theme.light", L"Light"},
        {L"settings.general.theme.dark", L"Dark"},
        {L"settings.general.language", L"Language"},
        {L"settings.general.language.auto", L"Automatic"},
        {L"settings.general.language.pt", L"Portugu\u00eas"},
        {L"settings.general.language.en", L"English"},

        // --- Rules page ------------------------------------------------------
        {L"settings.rules.title", L"Rules per app"},
        {L"settings.rules.hint",
         L"Type the executable (e.g. spotify.exe) and click Add."},
        {L"settings.rules.placeholder", L"process name"},
        {L"settings.rules.add", L"Add"},
        {L"settings.rules.systemDefault", L"System default"},
        {L"settings.rules.remove", L"Remove rule"},
        {L"settings.rules.enabled", L"Rule enabled"},
        {L"settings.rules.empty", L"No rules yet."},
        {L"settings.rules.invalid", L"Enter a name like \"spotify.exe\"."},
        {L"settings.rules.added", L"Rule saved."},

        // --- Profiles page ---------------------------------------------------
        {L"settings.profiles.title", L"Profiles"},
        {L"settings.profiles.placeholder", L"profile name"},
        {L"settings.profiles.create", L"Create"},
        {L"settings.profiles.empty", L"No profiles yet."},
        {L"settings.profiles.remove", L"Remove profile"},
        {L"settings.profiles.selectHint", L"Select a profile to edit its routes."},
        {L"settings.profiles.overrides", L"Profile routes"},
        {L"settings.profiles.overrideProcess", L"Process"},
        {L"settings.profiles.overrideDevice", L"Device (deviceId)"},
        {L"settings.profiles.overrideProcessPlaceholder", L"foo.exe"},
        {L"settings.profiles.overrideDevicePlaceholder", L"empty = system default"},
        {L"settings.profiles.addOverride", L"Add route"},
        {L"settings.profiles.removeOverride", L"Remove route"},
        {L"settings.profiles.invalid", L"Enter a profile name."},
        {L"settings.profiles.invalidOverride", L"Enter a process (e.g. foo.exe)."},
        {L"settings.profiles.noRouter", L"Routing is unavailable right now."},

        // --- Shortcuts page --------------------------------------------------
        {L"settings.hotkeys.title", L"Global shortcuts"},
        {L"settings.hotkeys.record", L"Record"},
        {L"settings.hotkeys.recording", L"Press the combination\u2026"},
        {L"settings.hotkeys.none", L"Unassigned"},
        {L"settings.hotkeys.hint", L"Recording: Esc cancels, Delete unassigns the shortcut."},
        {L"settings.hotkeys.mixer", L"Mixer"},
        {L"settings.hotkeys.cycleOutput", L"Next output"},
        {L"settings.hotkeys.profile", L"Profile"},
        {L"settings.hotkeys.empty", L"No shortcuts configured."},

        // --- Updates page ----------------------------------------------------
        {L"settings.updates.title", L"Updates"},
        {L"settings.updates.autoCheck", L"Check for updates automatically"},
        {L"settings.updates.betaChannel", L"Receive beta updates"},
        {L"settings.updates.checkNow", L"Check now"},
        {L"settings.updates.checking", L"Checking\u2026"},
        {L"settings.updates.available", L"Update available:"},
        {L"settings.updates.upToDate", L"You are on the latest version."},
        {L"settings.updates.current",
         concat(L"Installed version: ", SOUNDINT_VERSION_STRING)},
        {L"settings.updates.noService", L"Update check unavailable."},

        // --- About page ------------------------------------------------------
        {L"settings.about.title", L"About"},
        {L"settings.about.version", concat(L"Version ", SOUNDINT_VERSION_STRING)},
        {L"settings.about.license", L"MIT License"},
        {L"settings.about.repo", L"Repository"},
        {L"settings.about.reportIssue", L"Report an issue"},
        {L"settings.about.openLogs", L"Open logs folder"},
        {L"settings.about.credits", L"Credits"},
        {L"settings.about.creditsEarTrumpet", L"EarTrumpet \u2014 File-New-Project"},
    };
    return table;
}

// Tabela ativa (auto resolve pelo locale do sistema).
const Table& activeTable()
{
    const int requested = g_requested.load();
    if (requested == 1) {
        return ptTable();
    }
    if (requested == 2) {
        return enTable();
    }
    const WORD lang = GetUserDefaultUILanguage();
    if (lang == 0x416 || lang == 0x816) {  // pt-BR / pt
        return ptTable();
    }
    return enTable();
}

}  // namespace

const wchar_t* tr(const wchar_t* key)
{
    if (key == nullptr) {
        return L"";
    }
    const Table& active = activeTable();
    auto it = active.find(key);
    if (it != active.end()) {
        return it->second.c_str();
    }
    // Chave so existe na outra tabela: usa como fallback.
    const Table& other = (&active == &ptTable()) ? enTable() : ptTable();
    it = other.find(key);
    if (it != other.end()) {
        return it->second.c_str();
    }
    return key;
}

void setLanguage(const wchar_t* languageCode)
{
    g_requested.store(indexOfLanguage(languageCode));
}

const wchar_t* currentLanguage()
{
    return kCodes[g_requested.load()];
}

}  // namespace soundint::ui
