// ============================================================================
// CONTRACT — tipos centrais do SoundInt. CONGELADO na Wave 0.
// Alteracoes aqui precisam ser reportadas ao integrador; nao edite por conta.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace soundint {

enum class Flow : uint8_t { Render, Capture };

enum class Role : uint8_t { Console, Multimedia, Communications };

// ---------------------------------------------------------------------------
// Dispositivo de ponta (endpoint)
// ---------------------------------------------------------------------------
struct DeviceInfo {
    std::wstring id;          // IMMDevice::GetId (sem token MMDEVAPI)
    std::wstring friendlyName;
    Flow flow = Flow::Render;
    bool active = false;
    bool isDefault = false;   // default do papel Role::Multimedia
};

// ---------------------------------------------------------------------------
// Sessao de audio de um app (1 por processo/dispositivo)
// ---------------------------------------------------------------------------
struct SessionInfo {
    std::wstring instanceId;   // GetSessionInstanceIdentifier (unico)
    uint32_t pid = 0;          // 0 = sons do sistema
    std::wstring processName;  // minusculo, ex. L"spotify.exe"
    std::wstring processPath;  // caminho da imagem (pode ser vazio)
    std::wstring displayName;  // nome exibido (fallback: processName)
    std::wstring deviceId;     // endpoint onde a sessao vive
    bool systemSounds = false;
    bool active = false;       // AudioSessionStateActive
    float volume = 1.0f;       // 0..1
    bool muted = false;
};

// ---------------------------------------------------------------------------
// Regras e perfis
// ---------------------------------------------------------------------------
struct AppRule {
    bool enabled = true;
    std::wstring processName;  // normalizado minusculo "foo.exe"
    std::wstring deviceId;     // alvo; vazio => segue o default do sistema
};

// Regra de chegada de dispositivo: "ao conectar X => default + regras"
struct ArrivalRule {
    bool enabled = true;
    std::wstring deviceNamePattern;       // substring case-insensitive do friendly name
    bool setAsDefault = false;
    std::vector<AppRule> applyRules;      // aplicadas quando o dispositivo fica ativo
};

struct Profile {
    std::wstring name;
    std::wstring defaultDeviceId;          // Role::Multimedia/Console
    std::wstring communicationsDeviceId;   // opcional (Role::Communications)
    std::vector<AppRule> overrides;        // overrides por app do perfil
};

// Atalho global (RegisterHotKey). Valores de modifiers = winuser.h:
// MOD_ALT=0x0001 MOD_CONTROL=0x0002 MOD_SHIFT=0x0004 MOD_WIN=0x0008.
struct HotkeyBinding {
    bool enabled = true;
    std::wstring id;         // "mixer" | "cycleOutput" | "profile:<indice>"
    unsigned modifiers = 0;  // mascara MOD_*
    unsigned vk = 0;         // codigo VK_* (0 = nao atribuido)
};

struct Settings {
    bool popupOnNewDevice = true;
    bool popupOnNewApp = true;
    bool rememberNewAppChoice = true;      // estado padrao do checkbox "Lembrar"
    bool startWithWindows = false;
    bool closeToTray = true;
    bool autoCheckUpdates = true;
    bool betaChannel = false;
    std::wstring language = L"auto";       // L"auto" | L"pt-BR" | L"en"
    int theme = 0;                         // 0 = sistema, 1 = claro, 2 = escuro
    std::vector<HotkeyBinding> hotkeys;    // padroes preenchidos pelo Store
};

// ---------------------------------------------------------------------------
// Eventos
// ---------------------------------------------------------------------------
enum class DeviceChange : uint8_t { Added, Removed, StateChanged, DefaultChanged };
enum class SessionChange : uint8_t { Created, Removed, VolumeChanged, StateChanged };

struct DeviceEvent {
    DeviceChange what = DeviceChange::Added;
    Flow flow = Flow::Render;
    Role role = Role::Multimedia;  // sensivel apenas em DefaultChanged
    std::wstring deviceId;
    std::wstring friendlyName;     // melhor esforco (vazio se desconhecido)
    bool active = false;           // StateChanged: novo estado
};

struct SessionEvent {
    SessionChange what = SessionChange::Created;
    SessionInfo session;
};

using AppEvent = std::variant<DeviceEvent, SessionEvent>;

}  // namespace soundint
