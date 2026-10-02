// ============================================================================
// Fachada concreta IAudioService: dispositivos (MMDevice) + sessoes
// (SessionWatcher) + notificacoes (DeviceWatcher) + roteamento por app
// (encaminhado para soundint::audio::policy, do Track B).
// ============================================================================
#include "audio/audio_service_impl.h"

#include "audio/audio_util.h"
#include "audio/device_watcher.h"
#include "audio/policy_config.h"
#include "audio/session_watcher.h"

#include <memory>
#include <variant>

#include <mmdeviceapi.h>
#include <objbase.h>

namespace soundint::audio
{

namespace
{

std::wstring readDeviceId(IMMDevice* device)
{
    if (device == nullptr)
    {
        return L"";
    }
    LPWSTR id = nullptr;
    const HRESULT hr = device->GetId(&id);
    std::wstring result;
    if (SUCCEEDED(hr) && id != nullptr) {
        result = id;
    }
    // Livre sempre: free(nullptr) e no-op e cobre o caso raro de a API
    // devolver alocacao junto com falha.
    CoTaskMemFree(id);
    return result;
}

// ---------------------------------------------------------------------------
// setDefaultDevice: IMMDeviceEnumerator NAO expoe setter de default (a anotacao
// do contrato aponta uma API inexistente). A forma usada pelo ecossistema
// (EarTrumpet, Sunshine, AudioEndPointController ...) e a COM nao documentada
// CPolicyConfigClient/IPolicyConfig, disponivel desde o Windows 7.
// Layout da vtable verificado em varias fontes independentes:
//   IUnknown(3) + GetMixFormat + ... + SetPropertyValue + SetDefaultEndpoint.
// ---------------------------------------------------------------------------
struct IPolicyConfig : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&,
                                                       PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&,
                                                       const PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR deviceId, ERole role) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

constexpr GUID kClsidPolicyConfigClient = {
    0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};
constexpr GUID kIidPolicyConfig = {
    0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};

class AudioService final : public IAudioService
{
  public:
    explicit AudioService(EventBus& bus) : m_bus(bus) {}

