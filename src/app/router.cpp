// ============================================================================
// Router — implementacao (ver router.h). Sem polling: o EventBus entrega os
// eventos e os timers da janela oculta fazem debounce/cooldown.
// ============================================================================
#include "app/router.h"

#include "core/log.h"

#include <utility>
#include <vector>

namespace soundint {
namespace {

constexpr char kChannel[] = "router";

}  // namespace

Router::Router()
{
    // Callbacks padrao: ate a Wave 2 ligar a UI, tudo vira log.
    events_.onNewDevice = [](const DeviceInfo& device) {
        SI_LOG_INFO(kChannel, L"popup de dispositivo novo: " + device.friendlyName);
    };
    events_.onNewApp = [](const SessionInfo& session) {
        SI_LOG_INFO(kChannel, L"popup de app novo: " + session.displayName);
    };
    events_.onNotify = [](const std::wstring& message) {
        SI_LOG_INFO(kChannel, message);
    };
}

Router::~Router()
{
    stop();
}

// --- API para a Wave 2 -------------------------------------------------------

void Router::setRouterEvents(RouterEvents events)
{
    events_ = std::move(events);
}

void Router::setHotkeyHandler(std::function<void(const std::wstring& id)> handler)
{
    hotkeyHandler_ = std::move(handler);
}

// --- ciclo de vida -----------------------------------------------------------

void Router::start(audio::IAudioService& audio, core::Store& store, EventBus& bus, HWND window)
{
    stop();
    audio_ = &audio;
    store_ = &store;
    bus_ = &bus;
    window_ = window;
    subscription_ = bus.subscribe([this](const AppEvent& ev) { onEvent(ev); });
}

void Router::stop()
{
    if (window_ != nullptr) {
        for (const auto& entry : timerKeys_) {
            KillTimer(window_, entry.first);
        }
    }
    timerKeys_.clear();
    deviceTimers_.clear();
    deviceDebounce_.clear();
    staged_.clear();

    if (subscription_ != 0 && bus_ != nullptr) {
        bus_->unsubscribe(subscription_);
    }
    subscription_ = 0;
    audio_ = nullptr;
    store_ = nullptr;
    bus_ = nullptr;
    window_ = nullptr;
}

// --- despachos da janela -----------------------------------------------------

bool Router::handleTimer(UINT_PTR timerId)
{
    const auto it = timerKeys_.find(timerId);
    if (it == timerKeys_.end()) {
        return false;
    }
    const std::wstring deviceId = it->second;
    if (window_ == nullptr) {
        return false;
    }

    const policy::TimeMs nowMs = policy::now();
    const policy::TimeMs left = deviceDebounce_.remaining(deviceId, nowMs);
    if (left > 0) {
        // Disparo cedo (resolucao do relogio): rearma so o restante.
        SetTimer(window_, timerId, static_cast<UINT>(left), nullptr);
        return true;
    }

    deviceDebounce_.erase(deviceId);
    KillTimer(window_, timerId);
    deviceTimers_.erase(deviceId);
    timerKeys_.erase(it);
    evaluateDevice(deviceId);
    return true;
}

void Router::handleHotkey(const std::wstring& bindingId)
{
    if (bindingId.empty()) {
        return;
    }
    if (hotkeyHandler_) {
        hotkeyHandler_(bindingId);
        return;
    }
    SI_LOG_INFO(kChannel, L"atalho acionado (sem handler de UI): " + bindingId);
}

// --- eventos -----------------------------------------------------------------

void Router::onEvent(const AppEvent& ev)
{
    if (const auto* device = std::get_if<DeviceEvent>(&ev)) {
        onDeviceEvent(*device);
    } else if (const auto* session = std::get_if<SessionEvent>(&ev)) {
        onSessionEvent(*session);
    }
}

void Router::onDeviceEvent(const DeviceEvent& ev)
{
    if (audio_ == nullptr || store_ == nullptr || ev.deviceId.empty()) {
        return;
    }
    // So saidas interessam ao roteamento por app.
    if (ev.flow != Flow::Render) {
        return;
    }

    switch (ev.what) {
        case DeviceChange::Added:
            scheduleDevice(ev);
            break;
        case DeviceChange::StateChanged:
            if (ev.active) {
                scheduleDevice(ev);
            } else {
                cancelDeviceTimer(ev.deviceId);
            }
            break;
        case DeviceChange::Removed:
            cancelDeviceTimer(ev.deviceId);
            onDeviceRemoved(ev.deviceId);
            break;
        case DeviceChange::DefaultChanged:
        default:
            break;
    }
}

void Router::onSessionEvent(const SessionEvent& ev)
{
    if (audio_ == nullptr || store_ == nullptr || ev.what != SessionChange::Created) {
        return;
    }

    const SessionInfo& session = ev.session;
    if (session.processName.empty()) {
        return;  // sem nome nao ha regra nem cooldown por processo
    }

    const std::optional<AppRule> rule = store_->findRule(session.processName);
    const policy::TimeMs nowMs = policy::now();
    const policy::Decision decision = policy::decideSession(
        session, rule, store_->settings().popupOnNewApp, nowMs,
        appCooldown_.last(session.processName), appCooldown_.cooldown());

    switch (decision) {
        case policy::Decision::ApplyRuleSilently:
            if (rule.has_value()) {
                applyRuleToSessions(*rule, /*requireActive=*/false);
            }
            break;
        case policy::Decision::AskPopup:
            appCooldown_.mark(session.processName, nowMs);
            if (events_.onNewApp) {
                events_.onNewApp(session);
            }
            break;
        case policy::Decision::SuppressCooldown:
        case policy::Decision::Ignore:
            break;
    }
}

// --- coalesce por dispositivo ------------------------------------------------

void Router::scheduleDevice(const DeviceEvent& ev)
{
    DeviceInfo staged;
    staged.id = ev.deviceId;
    staged.friendlyName = ev.friendlyName;
    staged.flow = ev.flow;
    staged.active = ev.active;
    staged_[ev.deviceId] = staged;

    if (window_ == nullptr) {
        evaluateDevice(ev.deviceId);  // sem janela nao ha timer: avalia direto
        return;
    }

    (void)deviceDebounce_.poke(ev.deviceId, policy::now());

    const auto it = deviceTimers_.find(ev.deviceId);
    if (it != deviceTimers_.end()) {
        // Mesmo timer reiniciado: a janela recomeca (coalesce).
        SetTimer(window_, it->second, static_cast<UINT>(kDeviceDebounceMs), nullptr);
        return;
    }

    const UINT_PTR timerId = SetTimer(window_, 0, static_cast<UINT>(kDeviceDebounceMs), nullptr);
    if (timerId == 0) {
        SI_LOG_WARN(kChannel, L"SetTimer falhou; dispositivo avaliado sem coalesce");
        evaluateDevice(ev.deviceId);
        return;
    }
    deviceTimers_[ev.deviceId] = timerId;
    timerKeys_[timerId] = ev.deviceId;
}

void Router::cancelDeviceTimer(const std::wstring& deviceId)
{
    const auto it = deviceTimers_.find(deviceId);
    if (it != deviceTimers_.end()) {
        if (window_ != nullptr) {
            KillTimer(window_, it->second);
        }
        timerKeys_.erase(it->second);
        deviceTimers_.erase(it);
    }
    deviceDebounce_.erase(deviceId);
    staged_.erase(deviceId);
}

void Router::evaluateDevice(const std::wstring& deviceId)
{
    if (audio_ == nullptr || store_ == nullptr) {
        return;
    }

    const std::optional<DeviceInfo> info = resolveDevice(deviceId);
    staged_.erase(deviceId);
    if (!info.has_value() || !info->active) {
        return;  // sumiu ou ficou inativo antes da janela fechar
    }

    const ArrivalRule* arrival =
        policy::matchArrival(store_->arrivalRules(), info->friendlyName);
    if (arrival != nullptr) {
        if (arrival->setAsDefault) {
            audio_->setDefaultDevice(deviceId, Flow::Render, Role::Multimedia);
            audio_->setDefaultDevice(deviceId, Flow::Render, Role::Console);
        }
        for (const AppRule& rule : arrival->applyRules) {
            if (rule.enabled) {
                applyRuleToSessions(rule, /*requireActive=*/true);
            }
        }
        SI_LOG_INFO(kChannel, L"regra de chegada aplicada em " + info->friendlyName);
        return;  // regra casou => sem popup
    }

    const policy::TimeMs nowMs = policy::now();
    const policy::Decision decision = policy::decideDevice(
        /*arrivalMatched=*/false, store_->settings().popupOnNewDevice, nowMs,
        deviceCooldown_.last(deviceId), deviceCooldown_.cooldown());
    if (decision != policy::Decision::AskPopup) {
        return;
    }
    deviceCooldown_.mark(deviceId, nowMs);
    if (events_.onNewDevice) {
        events_.onNewDevice(*info);
    }
}

void Router::onDeviceRemoved(const std::wstring& deviceId)
{
    // Regras cujo alvo e o dispositivo que sumiu.
    std::vector<std::wstring> affected;
    auto collect = [&affected, &deviceId](const std::vector<AppRule>& rules) {
        for (const AppRule& rule : rules) {
            if (rule.deviceId == deviceId && !rule.processName.empty()) {
                affected.push_back(rule.processName);
            }
        }
    };

    collect(store_->rules());
    for (const ArrivalRule& arrival : store_->arrivalRules()) {
        collect(arrival.applyRules);
    }
    for (const Profile& profile : store_->profiles()) {
        collect(profile.overrides);
    }
    if (affected.empty()) {
        return;
    }

    int sessions = 0;
    for (const SessionInfo& session : audio_->sessions(Flow::Render)) {
        if (policy::isSystemSounds(session)) {
            continue;
        }
        for (const std::wstring& name : affected) {
            if (policy::iEqual(session.processName, name)) {
                audio_->setAppDevice(session.pid, Flow::Render, Role::Multimedia, L"");
                audio_->setAppDevice(session.pid, Flow::Render, Role::Console, L"");
                ++sessions;
                break;
            }
        }
    }

    if (sessions > 0) {
        notify(L"Dispositivo removido: aplicacoes voltaram ao padrao do sistema.");
    }
}

// --- API publica (Wave 2) ----------------------------------------------------

void Router::applyRuleNow(const AppRule& rule)
{
    if (audio_ == nullptr || store_ == nullptr || rule.processName.empty()) {
        return;
    }
    if (!rule.enabled) {
        notify(L"Regra desabilitada: " + rule.processName);
        return;
    }

    const ApplyResult result = applyRuleToSessions(rule, /*requireActive=*/false);
    if (result.targetFallback) {
        notify(L"Destino indisponivel para " + rule.processName +
               L"; usando o padrao do sistema.");
    }
    if (result.sessions == 0) {
        notify(L"Nenhuma sessao ativa de " + rule.processName +
               L" (regra vale quando o app tocar som).");
    } else if (!result.targetFallback) {
        notify(L"Roteamento aplicado: " + rule.processName);
    }
}

void Router::applyProfileNow(const Profile& profile)
{
    if (audio_ == nullptr || store_ == nullptr) {
        return;
    }

    if (!profile.defaultDeviceId.empty()) {
        if (isDeviceActive(profile.defaultDeviceId)) {
            audio_->setDefaultDevice(profile.defaultDeviceId, Flow::Render, Role::Multimedia);
            audio_->setDefaultDevice(profile.defaultDeviceId, Flow::Render, Role::Console);
        } else {
            notify(L"Perfil " + profile.name + L": dispositivo padrao indisponivel.");
        }
    }

    if (!profile.communicationsDeviceId.empty()) {
        if (isDeviceActive(profile.communicationsDeviceId)) {
            audio_->setDefaultDevice(profile.communicationsDeviceId, Flow::Render,
                                      Role::Communications);
        } else {
            notify(L"Perfil " + profile.name + L": dispositivo de voz indisponivel.");
        }
    }

    for (const AppRule& rule : profile.overrides) {
        applyRuleNow(rule);
    }
    notify(L"Perfil aplicado: " + profile.name);
}

// --- helpers -----------------------------------------------------------------

Router::ApplyResult Router::applyRuleToSessions(const AppRule& rule, bool requireActive)
{
    ApplyResult result;
    if (audio_ == nullptr || rule.processName.empty()) {
        return result;
    }

    std::wstring target = rule.deviceId;
    if (!target.empty() && !isDeviceActive(target)) {
        target.clear();
        result.targetFallback = true;
    }

    for (const SessionInfo& session : audio_->sessions(Flow::Render)) {
        if (policy::isSystemSounds(session)) {
            continue;
        }
        if (!policy::iEqual(session.processName, rule.processName)) {
            continue;
        }
        if (requireActive && !session.active) {
            continue;
        }
        audio_->setAppDevice(session.pid, Flow::Render, Role::Multimedia, target);
        audio_->setAppDevice(session.pid, Flow::Render, Role::Console, target);
        ++result.sessions;
    }
    return result;
}

std::optional<DeviceInfo> Router::resolveDevice(const std::wstring& deviceId) const
{
    if (audio_ == nullptr || deviceId.empty()) {
        return std::nullopt;
    }

    std::optional<DeviceInfo> info;
    for (const DeviceInfo& device : audio_->devices(Flow::Render)) {
        if (device.id == deviceId) {
            info = device;
            break;
        }
    }

    const auto staged = staged_.find(deviceId);
    const bool stagedKnown = staged != staged_.end();

    if (!info.has_value()) {
        if (!stagedKnown) {
            return std::nullopt;  // desconhecido neste ponto do tempo
        }
        info = staged->second;
    } else if (stagedKnown) {
        if (info->friendlyName.empty()) {
            info->friendlyName = staged->second.friendlyName;
        }
        info->active = info->active || staged->second.active;
    }

    if (info->friendlyName.empty()) {
        info->friendlyName = audio_->deviceName(deviceId);
    }
    info->id = deviceId;
    return info;
}

bool Router::isDeviceActive(const std::wstring& deviceId) const
{
    if (audio_ == nullptr || deviceId.empty()) {
        return false;
    }
    for (const DeviceInfo& device : audio_->devices(Flow::Render)) {
        if (device.id == deviceId) {
            return device.active;
        }
    }
    return false;
}

void Router::notify(const std::wstring& message)
{
    if (events_.onNotify) {
        events_.onNotify(message);
    }
}

}  // namespace soundint
