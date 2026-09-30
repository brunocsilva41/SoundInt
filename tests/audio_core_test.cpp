// ============================================================================
// Suite do Track A: helpers puros (audio_util) + smoke test da fachada
// IAudioService (COM real, best-effort: pula com WARN se o ambiente nao tiver
// audio). Nao altera o default nem o roteamento da maquina.
// ============================================================================
#include "doctest.h"

#include "audio/audio_service.h"
#include "audio/audio_util.h"
#include "core/event_bus.h"
#include "core/types.h"

#include <cmath>
#include <string>
#include <vector>

#include <mmdeviceapi.h>
#include <windows.h>

using soundint::DeviceInfo;
using soundint::Flow;
using soundint::Role;
using soundint::SessionInfo;
using soundint::audio::util::clampVolume;
using soundint::audio::util::dataFlowFromFlow;
using soundint::audio::util::eRoleFromRole;
using soundint::audio::util::flowFromDataFlow;
using soundint::audio::util::normalizeDisplayName;
using soundint::audio::util::normalizeProcessName;
using soundint::audio::util::pathBaseNameLower;
using soundint::audio::util::probeEndpoint;
using soundint::audio::util::roleFromERole;

// ---------------------------------------------------------------------------

TEST_CASE("audio_util: pathBaseNameLower isola o basename em minusculas")
{
    CHECK(pathBaseNameLower(L"C:\\Apps\\Chrome.exe") == L"chrome.exe");
    CHECK(pathBaseNameLower(L"C:/Apps/Chrome.EXE") == L"chrome.exe");
    CHECK(pathBaseNameLower(L"chrome.exe") == L"chrome.exe");
    CHECK(pathBaseNameLower(L"") == L"");
    CHECK(pathBaseNameLower(L"C:\\Apps\\") == L""); // termina no separador
    CHECK(pathBaseNameLower(L"/") == L"");
}

TEST_CASE("audio_util: pathBaseNameLower mantem acentos e espacos")
{
    CHECK(pathBaseNameLower(L"C:\\Programas\\Meu App.EXE") == L"meu app.exe");
    CHECK(pathBaseNameLower(L"caf\u00e9.exe") == L"caf\u00e9.exe");
}

TEST_CASE("audio_util: normalizeProcessName faz trim, basename e lowercase")
{
    CHECK(normalizeProcessName(L"  C:\\Program Files\\Spotify.exe  ") == L"spotify.exe");
    CHECK(normalizeProcessName(L"Spotify.EXE") == L"spotify.exe");
    CHECK(normalizeProcessName(L"\t\n Spotify.exe \r\n") == L"spotify.exe");
}

TEST_CASE("audio_util: normalizeProcessName rejeita vazio e caracteres proibidos")
{
    CHECK(normalizeProcessName(L"").empty());
    CHECK(normalizeProcessName(L"     ").empty());
    CHECK(normalizeProcessName(L"\\").empty()); // so separador
    CHECK(normalizeProcessName(L"a<b.exe").empty());
    CHECK(normalizeProcessName(L"a>b.exe").empty());
    CHECK(normalizeProcessName(L"a\"b.exe").empty());
    CHECK(normalizeProcessName(L"a:b.exe").empty());
    CHECK(normalizeProcessName(L"a|b.exe").empty());
    CHECK(normalizeProcessName(L"a?b.exe").empty());
    CHECK(normalizeProcessName(L"a*b.exe").empty());
    CHECK(normalizeProcessName(L"a/b.exe") == L"b.exe"); // basename antes de validar
    CHECK(normalizeProcessName(std::wstring(L"a") + wchar_t(0x01) + L"b.exe").empty());
    CHECK(normalizeProcessName(std::wstring(L"a") + wchar_t(0x07) + L"b.exe").empty());
}

TEST_CASE("audio_util: normalizeDisplayName cai no fallback quando nao serve")
{
    // Vazio => fallback.
    CHECK(normalizeDisplayName(L"", L"chrome.exe") == L"chrome.exe");
    CHECK(normalizeDisplayName(L"   ", L"chrome.exe") == L"chrome.exe");
    // Indicacao indireta nao resolvida ("@caminho,-id") => fallback.
    CHECK(normalizeDisplayName(L"@C:\\Apps\\x.exe,-3", L"chrome.exe") == L"chrome.exe");
    CHECK(normalizeDisplayName(L" @sem-resolver,0", L"chrome.exe") == L"chrome.exe");
    // Normal: usa o valor com trim.
    CHECK(normalizeDisplayName(L"  Google Chrome  ", L"chrome.exe") == L"Google Chrome");
    CHECK(normalizeDisplayName(L"Google Chrome", L"outro") == L"Google Chrome");
}