    ~AudioService() override
    {
        stop();
        releaseCom();
    }

    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    // COM + MMDevice. Falso => o factory devolve nullptr (contrato).
    // Assumir: quem cria e quem destroi usa a mesma thread (main).
    bool initialize()
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(hr))
        {
            // S_FALSE = ja inicializado no mesmo modo: tambem balanceamos.
            m_comOwned = true;
        }
        else if (hr != RPC_E_CHANGED_MODE)
        {
            util::debugFailure(L"CoInitializeEx", hr);
            return false;
        }

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                              __uuidof(IMMDeviceEnumerator),
                              reinterpret_cast<void**>(&m_enumerator));
        if (FAILED(hr) || m_enumerator == nullptr)
        {
            util::debugFailure(L"CoCreateInstance(MMDeviceEnumerator)", hr);
            releaseCom();
            return false;
        }
        return true;
    }

    // --- dispositivos ------------------------------------------------------

    std::vector<DeviceInfo> devices(Flow flow) override
    {
        std::vector<DeviceInfo> out;
        if (m_enumerator == nullptr)
        {
            return out;
        }
        IMMDeviceCollection* collection = nullptr;
        if (FAILED(m_enumerator->EnumAudioEndpoints(util::dataFlowFromFlow(flow),
                                                    DEVICE_STATE_ACTIVE, &collection)) ||
            collection == nullptr)
        {
            return out;
        }

        const std::wstring defaultId = defaultDevice(flow, Role::Multimedia);
        UINT count = 0;
        collection->GetCount(&count);
        out.reserve(count);
        for (UINT index = 0; index < count; ++index)
        {
            IMMDevice* device = nullptr;
            if (FAILED(collection->Item(index, &device)) || device == nullptr)
            {
                continue;
            }
            DeviceInfo info;
            info.flow = flow;
            info.active = true; // filtro DEVICE_STATE_ACTIVE da enumeracao
            info.id = readDeviceId(device);
            info.friendlyName = util::deviceFriendlyName(device);
            info.isDefault =
                !defaultId.empty() && _wcsicmp(info.id.c_str(), defaultId.c_str()) == 0;
            device->Release();
            out.push_back(std::move(info));
        }
        collection->Release();
        return out;
    }

    std::wstring defaultDevice(Flow flow, Role role) override
    {
        if (m_enumerator == nullptr)
        {
            return L"";
        }
        IMMDevice* device = nullptr;
        if (FAILED(m_enumerator->GetDefaultAudioEndpoint(util::dataFlowFromFlow(flow),
                                                         util::eRoleFromRole(role), &device)) ||
            device == nullptr)
        {
            return L"";
        }
        const std::wstring id = readDeviceId(device);
        device->Release();
        return id;
    }

    bool setDefaultDevice(const std::wstring& deviceId, Flow flow, Role role) override
    {
        if (m_enumerator == nullptr || deviceId.empty())
        {
            return false;
        }
        const util::EndpointProbe probe = util::probeEndpoint(m_enumerator, deviceId.c_str());
        if (!probe.found || probe.flow != flow)
        {
            util::debugFailure(L"setDefaultDevice(endpoint invalido/fluxo divergente)",
                               E_INVALIDARG);
            return false;
        }

        IPolicyConfig* config = nullptr;
        HRESULT hr = CoCreateInstance(kClsidPolicyConfigClient, nullptr, CLSCTX_ALL,
                                      kIidPolicyConfig, reinterpret_cast<void**>(&config));
        if (FAILED(hr) || config == nullptr)
        {
            util::debugFailure(L"CoCreateInstance(IPolicyConfig)", hr);
            return false;
        }
        hr = config->SetDefaultEndpoint(deviceId.c_str(), util::eRoleFromRole(role));
        config->Release();
        if (FAILED(hr))
        {
            util::debugFailure(L"IPolicyConfig::SetDefaultEndpoint", hr);
            return false;
        }
        return true;
    }

    std::wstring deviceName(const std::wstring& deviceId) override
    {
        if (m_enumerator == nullptr || deviceId.empty())
        {
            return L"";
        }
        IMMDevice* device = nullptr;
        if (FAILED(m_enumerator->GetDevice(deviceId.c_str(), &device)) || device == nullptr)
        {
            return L"";
        }
        const std::wstring name = util::deviceFriendlyName(device);
        device->Release();
        return name;
    }

    // --- sessoes -----------------------------------------------------------

    std::vector<SessionInfo> sessions(Flow flow) override
    {
        if (m_sessionWatcher == nullptr)
        {
            return {};
        }
        return m_sessionWatcher->sessions(flow);
    }

    bool setSessionVolume(const std::wstring& instanceId, float volume) override
    {
        if (m_sessionWatcher == nullptr)
        {
            return false;
        }
        return m_sessionWatcher->setVolume(instanceId, volume);
    }

    bool setSessionMute(const std::wstring& instanceId, bool mute) override
    {
        if (m_sessionWatcher == nullptr)
        {
            return false;
        }
        return m_sessionWatcher->setMute(instanceId, mute);
    }

    // --- roteamento por app (Track B) ---------------------------------------

    bool setAppDevice(uint32_t pid, Flow flow, Role role, const std::wstring& deviceId) override
    {
        return policy::setAppDevice(pid, flow, role, deviceId);
    }

    std::wstring appDevice(uint32_t pid, Flow flow, Role role) override
    {
        return policy::appDevice(pid, flow, role);
    }

    bool clearAllAppDevices() override { return policy::clearAllAppDevices(); }

    // --- ciclo de vida ------------------------------------------------------

    bool start() override
    {
        if (m_started)
        {
            return true;
        }
        if (m_enumerator == nullptr)
        {
            return false;
        }

        // Assina ANTES de registrar os watchers: nenhum DeviceEvent se perde.
        // O handler roda na main thread (pump) e ressincroniza as sessoes.
        m_deviceSubscription = m_bus.subscribe(
            [this](const AppEvent& event)
            {
                if (std::holds_alternative<DeviceEvent>(event) && m_sessionWatcher != nullptr)
                {
                    m_sessionWatcher->sync();
                }
            });

        m_deviceWatcher = new DeviceWatcher(m_enumerator, m_bus); // ref = 1
        if (!m_deviceWatcher->start())
        {
            m_deviceWatcher->Release();
            m_deviceWatcher = nullptr;
            m_bus.unsubscribe(m_deviceSubscription);
            m_deviceSubscription = 0;
            return false;
        }

        m_sessionWatcher = std::make_shared<SessionWatcher>(m_enumerator, m_bus);
        if (!m_sessionWatcher->start())
        {
            m_deviceWatcher->stop();
            m_deviceWatcher->Release();
            m_deviceWatcher = nullptr;
            m_sessionWatcher.reset();
            m_bus.unsubscribe(m_deviceSubscription);
            m_deviceSubscription = 0;
            return false;
        }

        m_started = true;
        return true;
    }

    void stop() override
    {
        if (m_deviceSubscription != 0)
        {
            m_bus.unsubscribe(m_deviceSubscription);
            m_deviceSubscription = 0;
        }
        if (m_deviceWatcher != nullptr)
        {
            m_deviceWatcher->stop();
            m_deviceWatcher->Release();
            m_deviceWatcher = nullptr;
        }
        if (m_sessionWatcher != nullptr)
        {
            m_sessionWatcher->stop();
            m_sessionWatcher.reset();
        }
        m_started = false;
    }

  private:
    void releaseCom()
    {
        if (m_enumerator != nullptr)
        {
            m_enumerator->Release();
            m_enumerator = nullptr;
        }
        if (m_comOwned)
        {
            CoUninitialize();
            m_comOwned = false;
        }
    }

    EventBus& m_bus;
    IMMDeviceEnumerator* m_enumerator = nullptr;
    DeviceWatcher* m_deviceWatcher = nullptr; // refcount proprio (cria com ref=1)
    std::shared_ptr<SessionWatcher> m_sessionWatcher;
    EventBus::SubscriptionId m_deviceSubscription = 0;
    bool m_comOwned = false;
    bool m_started = false; // somente main thread
};

} // namespace

std::unique_ptr<IAudioService> createAudioServiceImpl(EventBus& bus)
{
    auto service = std::make_unique<AudioService>(bus);
    if (!service->initialize())
    {
        return nullptr;
    }
    return service;
}

} // namespace soundint::audio
