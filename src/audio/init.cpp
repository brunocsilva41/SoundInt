#include "audio/audio_service.h"

namespace soundint::audio {

// STUB Wave 0 — Track A substitui este arquivo por um factory real que
// construi o AudioService (devices + sessoes + notificacoes) unindo o
// trabalho dos Tracks A e B.
std::unique_ptr<IAudioService> createAudioService(EventBus& /*bus*/)
{
    return nullptr;
}

}  // namespace soundint::audio