TEST_CASE("audio_util: clampVolume limita ao intervalo 0..1")
{
    CHECK(clampVolume(0.5f) == doctest::Approx(0.5f));
    CHECK(clampVolume(0.0f) == doctest::Approx(0.0f));
    CHECK(clampVolume(1.0f) == doctest::Approx(1.0f));
    CHECK(clampVolume(-0.25f) == doctest::Approx(0.0f));
    CHECK(clampVolume(1.75f) == doctest::Approx(1.0f));
    CHECK(clampVolume(std::nanf("")) == doctest::Approx(0.0f));
}

TEST_CASE("audio_util: conversoes Flow <-> EDataFlow sao bijetivas")
{
    CHECK(dataFlowFromFlow(Flow::Render) == eRender);
    CHECK(dataFlowFromFlow(Flow::Capture) == eCapture);
    CHECK(flowFromDataFlow(eRender) == Flow::Render);
    CHECK(flowFromDataFlow(eCapture) == Flow::Capture);
    // Roundtrip dos dois valores.
    CHECK(flowFromDataFlow(dataFlowFromFlow(Flow::Render)) == Flow::Render);
    CHECK(flowFromDataFlow(dataFlowFromFlow(Flow::Capture)) == Flow::Capture);
}

TEST_CASE("audio_util: conversoes Role <-> ERole cobrem os tres papeis")
{
    CHECK(eRoleFromRole(Role::Console) == eConsole);
    CHECK(eRoleFromRole(Role::Multimedia) == eMultimedia);
    CHECK(eRoleFromRole(Role::Communications) == eCommunications);
    CHECK(roleFromERole(eConsole) == Role::Console);
    CHECK(roleFromERole(eMultimedia) == Role::Multimedia);
    CHECK(roleFromERole(eCommunications) == Role::Communications);
    // Roundtrip.
    CHECK(roleFromERole(eRoleFromRole(Role::Console)) == Role::Console);
    CHECK(roleFromERole(eRoleFromRole(Role::Multimedia)) == Role::Multimedia);
    CHECK(roleFromERole(eRoleFromRole(Role::Communications)) == Role::Communications);
    // Enum invalido => fallback Multimedia/Render (nunca lanca).
    CHECK(roleFromERole(static_cast<ERole>(99)) == Role::Multimedia);
    CHECK(flowFromDataFlow(static_cast<EDataFlow>(99)) == Flow::Render);
}

// ---------------------------------------------------------------------------
// Smoke tests da fachada (COM real). Best-effort: se o ambiente nao tiver
// audio (servico/CI), WARN e segue - nunca falha a suite por infra.
// ---------------------------------------------------------------------------

