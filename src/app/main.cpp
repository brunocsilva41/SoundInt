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

namespace {

constexpr wchar_t kWindowClass[] = L"SoundIntMainWindow";

// Objetos vivos durante o message loop (o wndProc os referencia).
struct AppShell {
    soundint::Router router;
    soundint::Tray tray;
    soundint::Hotkeys hotkeys;
};

AppShell* g_shell = nullptr;

void shutdownShell()
{
    // Nao postar wakeup em uma janela que esta sendo destruida.
    soundint::events().setWakeup({});

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
            // Wave 2: aqui a UI abre a janela principal / configuracoes.
            if (wParam == soundint::kShowRequestSettings) {
                SI_LOG_INFO("ui", L"pedido de exibicao: configuracoes");
            } else {
                SI_LOG_INFO("ui", L"pedido de exibicao: janela principal");
            }
            return 0;

        case soundint::WM_APP_TRAY:
            if (g_shell != nullptr && g_shell->tray.handleMessage(hwnd, wParam, lParam)) {
                return 0;
            }
            break;

        case WM_CLOSE:
            // Sair da bandeja encerra o app (close-to-tray vem na Wave 2).
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
