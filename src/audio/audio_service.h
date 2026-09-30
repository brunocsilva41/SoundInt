// ============================================================================
// CONTRACT — fachada de audio consumida por app/UI. CONGELADO na Wave 0.
// Implementacao: Tracks A (devices/sessoes) + B (policy de roteamento).
// Nao ha polling: start() registra notificacoes que publicam no EventBus.
// ============================================================================
#pragma once

#include "core/event_bus.h"
#include "core/types.h"

#include <memory>
#include <vector>

namespace soundint::audio {

class IAudioService {
public:
    virtual ~IAudioService() = default;

    // --- dispositivos ---
    // Observacao: IMMDeviceEnumerator nao tem SetDefaultAudioEndpoint; a
    // troca do default usa IPolicyConfig::SetDefaultEndpoint (nao documentada)
    // por dentro, com validacao previa do endpoint.
    virtual std::vector<DeviceInfo> devices(Flow flow) = 0;
    virtual std::wstring defaultDevice(Flow flow, Role role) = 0;
    virtual bool setDefaultDevice(const std::wstring& deviceId, Flow flow, Role role) = 0;
    virtual std::wstring deviceName(const std::wstring& deviceId) = 0;  // "" se desconhecido

    // --- sessoes (apps com audio) ---
    virtual std::vector<SessionInfo> sessions(Flow flow) = 0;
    virtual bool setSessionVolume(const std::wstring& instanceId, float volume) = 0;
    virtual bool setSessionMute(const std::wstring& instanceId, bool mute) = 0;

    // --- roteamento por app (Windows.Media.Internal.AudioPolicyConfig) ---
    // deviceId vazio => limpa a atribuicao persistida (volta ao default).
    virtual bool setAppDevice(uint32_t pid, Flow flow, Role role,
                              const std::wstring& deviceId) = 0;
    virtual std::wstring appDevice(uint32_t pid, Flow flow, Role role) = 0;  // "" = nenhuma
    virtual bool clearAllAppDevices() = 0;  // ClearAllPersistedApplicationDefaultEndpoints

    // --- ciclo de vida ---
    // Registra IMMNotificationClient + IAudioSessionNotification em todos os
    // endpoints ativos e publica DeviceEvent/SessionEvent no bus.
    virtual bool start() = 0;
    virtual void stop() = 0;
};

// Factory. Retorno nulo indica falha de inicializacao COM/MMDevice.
std::unique_ptr<IAudioService> createAudioService(EventBus& bus);

}  // namespace soundint::audio
