// ============================================================================
// Roteamento persistido por app via COM nao-documentado
// Windows.Media.Internal.AudioPolicyConfig (contrato: policy_config.h).
//
// Duas IIDs com o MESMO layout de vtable (IInspectable + 19 stubs + 3 reais):
//   21H2+     {ab3d4648-e242-459f-b02f-541c70306324}
//   Downlevel {2a59116d-6c4f-45e0-a74f-707e3fef9258}
// Erros viram false/"" — nada lanca, nada depende do log de outros tracks.
// ============================================================================
#include "audio/policy_config.h"

#include "audio/endpoint_id.h"

#include <cwchar>
#include <mutex>
#include <string>

#include <mmdeviceapi.h>
#include <roapi.h>
#include <winstring.h>

namespace soundint::audio::policy
{

namespace
{

// --- IIDs --------------------------------------------------------------
constexpr GUID kIidPolicy21H2 = {
    0xab3d4648, 0xe242, 0x459f, {0xb0, 0x2f, 0x54, 0x1c, 0x70, 0x30, 0x63, 0x24}};
constexpr GUID kIidPolicyDownlevel = {
    0x2a59116d, 0x6c4f, 0x45e0, {0xa7, 0x4f, 0x70, 0x7e, 0x3f, 0xef, 0x92, 0x58}};

// Layout da vtable: IUnknown(3) + IInspectable(3) + 19 stubs + 3 reais.
// Os stubs existem somente para posicionar os metodos reais; nunca serao
// chamados (se fossem, o processo cairia — o layout real esta verificado).
struct IAudioPolicyConfigFactory : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE Stub01(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub02(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub03(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub04(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub05(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub06(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub07(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub08(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub09(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub10(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub11(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub12(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub13(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub14(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub15(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub16(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub17(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub18(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stub19(void*) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetPersistedDefaultAudioEndpoint(UINT32 pid, EDataFlow flow,
                                                                       ERole role,
                                                                       HSTRING deviceId) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPersistedDefaultAudioEndpoint(UINT32 pid, EDataFlow flow,
                                                                       ERole role,
                                                                       HSTRING* outDeviceId) = 0;
    virtual HRESULT STDMETHODCALLTYPE ClearAllPersistedApplicationDefaultEndpoints() = 0;
};

// --- diagnostico (sem soundint::log: esse track ainda nao existe) ------
void debugFailure(const wchar_t* what, HRESULT hr)
{
    wchar_t buffer[192];
    swprintf_s(buffer, L"SoundInt policy: %ls falhou (hr=0x%08lX)\n", what,
               static_cast<unsigned long>(hr));
    OutputDebugStringW(buffer);
}

// Inicializacao defensiva de COM/WinRT no thread chamador: o facade pode ser
// chamado de qualquer thread; RPC_E_CHANGED_MODE so segue em frente.
class RuntimeInit
{
  public:
    RuntimeInit()
    {
        const HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
        if (SUCCEEDED(hr))
        {
            // S_FALSE tambem exige RoUninitialize balanceador.
            mOwned = true;
        }
        else if (hr != RPC_E_CHANGED_MODE)
        {
            debugFailure(L"RoInitialize", hr);
        }
    }

    ~RuntimeInit()
    {
        if (mOwned)
        {
            RoUninitialize();
        }
    }

    RuntimeInit(const RuntimeInit&) = delete;
    RuntimeInit& operator=(const RuntimeInit&) = delete;

  private:
    bool mOwned = false;
};

// --- factory (cache lazy, thread-safe) --------------------------------
HRESULT createFactory(const GUID& iid, IAudioPolicyConfigFactory** out)
{
    *out = nullptr;

    const wchar_t kClassName[] = L"Windows.Media.Internal.AudioPolicyConfig";
    HSTRING className = nullptr;
    HRESULT hr =
        WindowsCreateString(kClassName, static_cast<UINT32>(std::wcslen(kClassName)), &className);
    if (FAILED(hr))
    {
        return hr;
    }

    void* raw = nullptr;
    hr = RoGetActivationFactory(className, iid, &raw);
    WindowsDeleteString(className);
    if (FAILED(hr))
    {
        return hr;
    }

    *out = static_cast<IAudioPolicyConfigFactory*>(raw);
    return S_OK;
}

IAudioPolicyConfigFactory* acquireFactory()
{
    struct Cache
    {
        std::mutex mutex;
        IAudioPolicyConfigFactory* instance = nullptr;
    };
    static Cache cache;

    std::lock_guard<std::mutex> lock(cache.mutex);
    if (cache.instance != nullptr)
    {
        return cache.instance;
    }

    IAudioPolicyConfigFactory* config = nullptr;
    HRESULT hr = createFactory(kIidPolicy21H2, &config);
    if (FAILED(hr))
    {
        // REGDB_E_CLASSNOTREG / E_NOINTERFACE => variante downlevel.
        hr = createFactory(kIidPolicyDownlevel, &config);
    }
    if (FAILED(hr) || config == nullptr)
    {
        debugFailure(L"RoGetActivationFactory(AudioPolicyConfig)", hr);
        return nullptr;
    }

    cache.instance = config;
    return config;
}

// --- mapeamento dos enums do contrato ---------------------------------
EDataFlow toEDataFlow(Flow flow)
{
    return (flow == Flow::Capture) ? eCapture : eRender;
}

ERole toERole(Role role)
{
    switch (role)
    {
    case Role::Console:
        return eConsole;
    case Role::Multimedia:
        return eMultimedia;
    case Role::Communications:
        return eCommunications;
    }
    return eMultimedia;
}

} // namespace

bool setAppDevice(uint32_t pid, Flow flow, Role role, const std::wstring& deviceId)
{
    RuntimeInit init;
    IAudioPolicyConfigFactory* config = acquireFactory();
    if (config == nullptr)
    {
        return false;
    }

    // Vazio => HSTRING nulo, que limpa a atribuicao persistida (EarTrumpet).
    const std::wstring packed = packDeviceId(deviceId, flow);
    HSTRING hDevice = nullptr;
    if (!packed.empty())
    {
        const HRESULT hrString =
            WindowsCreateString(packed.c_str(), static_cast<UINT32>(packed.size()), &hDevice);
        if (FAILED(hrString))
        {
            debugFailure(L"WindowsCreateString", hrString);
            return false;
        }
    }

    const HRESULT hr = config->SetPersistedDefaultAudioEndpoint(
        static_cast<UINT32>(pid), toEDataFlow(flow), toERole(role), hDevice);
    if (hDevice != nullptr)
    {
        WindowsDeleteString(hDevice);
    }
    if (FAILED(hr))
    {
        debugFailure(L"SetPersistedDefaultAudioEndpoint", hr);
        return false;
    }
    return true;
}

std::wstring appDevice(uint32_t pid, Flow flow, Role role)
{
    RuntimeInit init;
    IAudioPolicyConfigFactory* config = acquireFactory();
    if (config == nullptr)
    {
        return L"";
    }

    HSTRING out = nullptr;
    const HRESULT hr = config->GetPersistedDefaultAudioEndpoint(
        static_cast<UINT32>(pid), toEDataFlow(flow), toERole(role), &out);
    if (FAILED(hr))
    {
        // Falha aqui tambem significa "nenhuma atribuicao": nao polui o debug.
        if (out != nullptr)
        {
            WindowsDeleteString(out);
        }
        return L"";
    }
    if (out == nullptr)
    {
        return L"";
    }

    UINT32 length = 0;
    const PCWSTR buffer = WindowsGetStringRawBuffer(out, &length);
    std::wstring packed;
    if (buffer != nullptr && length > 0)
    {
        packed.assign(buffer, length);
    }
    WindowsDeleteString(out);

    if (packed.empty())
    {
        return L"";
    }
    return unpackDeviceId(packed);
}

bool clearAllAppDevices()
{
    RuntimeInit init;
    IAudioPolicyConfigFactory* config = acquireFactory();
    if (config == nullptr)
    {
        return false;
    }

    const HRESULT hr = config->ClearAllPersistedApplicationDefaultEndpoints();
    if (FAILED(hr))
    {
        debugFailure(L"ClearAllPersistedApplicationDefaultEndpoints", hr);
        return false;
    }
    return true;
}

} // namespace soundint::audio::policy
