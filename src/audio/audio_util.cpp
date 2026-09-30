// ============================================================================
// Helpers do Track A: funcoes puras (testadas em tests/audio_core_test.cpp) e
// utilidades COM best-effort compartilhadas por device_watcher/session_watcher/
// audio_service. Sem dependencia de outros tracks.
// ============================================================================
#include "audio/audio_util.h"

#include <cmath>
#include <cwchar>
#include <cwctype>

namespace soundint::audio::util
{

namespace
{

// PKEY_Device_FriendlyName (functiondiscoverykeys_devpkey.h) e a variante
// DEVPKEY_Device_FriendlyName, declaradas localmente: os PROPERTYKEYs do SDK
// exigem libs de GUID que o alvo `audio` nao linka.
constexpr PROPERTYKEY kFriendlyNameFd = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
constexpr PROPERTYKEY kFriendlyNameDevice = {
    {0xa45c254e, 0xdf6c, 0x4e10, {0x9a, 0x3c, 0x04, 0xfa, 0x06, 0x65, 0x5e, 0x1a}}, 14};

std::wstring trimCopy(const std::wstring& text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == L' ' || text[begin] == L'\t' || text[begin] == L'\r' ||
                           text[begin] == L'\n'))
    {
        ++begin;
    }
    while (end > begin && (text[end - 1] == L' ' || text[end - 1] == L'\t' ||
                           text[end - 1] == L'\r' || text[end - 1] == L'\n'))
    {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool isInvalidNameChar(wchar_t value)
{
    if (value < 0x20 || value == 0x7f)
    {
        return true;
    }
    switch (value)
    {
    case L'<':
    case L'>':
    case L'"':
    case L':':
    case L'|':
    case L'?':
    case L'*':
    case L'\\':
    case L'/':
        return true;
    default:
        return false;
    }
}

} // namespace

std::wstring pathBaseNameLower(const std::wstring& path)
{
    const std::size_t separator = path.find_last_of(L"\\/");
    const std::wstring base = (separator == std::wstring::npos) ? path : path.substr(separator + 1);
    std::wstring result;
    result.reserve(base.size());
    for (const wchar_t character : base)
    {
        result.push_back(static_cast<wchar_t>(std::towlower(character)));
    }
    return result;
}

std::wstring normalizeProcessName(const std::wstring& raw)
{
    const std::wstring trimmed = trimCopy(raw);
    if (trimmed.empty())
    {
        return L"";
    }
    const std::wstring name = pathBaseNameLower(trimmed);
    if (name.empty())
    {
        return L"";
    }
    for (const wchar_t character : name)
    {
        if (isInvalidNameChar(character))
        {
            return L"";
        }
    }
    return name;
}

std::wstring normalizeDisplayName(const std::wstring& raw, const std::wstring& fallback)
{
    const std::wstring trimmed = trimCopy(raw);
    if (trimmed.empty())
    {
        return fallback;
    }
    if (trimmed.front() == L'@')
    {
        // Indicacao indireta nao resolvida ("@caminho,-id"): nao serve para
        // exibicao, entao cai no nome do processo.
        return fallback;
    }
    return trimmed;
}

float clampVolume(float volume)
{
    if (std::isnan(volume) || volume < 0.0f)
    {
        return 0.0f;
    }
    if (volume > 1.0f)
    {
        return 1.0f;
    }
    return volume;
}

Flow flowFromDataFlow(EDataFlow flow)
{
    return (flow == eCapture) ? Flow::Capture : Flow::Render;
}

EDataFlow dataFlowFromFlow(Flow flow)
{
    return (flow == Flow::Capture) ? eCapture : eRender;
}

Role roleFromERole(ERole role)
{
    switch (role)
    {
    case eConsole:
        return Role::Console;
    case eCommunications:
        return Role::Communications;
    case eMultimedia:
    default:
        return Role::Multimedia;
    }
}

ERole eRoleFromRole(Role role)
{
    switch (role)
    {
    case Role::Console:
        return eConsole;
    case Role::Communications:
        return eCommunications;
    case Role::Multimedia:
    default:
        return eMultimedia;
    }
}

std::wstring deviceFriendlyName(IMMDevice* device)
{
    if (device == nullptr)
    {
        return L"";
    }
    IPropertyStore* store = nullptr;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store)) || store == nullptr)
    {
        return L"";
    }

    std::wstring result;
    const PROPERTYKEY keys[] = {kFriendlyNameFd, kFriendlyNameDevice};
    for (const PROPERTYKEY& key : keys)
    {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(store->GetValue(key, &value)))
        {
            if (value.vt == VT_LPWSTR && value.pwszVal != nullptr && value.pwszVal[0] != L'\0')
            {
                result = value.pwszVal;
            }
            else if (value.vt == VT_BSTR && value.bstrVal != nullptr && value.bstrVal[0] != L'\0')
            {
                result = value.bstrVal;
            }
        }
        PropVariantClear(&value);
        if (!result.empty())
        {
            break;
        }
    }

    store->Release();
    return result;
}

Flow deviceFlow(IMMDevice* device)
{
    if (device == nullptr)
    {
        return Flow::Render;
    }
    IMMEndpoint* endpoint = nullptr;
    const HRESULT hr =
        device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast<void**>(&endpoint));
    if (FAILED(hr) || endpoint == nullptr)
    {
        return Flow::Render;
    }
    EDataFlow dataFlow = eRender;
    Flow result = Flow::Render;
    if (SUCCEEDED(endpoint->GetDataFlow(&dataFlow)))
    {
        result = flowFromDataFlow(dataFlow);
    }
    endpoint->Release();
    return result;
}

EndpointProbe probeEndpoint(IMMDeviceEnumerator* enumerator, LPCWSTR deviceId)
{
    EndpointProbe probe;
    if (enumerator == nullptr || deviceId == nullptr || deviceId[0] == L'\0')
    {
        return probe;
    }
    IMMDevice* device = nullptr;
    if (FAILED(enumerator->GetDevice(deviceId, &device)) || device == nullptr)
    {
        return probe;
    }

    probe.found = true;
    probe.friendlyName = deviceFriendlyName(device);
    probe.flow = deviceFlow(device);
    DWORD state = DEVICE_STATE_NOTPRESENT;
    if (SUCCEEDED(device->GetState(&state)))
    {
        probe.state = state;
    }
    probe.active = (probe.state & DEVICE_STATE_ACTIVE) != 0;
    device->Release();
    return probe;
}

void debugFailure(const wchar_t* what, HRESULT hr)
{
    wchar_t buffer[192];
    swprintf_s(buffer, L"SoundInt audio: %ls falhou (hr=0x%08lX)\n",
               (what != nullptr) ? what : L"?", static_cast<unsigned long>(static_cast<DWORD>(hr)));
    OutputDebugStringW(buffer);
}

} // namespace soundint::audio::util
