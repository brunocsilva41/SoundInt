// ============================================================================
// CONTRACT — empacotamento de device id para o AudioPolicyConfig.
// IMPLEMENTACAO: Track B (endpoint_id.cpp). Testes: tests/audio_endpoint_test.cpp
//
// Formato GetId (IMMDevice::GetId, = DeviceInfo.id):
//   \\?\SWD#MMDEVAPI#{0.0.0.00000000}.{xxxxxxxx-....}
// Formato exigido pelo AudioPolicyConfig (pack):
//   \\?\SWD#MMDEVAPI#{0.0.0.00000000}.{xxxxxxxx-....}#{e6327cad-...-991e976a79d2}
// (sufixo = GUID da interface de dispositivo de audio render/capture)
// ============================================================================
#pragma once

#include "core/types.h"

#include <string>

namespace soundint::audio {

inline constexpr wchar_t kMmdevToken[] = L"\\\\?\\SWD#MMDEVAPI#";
inline constexpr wchar_t kInterfaceRender[] = L"{e6327cad-dcec-4949-ae8a-991e976a79d2}";
inline constexpr wchar_t kInterfaceCapture[] = L"{2eef81be-33fa-4800-9670-1cd474972c3f}";

// GetId -> formato AudioPolicyConfig (idempotente: aceita os dois formatos).
std::wstring packDeviceId(const std::wstring& rawId, Flow flow);

// Formato AudioPolicyConfig (ou GetId) -> formato GetId canonico.
std::wstring unpackDeviceId(const std::wstring& packed);

}  // namespace soundint::audio
