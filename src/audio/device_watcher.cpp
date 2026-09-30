// ============================================================================
// Watcher de endpoints: converte as notificacoes do MMDevice em DeviceEvent
// publicados no EventBus. As threads de sistema fazem apenas leitura best-effort
// + publish (thread-safe); nada de Register/Unregister aqui dentro.
// ============================================================================
#include "audio/device_watcher.h"

#include "audio/audio_util.h"

namespace soundint::audio
{

DeviceWatcher::DeviceWatcher(IMMDeviceEnumerator* enumerator, EventBus& bus)
    : m_enumerator(enumerator), m_bus(bus)
{
    if (m_enumerator != nullptr)
    {
        m_enumerator->AddRef();
    }
}

DeviceWatcher::~DeviceWatcher()
{
    stop();
    if (m_enumerator != nullptr)
    {
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
}

bool DeviceWatcher::start()
{
    if (m_running)
    {
        return true;
    }
    if (m_enumerator == nullptr)
    {
        return false;
    }
    const HRESULT hr = m_enumerator->RegisterEndpointNotificationCallback(this);
    if (FAILED(hr))
    {
        util::debugFailure(L"RegisterEndpointNotificationCallback", hr);
        return false;
    }
    m_running = true;
    return true;
}

void DeviceWatcher::stop()
{
    if (!m_running)
    {
        return;
    }
    m_running = false;
    if (m_enumerator != nullptr)
    {
        m_enumerator->UnregisterEndpointNotificationCallback(this);
    }
}

// --- IUnknown --------------------------------------------------------------

HRESULT DeviceWatcher::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr)
    {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient))
    {
        *ppv = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG DeviceWatcher::AddRef()
{
    return m_ref.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG DeviceWatcher::Release()
{
    const ULONG value = m_ref.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (value == 0)
    {
        delete this;
    }
    return value;
}

// --- IMMNotificationClient -------------------------------------------------

HRESULT DeviceWatcher::OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState)
{
    const CallbackGuard guard(*this);
    publishState(deviceId, newState);
    return S_OK;
}

HRESULT DeviceWatcher::OnDeviceAdded(LPCWSTR deviceId)
{
    const CallbackGuard guard(*this);
    publishProbe(DeviceChange::Added, deviceId, /*fallbackActive=*/true);
    return S_OK;
}

HRESULT DeviceWatcher::OnDeviceRemoved(LPCWSTR deviceId)
{
    const CallbackGuard guard(*this);
    publishProbe(DeviceChange::Removed, deviceId, /*fallbackActive=*/false);
    return S_OK;
}

HRESULT DeviceWatcher::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR defaultDeviceId)
{
    const CallbackGuard guard(*this);

    DeviceEvent event;
    event.what = DeviceChange::DefaultChanged;
    event.flow = util::flowFromDataFlow(flow);
    event.role = util::roleFromERole(role);
    event.deviceId = (defaultDeviceId != nullptr) ? defaultDeviceId : L"";
    event.active = !event.deviceId.empty();
    if (!event.deviceId.empty())
    {
        const util::EndpointProbe probe = util::probeEndpoint(m_enumerator, event.deviceId.c_str());
        event.friendlyName = probe.friendlyName;
    }
    m_bus.publish(event);
    return S_OK;
}

HRESULT DeviceWatcher::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY)
{
    // Nome/forma do endpoint mudou: o contrato nao tem evento para isso e o
    // consumidor resolve o nome sob demanda (IAudioService::deviceName).
    const CallbackGuard guard(*this);
    return S_OK;
}

// --- helpers ---------------------------------------------------------------

void DeviceWatcher::publishProbe(DeviceChange what, LPCWSTR deviceId, bool fallbackActive)
{
    if (deviceId == nullptr || deviceId[0] == L'\0')
    {
        return;
    }
    DeviceEvent event;
    event.what = what;
    event.deviceId = deviceId;
    const util::EndpointProbe probe = util::probeEndpoint(m_enumerator, deviceId);
    event.friendlyName = probe.friendlyName;
    event.flow = probe.flow;
    event.active = probe.found ? probe.active : fallbackActive;
    m_bus.publish(event);
}

void DeviceWatcher::publishState(LPCWSTR deviceId, DWORD newState)
{
    if (deviceId == nullptr || deviceId[0] == L'\0')
    {
        return;
    }
    DeviceEvent event;
    event.what = DeviceChange::StateChanged;
    event.deviceId = deviceId;
    event.active = (newState & DEVICE_STATE_ACTIVE) != 0;
    const util::EndpointProbe probe = util::probeEndpoint(m_enumerator, deviceId);
    event.friendlyName = probe.friendlyName;
    event.flow = probe.flow;
    m_bus.publish(event);
}

} // namespace soundint::audio
