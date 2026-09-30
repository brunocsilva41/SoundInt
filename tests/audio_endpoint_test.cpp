#include "doctest.h"

#include "audio/endpoint_id.h"
#include "audio/policy_config.h"

#include <string>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <roapi.h>
#include <windows.h>

using soundint::Flow;
using soundint::Role;
using soundint::audio::kInterfaceCapture;
using soundint::audio::kInterfaceRender;
using soundint::audio::kMmdevToken;
using soundint::audio::packDeviceId;
using soundint::audio::unpackDeviceId;

namespace policy = soundint::audio::policy;

namespace
{

// Endpoint sintetico no formato IMMDevice::GetId.
const std::wstring kRaw =
    std::wstring(kMmdevToken) + L"{0.0.0.00000000}.{1f8a5c2e-9d3b-4e71-a6c0-52b7d94e0f11}";

// Sufixos canonicos (o '#' antes do GUID e obrigatorio — verificado contra o
// AudioPolicyConfig real: sem ele o Windows responde E_INVALIDARG).
const std::wstring kSuffixRender = std::wstring(L"#") + kInterfaceRender;
const std::wstring kSuffixCapture = std::wstring(L"#") + kInterfaceCapture;

size_t countOccurrences(const std::wstring& text, const std::wstring& needle)
{
    if (needle.empty())
    {
        return 0;
    }
    size_t count = 0;
    size_t pos = text.find(needle);
    while (pos != std::wstring::npos)
    {
        ++count;
        pos = text.find(needle, pos + needle.size());
    }
    return count;
}

// GUIDs fixas do SDK (mmdeviceapi.h) para nao depender de uuid.lib.
constexpr GUID kClsidMMDeviceEnumerator = {
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}};
constexpr GUID kIidIMMDeviceEnumerator = {
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};

// ---------------------------------------------------------------------------
// GetId do default render; "" se o ambiente nao tiver endpoint (CI/servico).
// ---------------------------------------------------------------------------
std::wstring queryDefaultRenderId()
{
    const HRESULT hrInit = RoInitialize(RO_INIT_MULTITHREADED);
    const bool uninit = SUCCEEDED(hrInit); // S_FALSE tambem exige balancear

    std::wstring result;
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(kClsidMMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                  kIidIMMDeviceEnumerator, reinterpret_cast<void**>(&enumerator));
    if (SUCCEEDED(hr) && enumerator != nullptr)
    {
        IMMDevice* device = nullptr;
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (SUCCEEDED(hr) && device != nullptr)
        {
            LPWSTR id = nullptr;
            if (SUCCEEDED(device->GetId(&id)) && id != nullptr)
            {
                result = id;
                CoTaskMemFree(id);
            }
            device->Release();
        }
        enumerator->Release();
    }

    if (uninit)
    {
        RoUninitialize();
    }
    return result;
}

// ---------------------------------------------------------------------------
// Stream WASAPI muda no processo de teste: o Windows so aceita Set/Get de
// politica persistida para um pid que ja tenha sessao de audio. Sem audio
// device o open() falha e o teste degrada com aviso.
// ---------------------------------------------------------------------------
class SilentSession
{
  public:
    SilentSession() = default;
    ~SilentSession() { close(); }

    SilentSession(const SilentSession&) = delete;
    SilentSession& operator=(const SilentSession&) = delete;

    bool open()
    {
        // queryDefaultRenderId() ja balanceou o RoInitialize; o COM tem de estar
        // ativo de novo para CoCreateInstance/Activate.
        const HRESULT hrInit = RoInitialize(RO_INIT_MULTITHREADED);
        mUninit = SUCCEEDED(hrInit);

        IMMDeviceEnumerator* enumerator = nullptr;
        HRESULT hr =
            CoCreateInstance(kClsidMMDeviceEnumerator, nullptr, CLSCTX_ALL, kIidIMMDeviceEnumerator,
                             reinterpret_cast<void**>(&enumerator));
        if (FAILED(hr))
        {
            close();
            return false;
        }
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mDevice);
        enumerator->Release();
        if (FAILED(hr))
        {
            close();
            return false;
        }

