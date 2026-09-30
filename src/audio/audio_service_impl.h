// ============================================================================
// Detalhe interno da fachada IAudioService (contrato em audio_service.h).
// A implementacao concreta vive em audio_service.cpp; a factory publica fica
// em init.cpp. Nao e header de contrato - Track A (Wave 1).
// ============================================================================
#pragma once

#include "audio/audio_service.h"

namespace soundint::audio
{

// Constroi o AudioService concreto (COM/MMDevice). Retorno nulo = falha de
// inicializacao COM - mesma semantica da factory publica.
std::unique_ptr<IAudioService> createAudioServiceImpl(EventBus& bus);

} // namespace soundint::audio
