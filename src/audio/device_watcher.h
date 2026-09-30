// ============================================================================
// Watcher de endpoints: IMMNotificationClient -> DeviceEvent no EventBus.
// Nao e header de contrato - Track A (Wave 1).
//
// Regras MSDN respeitadas: nenhum Register/Unregister dentro do callback e
// nenhuma liberacao da ultima referencia ali tambem (as referencias do
// enumerator sao liberadas por stop(), sempre na main thread). Cada callback
// faz AddRef no objeto enquanto executa, entao um stop() concorrente nao
// destrui o watcher no meio de uma notificacao.
// ============================================================================
#pragma once

#include "core/event_bus.h"
#include "core/types.h"

#include <atomic>

#include <mmdeviceapi.h>

namespace soundint::audio
{

class DeviceWatcher final : public IMMNotificationClient
{
  public:
    // O dono (AudioService) cria com ref=1 e chama Release() depois de stop().
    // `enumerator` recebe um AddRef proprio.
    DeviceWatcher(IMMDeviceEnumerator* enumerator, EventBus& bus);
    ~DeviceWatcher();

    DeviceWatcher(const DeviceWatcher&) = delete;
    DeviceWatcher& operator=(const DeviceWatcher&) = delete;

    // Main thread. Register/UnregisterEndpointNotificationCallback.
    bool start();
    void stop();
    bool running() const noexcept { return m_running; }

    // --- IUnknown ---------------------------------------------------------
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // --- IMMNotificationClient (threads de sistema) -----------------------
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override;
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role,
                                                     LPCWSTR defaultDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR deviceId,
                                                     const PROPERTYKEY key) override;

  private:
    // AddRef/Release enquanto um callback executa.
    struct CallbackGuard
    {
        explicit CallbackGuard(DeviceWatcher& watcher) : watcher(watcher) { watcher.AddRef(); }
        ~CallbackGuard() { watcher.Release(); }
        CallbackGuard(const CallbackGuard&) = delete;
        CallbackGuard& operator=(const CallbackGuard&) = delete;
        DeviceWatcher& watcher;
    };

    // Resolve nome/fluxo/estado (best-effort) e publica.
    void publishProbe(DeviceChange what, LPCWSTR deviceId, bool fallbackActive);
    void publishState(LPCWSTR deviceId, DWORD newState);

    IMMDeviceEnumerator* m_enumerator = nullptr; // referencia propria
    EventBus& m_bus;
    std::atomic<ULONG> m_ref{1};
    bool m_running = false; // somente main thread
};

} // namespace soundint::audio
