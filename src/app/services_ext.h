// ============================================================================
// Estencao interna do contrato services.h (Track H). So o boot (main.cpp) e a
// implementacao em services.cpp usam estes pontos; modulos de fora continuam
// consumindo apenas services.h.
// ============================================================================
#pragma once

#include <windows.h>

#include "app/services.h"

#include <memory>

namespace soundint {

// Instala a fachada de audio criada no boot. nullptr => audioService() passa
// a devolver o NullAudioService (nunca ha dereference nulo).
void setAudioService(std::unique_ptr<audio::IAudioService> service);

// Janela oculta que recebe a wakeup do EventBus (WM_APP_WAKEUP).
void setMainMessageWindow(HWND window);

// Controle de sujeira da Store: true = precisa de save() no shutdown.
// A Store nao expoe dirty() no contrato; o boot marca quando o load() falha e
// quem mutar estado deve marcar de novo (ou chamar save() direto, contrato).
void setStoreDirty(bool dirty);
bool storeDirty();

}  // namespace soundint
