// ============================================================================
// Modo de captura de telas — implementacao (ver capture.h).
// ============================================================================
#include "app/capture.h"

#include <shlobj.h>
#include <windows.h>

#include "app/messages.h"
#include "core/log.h"
#include "core/types.h"
#include "ui/windows/mixer.h"
#include "ui/windows/modal_common.h"
#include "ui/windows/settings/settings_window.h"

namespace soundint {
namespace {

constexpr char kChannel[] = "capture";

// Pagina de settings por nome (ordem de createPages()/kNavSpecs).
bool settingsPageIndex(const std::wstring& name, size_t& index)
{
    struct PageSpec {
        const wchar_t* name;
        size_t index;
    };
    static const PageSpec kPages[] = {
        {L"general", 0}, {L"rules", 1},  {L"profiles", 2},
        {L"hotkeys", 3},  {L"updates", 4}, {L"about", 5},
    };
    for (const PageSpec& page : kPages) {
        if (name == page.name) {
            index = page.index;
            return true;
        }
    }
    return false;
}

// Caminho real do Spotify na maquina (icone bonito no modal); vazio = padrao.
std::wstring findSpotifyPath()
{
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT,
                                      nullptr, &appData))) {
        std::wstring path(appData);
        CoTaskMemFree(appData);
        path += L"\\Spotify\\Spotify.exe";
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
    }
    return {};
}

}  // namespace

std::wstring parseCaptureTarget(const wchar_t* cmdLine)
{
    if (cmdLine == nullptr) {
        return {};
    }
    const std::wstring line = cmdLine;
    const size_t flag = line.find(L"--capture");
    if (flag == std::wstring::npos) {
        return {};
    }
    size_t start = line.find_first_not_of(L" \"", flag + 9);
    if (start == std::wstring::npos) {
        return {};
    }
    const size_t end = line.find_first_of(L" \"", start);
    return line.substr(start, end == std::wstring::npos ? std::wstring::npos
                                                        : end - start);
}

void openCaptureTarget(const std::wstring& target, HWND messageWindow)
{
    SI_LOG_INFO(kChannel, L"abrindo tela para captura: " + target);

    // Configuracoes: abre a janela ja na pagina pedida.
    if (target.rfind(L"settings:", 0) == 0) {
        size_t index = 0;
        if (settingsPageIndex(target.substr(9), index)) {
            soundint::ui::settings::showPage(index);
        }
        return;
    }

    // Mixer: ancora deterministica no canto inferior direito da area util.
    if (target == L"mixer") {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        POINT anchor{};
        anchor.x = work.right - 60;
        anchor.y = work.bottom - 60;
        soundint::ui::mixer::toggleAt(anchor);
        return;
    }

    // Modais: dados de demonstracao (saidas reais da maquina no dropdown).
    if (target == L"modal:newapp") {
        SessionInfo session;
        session.instanceId = L"capture-demo-session";
        session.pid = 4242;
        session.processName = L"spotify.exe";
        session.processPath = findSpotifyPath();
        session.displayName = L"Spotify";
        session.active = true;
        session.volume = 0.8f;
        soundint::ui::modal::requestNewApp(session);
        return;
    }
    if (target == L"modal:newdevice") {
        DeviceInfo device;
        device.id = L"capture-demo-device";
        device.friendlyName = L"WH-1000XM5 (Fone Bluetooth)";
        device.active = true;
        soundint::ui::modal::requestNewDevice(device);
        return;
    }

    // Menu da bandeja: cursor perto do relogio + a mesma mensagem que a tray
    // envia no clique direito (showMenu usa GetCursorPos como ancora).
    if (target == L"traymenu") {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        SetCursorPos(work.right - 260, work.bottom - 8);
        PostMessageW(messageWindow, soundint::WM_APP_TRAY, 0,
                     static_cast<LPARAM>(WM_CONTEXTMENU));
        return;
    }

    SI_LOG_WARN(kChannel, L"alvo de captura desconhecido: " + target);
}

}  // namespace soundint