TEST_CASE("audio: factory devolve fachada utilizavel ou nullptr documentado")
{
    soundint::EventBus bus;
    auto service = soundint::audio::createAudioService(bus);
    if (service == nullptr)
    {
        WARN("audio: COM/MMDevice indisponivel no ambiente - factory devolveu nullptr");
        return;
    }

    // Leituras nao podem lancar nem travar.
    const std::wstring defaultId = service->defaultDevice(Flow::Render, Role::Multimedia);
    const std::vector<DeviceInfo> renders = service->devices(Flow::Render);
    const std::vector<DeviceInfo> captures = service->devices(Flow::Capture);

    for (const DeviceInfo& device : renders)
    {
        CHECK(device.flow == Flow::Render);
        CHECK(device.active);
        CHECK_FALSE(device.id.empty());
    }
    for (const DeviceInfo& device : captures)
    {
        CHECK(device.flow == Flow::Capture);
        CHECK_FALSE(device.id.empty());
    }

    if (!defaultId.empty() && !renders.empty())
    {
        // O default do render aparece na lista e e o unico marcado.
        int defaults = 0;
        bool found = false;
        for (const DeviceInfo& device : renders)
        {
            if (device.id == defaultId)
            {
                found = true;
            }
            if (device.isDefault)
            {
                ++defaults;
            }
        }
        CHECK(found);
        CHECK(defaults == 1);

        // Nome do default preenche deviceName; id inexistente devolve "".
        CHECK_FALSE(service->deviceName(defaultId).empty());
        CHECK(service->deviceName(L"\\?\\SWD#MMDEVAPI#nao-existe").empty());
    }
    else
    {
        WARN("audio: sem endpoint render ativo - verificacoes de default puladas");
    }

    // setDefaultDevice: apenas rejeicoes seguras (nao mexe no default real).
    CHECK_FALSE(service->setDefaultDevice(L"", Flow::Render, Role::Multimedia));
    CHECK_FALSE(
        service->setDefaultDevice(L"\\?\\SWD#MMDEVAPI#nao-existe", Flow::Render, Role::Multimedia));
    if (!defaultId.empty())
    {
        // Fluxo divergente do id real => recusado antes de tocar na API.
        CHECK_FALSE(service->setDefaultDevice(defaultId, Flow::Capture, Role::Multimedia));
    }

    // Sessoes: leitura best-effort; sem apps tocando a lista vem vazia e, se
    // vier com algo, cada sessao precisa de id e volume no intervalo 0..1.
    const std::vector<SessionInfo> live = service->sessions(Flow::Render);
    for (const SessionInfo& session : live)
    {
        CHECK_FALSE(session.instanceId.empty());
        CHECK(session.volume >= 0.0f);
        CHECK(session.volume <= 1.0f);
    }
    CHECK_FALSE(service->setSessionVolume(L"inexistente", 0.5f));
    CHECK_FALSE(service->setSessionMute(L"inexistente", true));
}

TEST_CASE("audio: start registra watchers e stop desliga tudo")
{
    soundint::EventBus bus;
    auto service = soundint::audio::createAudioService(bus);
    if (service == nullptr)
    {
        WARN("audio: COM/MMDevice indisponivel no ambiente - teste live pulado");
        return;
    }

    if (!service->start())
    {
        WARN("audio: start() falhou no ambiente (registro de notificacoes) - teste pulado");
        return;
    }
    CHECK(service->start()); // idempotente

    // Leituras continuam funcionando com watchers ativos.
    for (const SessionInfo& session : service->sessions(Flow::Render))
    {
        CHECK_FALSE(session.instanceId.empty());
    }

    service->stop();
    service->stop(); // idempotente

    // Depois de stop: sem watcher de sessao a lista vem vazia e as leituras
    // de dispositivo continuam seguras.
    CHECK(service->sessions(Flow::Render).empty());
    const std::vector<DeviceInfo> after = service->devices(Flow::Render);
    for (const DeviceInfo& device : after)
    {
        CHECK_FALSE(device.id.empty());
    }
}

TEST_CASE("audio: probeEndpoint devolve estado real ou found=false")
{
    soundint::EventBus bus;
    auto service = soundint::audio::createAudioService(bus);
    if (service == nullptr)
    {
        WARN("audio: COM/MMDevice indisponivel no ambiente - teste live pulado");
        return;
    }

    const std::wstring defaultId = service->defaultDevice(Flow::Render, Role::Multimedia);
    if (defaultId.empty())
    {
        WARN("audio: sem endpoint render ativo - probe pulado");
        CHECK_FALSE(probeEndpoint(nullptr, L"qualquer").found);
        return;
    }

    // Recria um enumerator proprio so para o probe.
    IMMDeviceEnumerator* enumerator = nullptr;
    const HRESULT hr =
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || enumerator == nullptr)
    {
        WARN("audio: CoCreateInstance(MMDeviceEnumerator) falhou - probe pulado");
        return;
    }

    const auto probe = probeEndpoint(enumerator, defaultId.c_str());
    CHECK(probe.found);
    CHECK(probe.flow == Flow::Render);
    CHECK(probe.active);
    CHECK_FALSE(probe.friendlyName.empty());

    CHECK_FALSE(probeEndpoint(enumerator, L"\\?\\SWD#MMDEVAPI#nao-existe").found);
    CHECK_FALSE(probeEndpoint(enumerator, L"").found);
    CHECK_FALSE(probeEndpoint(nullptr, defaultId.c_str()).found);

    enumerator->Release();
}