        hr = mDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(&mClient));
        if (FAILED(hr))
        {
            close();
            return false;
        }

        WAVEFORMATEX* mixFormat = nullptr;
        hr = mClient->GetMixFormat(&mixFormat);
        if (FAILED(hr) || mixFormat == nullptr)
        {
            close();
            return false;
        }
        const UINT blockAlign = mixFormat->nBlockAlign;
        hr = mClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1'000'000, 0, mixFormat, nullptr);
        CoTaskMemFree(mixFormat);
        if (FAILED(hr))
        {
            close();
            return false;
        }

        hr = mClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&mRender));
        if (FAILED(hr))
        {
            close();
            return false;
        }

        BYTE* buffer = nullptr;
        if (SUCCEEDED(mRender->GetBuffer(1, &buffer)) && buffer != nullptr)
        {
            for (UINT i = 0; i < blockAlign; ++i)
            {
                buffer[i] = 0; // um frame de silencio
            }
            mRender->ReleaseBuffer(1, 0);
        }
        mClient->Start();
        return true;
    }

  private:
    void close()
    {
        if (mClient != nullptr)
        {
            mClient->Stop();
        }
        if (mRender != nullptr)
        {
            mRender->Release();
            mRender = nullptr;
        }
        if (mClient != nullptr)
        {
            mClient->Release();
            mClient = nullptr;
        }
        if (mDevice != nullptr)
        {
            mDevice->Release();
            mDevice = nullptr;
        }
        if (mUninit)
        {
            mUninit = false;
            RoUninitialize();
        }
    }

    IMMDevice* mDevice = nullptr;
    IAudioClient* mClient = nullptr;
    IAudioRenderClient* mRender = nullptr;
    bool mUninit = false;
};

} // namespace

TEST_CASE("endpoint: constantes do contrato")
{
    CHECK(std::wstring(kMmdevToken) == L"\\\\?\\SWD#MMDEVAPI#");
    CHECK(std::wstring(kInterfaceRender) == L"{e6327cad-dcec-4949-ae8a-991e976a79d2}");
    CHECK(std::wstring(kInterfaceCapture) == L"{2eef81be-33fa-4800-9670-1cd474972c3f}");
}

TEST_CASE("endpoint: pack monta o formato do AudioPolicyConfig")
{
    const std::wstring packedRender = packDeviceId(kRaw, Flow::Render);
    CHECK(packedRender == kRaw + kSuffixRender);
    CHECK(countOccurrences(packedRender, std::wstring(kMmdevToken)) == 1);
    CHECK(countOccurrences(packedRender, kSuffixRender) == 1);

    const std::wstring packedCapture = packDeviceId(kRaw, Flow::Capture);
    CHECK(packedCapture == kRaw + kSuffixCapture);
    CHECK(countOccurrences(packedCapture, kSuffixCapture) == 1);
}

TEST_CASE("endpoint: pack/unpack fazem roundtrip nos dois fluxos")
{
    const std::wstring packedRender = packDeviceId(kRaw, Flow::Render);
    CHECK(unpackDeviceId(packedRender) == kRaw);
    CHECK(packedRender == packDeviceId(unpackDeviceId(packedRender), Flow::Render));

    const std::wstring packedCapture = packDeviceId(kRaw, Flow::Capture);
    CHECK(unpackDeviceId(packedCapture) == kRaw);
    CHECK(packedCapture == packDeviceId(unpackDeviceId(packedCapture), Flow::Capture));
}

TEST_CASE("endpoint: pack e idempotente (nao duplica token nem sufixo)")
{
    const std::wstring once = packDeviceId(kRaw, Flow::Render);
    const std::wstring twice = packDeviceId(once, Flow::Render);
    const std::wstring thrice = packDeviceId(twice, Flow::Render);

    CHECK(twice == once);
    CHECK(thrice == once);
    CHECK(countOccurrences(thrice, std::wstring(kMmdevToken)) == 1);
    CHECK(countOccurrences(thrice, kSuffixRender) == 1);

    // unpack tambem e estavel.
    CHECK(unpackDeviceId(once) == kRaw);
    CHECK(unpackDeviceId(unpackDeviceId(once)) == kRaw);
}

