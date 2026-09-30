// ============================================================================
// Factory publica do modulo de audio (contrato: audio_service.h).
// Devolve nullptr quando o COM/MMDevice nao inicializa.
// ============================================================================
#include "audio/audio_service.h"

#include "audio/audio_service_impl.h"

namespace soundint::audio
{

std::unique_ptr<IAudioService> createAudioService(EventBus& bus)
{
    return createAudioServiceImpl(bus);
}

} // namespace soundint::audio
