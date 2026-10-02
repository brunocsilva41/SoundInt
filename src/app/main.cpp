// ============================================================================
// Shell do SoundInt (Track H): boot, janela oculta, message loop e despacho
// para Router/Tray/Hotkeys. Substitui o stub da Wave 0.
// ============================================================================
#include <windows.h>

#include <objbase.h>

#include <memory>
#include <string>

#include "app/hotkeys.h"
#include "app/messages.h"
#include "app/router.h"
#include "app/services.h"
#include "app/services_ext.h"
#include "app/single_instance.h"
#include "app/tray.h"
#include "core/log.h"
#include "core/store.h"
#include "soundint/version.h"
#include "ui/i18n.h"
#include "ui/windows/mixer.h"
#include "ui/windows/modal_common.h"
#include "ui/windows/settings/settings_window.h"
#include "ui/windows/window_base.h"
#include "update/update_service.h"

#include <thread>

namespace {

constexpr wchar_t kWindowClass[] = L"SoundIntMainWindow";

// Objetos vivos durante o message loop (o wndProc os referencia).
struct AppShell {
    soundint::Router router;
    soundint::Tray tray;
    soundint::Hotkeys hotkeys;
};

AppShell* g_shell = nullptr;

// Persiste a Store apos mutacao de UI; se o save falhar, marca o shutdown.
void persist()
{
    if (!soundint::store().save()) {
        soundint::setStoreDirty(true);
        SI_LOG_WARN("ui", L"falha ao gravar configuracoes");
    }
}

// Ciclo da saida default (atalho cycleOutput): proxima saida Render ativa.
void cycleOutput()
{
    soundint::audio::IAudioService& audio = soundint::audioService();
    std::vector<soundint::DeviceInfo> outputs = audio.devices(soundint::Flow::Render);
    std::vector<size_t> active;
    size_t current = 0;
    const std::wstring def =
        audio.defaultDevice(soundint::Flow::Render, soundint::Role::Multimedia);
    for (size_t i = 0; i < outputs.size(); ++i) {
        if (outputs[i].active) {
            if (outputs[i].id == def) {
                current = active.size();
            }
            active.push_back(i);
        }
    }
    if (active.size() < 2) {
        SI_LOG_INFO("hotkey", L"cycleOutput: apenas uma saida ativa");
        return;
    }
    const std::wstring& next = outputs[active[(current + 1) % active.size()]].id;
    if (!audio.setDefaultDevice(next, soundint::Flow::Render,
                                 soundint::Role::Multimedia)) {
        SI_LOG_WARN("hotkey", L"cycleOutput: falha ao trocar a saida default");
        return;
    }
    audio.setDefaultDevice(next, soundint::Flow::Render, soundint::Role::Console);
    SI_LOG_INFO("hotkey", L"cycleOutput: saida default alterada");
}

// Ancora do mixer quando o gatilho nao e o clique na tray: canto inferior
// direito da work area (regiao da bandeja do monitor sob o cursor).
POINT mixerAnchorFromCursor()
{
    POINT point{};
    GetCursorPos(&point);
    RECT work{};
    if (soundint::ui::WindowBase::workAreaAt(point, work)) {
        point.x = work.right - 12;
        point.y = work.bottom + 4;  // abaixo da work area => flyout sobe
    }
    return point;
}

// Liga as janelas da UI (modais, mixer, configuracoes) aos servicos do app.
void wireUi(AppShell& shell, HWND window, soundint::core::Store& config)
{
    using soundint::audio::IAudioService;
    using soundint::Role;

    // --- modais de popup ----------------------------------------------------
    soundint::ui::modal::ModalHost modalHost;
    modalHost.outputs = []() {
        return soundint::audioService().devices(soundint::Flow::Render);
    };
    modalHost.defaultOutput = []() {
        return soundint::audioService().defaultDevice(soundint::Flow::Render,
                                                       Role::Multimedia);
    };
    modalHost.currentAppDevice = [](uint32_t pid) {
        return soundint::audioService().appDevice(pid, soundint::Flow::Render,
                                                  Role::Multimedia);
    };
    modalHost.setDefaultDevice = [](const soundint::DeviceInfo& device) {
        IAudioService& audio = soundint::audioService();
        bool ok = audio.setDefaultDevice(device.id, soundint::Flow::Render,
                                          Role::Multimedia);
        ok = audio.setDefaultDevice(device.id, soundint::Flow::Render, Role::Console) &&
             ok;
        return ok;
    };
    modalHost.routeApp = [](uint32_t pid, const std::wstring& deviceId) {
        IAudioService& audio = soundint::audioService();
        bool ok = audio.setAppDevice(pid, soundint::Flow::Render, Role::Multimedia,
                                     deviceId);
        ok = audio.setAppDevice(pid, soundint::Flow::Render, Role::Console, deviceId) &&
             ok;
        return ok;
    };
    modalHost.upsertArrival = [](const soundint::ArrivalRule& rule) {
        soundint::store().upsertArrivalRule(rule);
        persist();
    };
    modalHost.upsertRule = [](const soundint::AppRule& rule) {
        soundint::store().upsertRule(rule);
        persist();
    };
    modalHost.notify = [](const std::wstring& title, const std::wstring& detail) {
        SI_LOG_INFO("ui", title + L": " + detail);
    };
    soundint::ui::modal::init(modalHost);

    // --- mixer flyout -------------------------------------------------------
    soundint::ui::mixer::MixerHost mixerHost;
    mixerHost.outputs = modalHost.outputs;
    mixerHost.defaultOutput = modalHost.defaultOutput;
    mixerHost.sessions = []() {
        return soundint::audioService().sessions(soundint::Flow::Render);
    };
    mixerHost.setDefault = [setDefault = modalHost.setDefaultDevice](
                               const std::wstring& deviceId) {
        soundint::DeviceInfo device;
        device.id = deviceId;
        return setDefault(device);
    };
    mixerHost.routeApp = modalHost.routeApp;
    mixerHost.setVolume = [](const std::wstring& instanceId, float volume) {
        return soundint::audioService().setSessionVolume(instanceId, volume);
    };
    mixerHost.setMute = [](const std::wstring& instanceId, bool mute) {
        return soundint::audioService().setSessionMute(instanceId, mute);
    };
    mixerHost.openSettings = []() { soundint::ui::settings::show(); };
    soundint::ui::mixer::init(soundint::events(), mixerHost);

    // --- configuracoes ------------------------------------------------------
    soundint::ui::settings::SettingsHost settingsHost;
    settingsHost.checkForUpdates =
        [](std::function<void(bool, const std::wstring&)> done) {
            const bool beta = soundint::store().settings().betaChannel;
            std::thread([done, beta]() {
                soundint::update::CheckResult result =
                    soundint::update::checkForUpdates(beta);
                if (!result.error.empty()) {
                    done(false, L"Erro: " + result.error);
                } else if (result.updateAvailable) {
                    done(true, L"v" + result.manifest.version);
                } else {
                    done(false, L"");
                }
            }).detach();
        };
    settingsHost.hotkeysChanged = [&shell, window]() {
        shell.hotkeys.unregister();
        shell.hotkeys.registerHotkeys(window,
                                       soundint::store().settings().hotkeys);
    };
    settingsHost.applyProfile = [&shell](const std::wstring& name) {
        for (const soundint::Profile& profile : soundint::store().profiles()) {
            if (profile.name == name) {
                shell.router.applyProfileNow(profile);
                return;
            }
        }
        SI_LOG_WARN("ui", L"perfil nao encontrado: " + name);
    };
    soundint::ui::settings::init(settingsHost);
    if (!soundint::ui::settings::applyStartup(config.settings().startWithWindows)) {
        SI_LOG_WARN("boot", L"falha ao sincronizar inicializacao com o Windows");
    }

    // --- eventos do router -> modais ---------------------------------------
    soundint::RouterEvents routerEvents;
    routerEvents.onNewDevice = [](const soundint::DeviceInfo& device) {
        soundint::ui::modal::requestNewDevice(device);
    };
    routerEvents.onNewApp = [](const soundint::SessionInfo& session) {
        soundint::ui::modal::requestNewApp(session);
    };
    routerEvents.onNotify = [](const std::wstring& message) {
        SI_LOG_INFO("router", message);
    };
    shell.router.setRouterEvents(routerEvents);

    // --- acoes dos atalhos globais -----------------------------------------
    shell.router.setHotkeyHandler([&shell](const std::wstring& id) {
        if (id == L"mixer") {
            soundint::ui::mixer::toggleAt(mixerAnchorFromCursor());
        } else if (id == L"cycleOutput") {
            cycleOutput();
        } else if (id.rfind(L"profile:", 0) == 0) {
            const int index = _wtoi(id.c_str() + 8);
            const std::vector<soundint::Profile>& profiles =
                soundint::store().profiles();
            if (index >= 0 && static_cast<size_t>(index) < profiles.size()) {
                shell.router.applyProfileNow(profiles[static_cast<size_t>(index)]);
            } else {
                SI_LOG_WARN("hotkey", L"perfil invalido: " + id);
            }
        }
    });
}

void shutdownShell()
{
    // Nao postar wakeup em uma janela que esta sendo destruida.
    soundint::events().setWakeup({});

    // UI primeiro: fecha janelas e desassina o bus antes do audio parar.
    soundint::ui::mixer::shutdown();
    soundint::ui::modal::shutdown();
    soundint::ui::settings::shutdown();

    if (g_shell != nullptr) {
        g_shell->hotkeys.unregister();
        g_shell->tray.remove();
        g_shell->router.stop();
    }

    soundint::audioService().stop();

    if (soundint::storeDirty()) {
        if (!soundint::store().save()) {
            SI_LOG_WARN("boot", L"falha ao gravar as configuracoes no shutdown");
        }
    }

    soundint::setMainMessageWindow(nullptr);
    soundint::log::shutdown();
    PostQuitMessage(0);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case soundint::WM_APP_WAKEUP:
            soundint::events().pump();
            return 0;

        case WM_TIMER:
            if (g_shell != nullptr && g_shell->router.handleTimer(wParam)) {
                return 0;
            }
            break;

        case WM_HOTKEY:
            if (g_shell != nullptr) {
                const std::wstring id = g_shell->hotkeys.idForIndex(static_cast<int>(wParam));
                if (!id.empty()) {
                    g_shell->router.handleHotkey(id);
                }
            }
            return 0;

        case soundint::WM_APP_SHOW_REQUEST:
            if (wParam == soundint::kShowRequestMixer) {
                soundint::ui::mixer::toggleAt(mixerAnchorFromCursor());
                return 0;
            }
            // kShowRequestMain (2a instancia) e kShowRequestSettings abrem
            // a unica janela visivel do app: as configuracoes.
            SI_LOG_INFO("ui", L"pedido de exibicao: configuracoes");
            soundint::ui::settings::show();
            return 0;

        case soundint::WM_APP_TRAY:
            if (g_shell != nullptr && g_shell->tray.handleMessage(hwnd, wParam, lParam)) {
                return 0;
            }
            break;

        case WM_CLOSE:
            // wParam != 0 = saida explicita (menu Sair). Caso contrario
            // respeita closeToTray: apenas esconde (o app segue na bandeja).
            if (wParam == 0 && soundint::store().settings().closeToTray) {
                SI_LOG_INFO("ui", L"WM_CLOSE: seguindo na bandeja (closeToTray)");
                return 0;
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            shutdownShell();
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // 1) instancia unica: a segunda avisa a primeira e sai.
    soundint::SingleInstance single;
    if (!single.acquire()) {
        single.notifyExisting();
        return 0;
    }

    // 2) COM (MTA) para MMDevice/WinRT usados pela fachada de audio.
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(comResult);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        if (comInitialized) {
            CoUninitialize();
        }
        return 1;
    }

    // 3-4) configuracao e log antes de qualquer coisa que possa falhar.
    soundint::core::Store& config = soundint::store();
    const bool loaded = config.load();
    soundint::setStoreDirty(!loaded);

    soundint::log::init(config.logDir());
    SI_LOG_INFO("boot", L"SoundInt " SOUNDINT_VERSION_STRING " iniciando");

    // Idioma antes de qualquer janela/paint (tabelas do Track K).
    soundint::ui::setLanguage(config.settings().language.c_str());

    // 5-6) janela oculta (classe usada pela 2a instancia via FindWindowW).
    HWND window = CreateWindowExW(0, kWindowClass, L"SoundInt " SOUNDINT_VERSION_STRING,
                                  WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, instance,
                                  nullptr);
    if (window == nullptr) {
        SI_LOG_ERROR("boot", L"falha ao criar a janela de mensagens");
        soundint::log::shutdown();
        if (comInitialized) {
            CoUninitialize();
        }
        return 1;
    }

    soundint::setMainMessageWindow(window);
    soundint::events().setWakeup(
        [window]() { PostMessageW(window, soundint::WM_APP_WAKEUP, 0, 0); });
    SI_LOG_INFO("boot", L"versao " SOUNDINT_VERSION_STRING " pronta para o audio");

    // 7-8) fachada de audio: se falhar, o app segue com NullAudioService.
    std::unique_ptr<soundint::audio::IAudioService> audio =
        soundint::audio::createAudioService(soundint::events());
    const bool hasAudio = audio != nullptr;
    soundint::setAudioService(std::move(audio));
    if (!hasAudio) {
        SI_LOG_ERROR("boot", L"createAudioService falhou; seguindo sem audio");
    }
    if (!soundint::audioService().start()) {
        SI_LOG_ERROR("boot", L"falha ao iniciar o servico de audio");
    }

    // 9-11) roteador, bandeja e atalhos.
    AppShell shell;
    g_shell = &shell;
    shell.router.start(soundint::audioService(), config, soundint::events(), window);
    if (!shell.tray.init(window)) {
        SI_LOG_ERROR("boot", L"sem icone na bandeja; o app segue sem tray");
    }
    shell.hotkeys.registerHotkeys(window, config.settings().hotkeys);

    // 12) UI: modais, mixer e configuracoes ligados aos servicos.
    wireUi(shell, window, config);

    // 13) checagem automatica de atualizacoes (best-effort, em background).
    if (config.settings().autoCheckUpdates) {
        std::thread([]() {
            const bool beta = soundint::store().settings().betaChannel;
            soundint::update::CheckResult result =
                soundint::update::checkForUpdates(beta);
            if (result.updateAvailable) {
                SI_LOG_INFO("update", L"disponivel: v" + result.manifest.version);
            } else if (!result.error.empty()) {
                SI_LOG_WARN("update", result.error);
            }
        }).detach();
    }

    SI_LOG_INFO("boot", L"SoundInt ativo");

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_shell = nullptr;
    if (IsWindow(window)) {
        DestroyWindow(window);
    }
    if (comInitialized) {
        CoUninitialize();
    }
    return static_cast<int>(msg.wParam);
}
