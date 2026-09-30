// ============================================================================
// Router (Track H): consome o EventBus e aplica as decisoes de roteamento
// (regras de chegada, regras por app, perfis, popups) usando IAudioService e
// Store. Sem polling: debounce/cooldown por timers na janela oculta.
//
// A UI da Wave 2 consume a API publica (setRouterEvents/applyRuleNow/
// applyProfileNow/setHotkeyHandler); enquanto nao ha UI os callbacks caem em
// log.
// ============================================================================
#pragma once

#include "app/policy.h"
#include "audio/audio_service.h"
#include "core/event_bus.h"
#include "core/store.h"
#include "core/types.h"

#include <windows.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace soundint {

// Callbacks consumidos pela UI (padrao: log/noop).
struct RouterEvents {
    std::function<void(const DeviceInfo&)> onNewDevice;
    std::function<void(const SessionInfo&)> onNewApp;
    std::function<void(const std::wstring&)> onNotify;
};

// Limites de tempo do roteador (expostos p/ a UI reproduzir a mesma regra).
inline constexpr policy::TimeMs kDeviceDebounceMs = 800;        // coalesce
inline constexpr policy::TimeMs kDeviceCooldownMs = 5 * 60 * 1000;   // popup disp.
inline constexpr policy::TimeMs kNewAppCooldownMs = 10 * 60 * 1000;  // popup app

class Router {
public:
    Router();
    ~Router();

    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    // --- API para a Wave 2 (UI) ---------------------------------------------
    void setRouterEvents(RouterEvents events);
    void setHotkeyHandler(std::function<void(const std::wstring& id)> handler);

    // Aplica imediatamente uma regra por app / um perfil.
    void applyRuleNow(const AppRule& rule);
    void applyProfileNow(const Profile& profile);

    // --- ciclo de vida ------------------------------------------------------
    // Assina o bus e arma os timers na janela oculta.
    void start(audio::IAudioService& audio, core::Store& store, EventBus& bus, HWND window);
    void stop();

    // --- despachos vindos da janela oculta ----------------------------------
    bool handleTimer(UINT_PTR timerId);                 // WM_TIMER
    void handleHotkey(const std::wstring& bindingId);   // WM_HOTKEY

private:
    struct ApplyResult {
        int sessions = 0;        // sessoes roteadas
        bool targetFallback = false;  // alvo inativo => caiu para o default
    };

    void onEvent(const AppEvent& ev);
    void onDeviceEvent(const DeviceEvent& ev);
    void onSessionEvent(const SessionEvent& ev);

    void scheduleDevice(const DeviceEvent& ev);
    void cancelDeviceTimer(const std::wstring& deviceId);
    void evaluateDevice(const std::wstring& deviceId);
    void onDeviceRemoved(const std::wstring& deviceId);

    std::optional<DeviceInfo> resolveDevice(const std::wstring& deviceId) const;
    bool isDeviceActive(const std::wstring& deviceId) const;
    ApplyResult applyRuleToSessions(const AppRule& rule, bool requireActive);
    void notify(const std::wstring& message);

    audio::IAudioService* audio_ = nullptr;
    core::Store* store_ = nullptr;
    EventBus* bus_ = nullptr;
    HWND window_ = nullptr;
    EventBus::SubscriptionId subscription_ = 0;

    RouterEvents events_;
    std::function<void(const std::wstring&)> hotkeyHandler_;

    policy::Debouncer deviceDebounce_{kDeviceDebounceMs};
    policy::CooldownTracker deviceCooldown_{kDeviceCooldownMs};
    policy::CooldownTracker appCooldown_{kNewAppCooldownMs};

    // deviceId <-> id de timer Win32 (coalesce por dispositivo).
    std::unordered_map<std::wstring, UINT_PTR> deviceTimers_;
    std::unordered_map<UINT_PTR, std::wstring> timerKeys_;
    // Ultimo DeviceEvent por dispositivo (fallback se o lookup falhar).
    std::unordered_map<std::wstring, DeviceInfo> staged_;
};

}  // namespace soundint