TEST_CASE("endpoint: pack troca o sufixo ao mudar de fluxo")
{
    const std::wstring packedRender = packDeviceId(kRaw, Flow::Render);
    const std::wstring recapture = packDeviceId(packedRender, Flow::Capture);

    CHECK(recapture == kRaw + kSuffixCapture);
    CHECK(countOccurrences(recapture, kSuffixRender) == 0);
    CHECK(countOccurrences(recapture, std::wstring(kMmdevToken)) == 1);

    // E de volta.
    CHECK(packDeviceId(recapture, Flow::Render) == packedRender);
}

TEST_CASE("endpoint: formatos parciais sao completados")
{
    // Sem token, sem sufixo.
    const std::wstring partial = L"{0.0.0.00000000}.{1f8a5c2e-9d3b-4e71-a6c0-52b7d94e0f11}";
    CHECK(packDeviceId(partial, Flow::Render) == kRaw + kSuffixRender);
    CHECK(unpackDeviceId(partial) == kRaw);

    // Sem token, ja com sufixo (id "pela metade").
    CHECK(unpackDeviceId(partial + kSuffixCapture) == kRaw);
    CHECK(packDeviceId(partial + kSuffixCapture, Flow::Render) == kRaw + kSuffixRender);

    // Sufixo sem o '#' separador (formato incompleto) tambem e reconhecido.
    CHECK(unpackDeviceId(partial + std::wstring(kInterfaceRender)) == kRaw);

    // Com token e sufixo do outro fluxo: unpack ignora o fluxo.
    CHECK(unpackDeviceId(kRaw + kSuffixCapture) == kRaw);
}

TEST_CASE("endpoint: sufixo em caixa alta tambem e reconhecido")
{
    const std::wstring upper = kRaw + L"#" + L"{E6327CAD-DCEC-4949-AE8A-991E976A79D2}";
    CHECK(unpackDeviceId(upper) == kRaw);
    CHECK(packDeviceId(upper, Flow::Render) == kRaw + kSuffixRender);

    // Token em caixa baixa e normalizado para o canonico, sem duplicar.
    const std::wstring mixedToken = std::wstring(L"\\\\?\\swd#mmdevapi#") + L"{0.0.0.00000000}";
    CHECK(unpackDeviceId(mixedToken) == std::wstring(kMmdevToken) + L"{0.0.0.00000000}");
}

TEST_CASE("endpoint: casos borda (vazio)")
{
    CHECK(packDeviceId(L"", Flow::Render).empty());
    CHECK(packDeviceId(L"", Flow::Capture).empty());
    CHECK(unpackDeviceId(L"").empty());
}

TEST_CASE("policy: live set/get/clear no PID do processo de teste")
{
    const uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
    const std::wstring rawId = queryDefaultRenderId();

    if (rawId.empty())
    {
        WARN("policy: sem endpoint render disponivel — teste live pulado");
    }
    else
    {
        SilentSession session;
        if (!session.open())
        {
            WARN("policy: ambiente sem audio device (CI) — teste live pulado");
        }
        else
        {
            const bool set = policy::setAppDevice(pid, Flow::Render, Role::Console, rawId);
            if (!set)
            {
                WARN("policy: Set recusado pelo Windows — teste live pulado");
            }
            else
            {
                // O Get devolve o id empacotado; comparar na forma canonica
                // (com token), ja que o GetId pode vir sem o token.
                const std::wstring got = policy::appDevice(pid, Flow::Render, Role::Console);
                CHECK(unpackDeviceId(got) == unpackDeviceId(rawId));

                CHECK(policy::setAppDevice(pid, Flow::Render, Role::Console, L""));
                CHECK(policy::appDevice(pid, Flow::Render, Role::Console) == L"");
            }
        }
    }

    // Deixa a maquina limpa mesmo quando algo falhou no meio.
    policy::clearAllAppDevices();
}
