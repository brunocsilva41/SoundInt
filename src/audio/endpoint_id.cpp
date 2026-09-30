// ============================================================================
// Empacotamento de device id para o AudioPolicyConfig (contrato: endpoint_id.h).
// Formatos tratados:
//   GetId             \\?\SWD#MMDEVAPI#{0.0.0.00000000}.{xxxxxxxx-....}
//   AudioPolicyConfig = GetId + #{<GUID da interface render/capture>}
// O separador '#' antes do GUID da interface e obrigatorio (formato verificado
// contra o AudioPolicyConfig real: sem ele o Windows responde E_INVALIDARG).
// O GetId pode chegar com ou sem o token MMDEVAPI; pack/unpack normalizam
// para o formato canonico com token.
// ============================================================================
#include "audio/endpoint_id.h"

#include <cwchar>

namespace soundint::audio
{

namespace
{

// GUIDs do Windows voltam em caixa mista; comparar sem depender de locale.
wchar_t asciiLower(wchar_t ch)
{
    if (ch >= L'A' && ch <= L'Z')
    {
        return static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return ch;
}

bool endsWithIgnoreCase(const std::wstring& text, const wchar_t* suffix)
{
    const size_t suffixLen = std::wcslen(suffix);
    if (text.size() < suffixLen)
    {
        return false;
    }
    const size_t offset = text.size() - suffixLen;
    for (size_t i = 0; i < suffixLen; ++i)
    {
        if (asciiLower(text[offset + i]) != asciiLower(suffix[i]))
        {
            return false;
        }
    }
    return true;
}

bool startsWithIgnoreCase(const std::wstring& text, const wchar_t* prefix)
{
    const size_t prefixLen = std::wcslen(prefix);
    if (text.size() < prefixLen)
    {
        return false;
    }
    for (size_t i = 0; i < prefixLen; ++i)
    {
        if (asciiLower(text[i]) != asciiLower(prefix[i]))
        {
            return false;
        }
    }
    return true;
}

// Remove todos os sufixos de interface conhecidos (formato parcial tambem).
// O sufixo canonico e "#<GUID>"; se o '#' nao estiver la (entrada parcial de
// outra fonte), remove so o GUID.
std::wstring stripInterfaceSuffix(std::wstring id)
{
    bool removed = true;
    while (removed && !id.empty())
    {
        removed = false;
        if (endsWithIgnoreCase(id, kInterfaceRender))
        {
            id.resize(id.size() - std::wcslen(kInterfaceRender));
            removed = true;
        }
        else if (endsWithIgnoreCase(id, kInterfaceCapture))
        {
            id.resize(id.size() - std::wcslen(kInterfaceCapture));
            removed = true;
        }
        // Come tambem o '#' separador, mas so se ele fecha um endpoint
        // ("...}#"), nunca o '#' final do proprio token MMDEVAPI.
        if (removed && id.size() >= 2 && id.back() == L'#' && id[id.size() - 2] == L'}')
        {
            id.resize(id.size() - 1);
        }
    }
    return id;
}

// Garante o token MMDEVAPI no inicio, sem duplicar (normaliza a caixa para o
// formato canonico do GetId).
std::wstring ensureToken(std::wstring id)
{
    const size_t tokenLen = std::wcslen(kMmdevToken);
    if (startsWithIgnoreCase(id, kMmdevToken))
    {
        if (id.compare(0, tokenLen, kMmdevToken) != 0)
        {
            id.replace(0, tokenLen, kMmdevToken);
        }
        return id;
    }
    id.insert(0, kMmdevToken);
    return id;
}

} // namespace

std::wstring packDeviceId(const std::wstring& rawId, Flow flow)
{
    if (rawId.empty())
    {
        return L"";
    }
    const wchar_t* const interfaceId =
        (flow == Flow::Render) ? kInterfaceRender : kInterfaceCapture;
    // Idempotente: ja empacotado -> tira o sufixo antigo e regrava o do fluxo.
    std::wstring id = ensureToken(stripInterfaceSuffix(rawId));
    if (id.back() != L'#')
    {
        id += L'#'; // separador obrigatorio antes do GUID da interface
    }
    id += interfaceId;
    return id;
}

std::wstring unpackDeviceId(const std::wstring& packed)
{
    if (packed.empty())
    {
        return L"";
    }
    return ensureToken(stripInterfaceSuffix(packed));
}

} // namespace soundint::audio
