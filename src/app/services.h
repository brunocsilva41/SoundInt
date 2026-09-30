// ============================================================================
// CONTRACT — pontos de acesso globais do app (main thread). Track H
// implementa em src/app/services.cpp; demais modulos usam sem depender do app.
// ============================================================================
#pragma once

#include "audio/audio_service.h"
#include "core/event_bus.h"
#include "core/store.h"

namespace soundint {

// Instancias criadas pelo app no boot (main thread).
audio::IAudioService& audioService();
EventBus& events();
core::Store& store();  // alias de core::Store::instance()

// Janela hidden que recebe as wakeups do EventBus (PostMessage WM_APP+1).
// Retorno null antes do boot — consumidores devem tolerar.
HWND mainMessageWindow();

}  // namespace soundint
