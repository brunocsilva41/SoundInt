// ============================================================================
// CONTRACT — roteamento persistido por app via
// Windows.Media.Internal.AudioPolicyConfig (RoGetActivationFactory).
// IMPLEMENTACAO: Track B. Consumidores: facade do Track A (IAudioService).
//
// IIDs e layout de vtable verificados no EarTrumpet:
//   IAudioPolicyConfigFactoryVariantFor21H2     {ab3d4648-e242-459f-b02f-541c70306324}
//   IAudioPolicyConfigFactoryVariantForDownlevel{2a59116d-6c4f-45e0-a74f-707e3fef9258}
// Ambos: IInspectable + 19 stubs + 3 metodos reais (Set/Get/Clear).
// ============================================================================
#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>

#include <windows.h>
#include <inspectable.h>

namespace soundint::audio::policy {

// deviceId no formato IMMDevice::GetId (empacotado por endpoint_id::packDeviceId)
// ou vazio para limpar a atribuicao persistida.
bool setAppDevice(uint32_t pid, Flow flow, Role role, const std::wstring& deviceId);
std::wstring appDevice(uint32_t pid, Flow flow, Role role);  // "" = nenhuma
bool clearAllAppDevices();

}  // namespace soundint::audio::policy
