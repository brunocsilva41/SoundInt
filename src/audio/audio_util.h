// ============================================================================
// Helpers do Track A: funcoes puras testaveis + utilidades COM best-effort.
// Nao e header de contrato (Wave 0) - livre para evoluir na Wave 1.
// ============================================================================
#pragma once

#include "core/types.h"

#include <string>

#include <mmdeviceapi.h>
#include <windows.h>

namespace soundint::audio::util
{

// --- funcoes puras (tests/audio_core_test.cpp) -----------------------------

// Caminho -> basename em minusculas. Aceita '\' e '/'; devolve vazio quando o
// caminho termina no separador ou nao ha componente final util.
std::wstring pathBaseNameLower(const std::wstring& path);

// Normaliza o nome de um processo: trim + pathBaseNameLower + validacao.
// Devolve L"" quando o resultado seria vazio ou conteria caracteres proibidos
// em nomes de arquivo (<>:"|?* ou caracteres de controle).
std::wstring normalizeProcessName(const std::wstring& raw);

// displayName final: usa `raw`; se vazio (ou ainda no formato "@indireto",
// nao resolvido) cai para `fallback` (normalmente o processName).
std::wstring normalizeDisplayName(const std::wstring& raw, const std::wstring& fallback);

// Limita ao intervalo 0..1; NaN vira 0.
float clampVolume(float volume);

// --- conversoes entre os enums do contrato e os do COM ---------------------
Flow flowFromDataFlow(EDataFlow flow);
EDataFlow dataFlowFromFlow(Flow flow);
Role roleFromERole(ERole role);
ERole eRoleFromRole(Role role);

// --- COM best-effort (nunca lancam; vazio/falso em falha) ------------------

// PKEY_Device_FriendlyName do endpoint ("" se falhar).
std::wstring deviceFriendlyName(IMMDevice* device);

// Fluxo do endpoint via IMMEndpoint::GetDataFlow (Render em falha).
Flow deviceFlow(IMMDevice* device);

struct EndpointProbe
{
    bool found = false;
    std::wstring friendlyName;
    Flow flow = Flow::Render;
    DWORD state = DEVICE_STATE_NOTPRESENT;
    bool active = false;
};

// GetDevice + nome + fluxo + estado a partir do id.
// enumerator/id nulos ou endpoint desconhecido => found = false.
EndpointProbe probeEndpoint(IMMDeviceEnumerator* enumerator, LPCWSTR deviceId);

// Diagnostico silencioso (OutputDebugStringW) - nada do log de outros tracks.
void debugFailure(const wchar_t* what, HRESULT hr);

} // namespace soundint::audio::util
