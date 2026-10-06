// ============================================================================
// Metricas internas compartilhadas pelos controles (Track E): escala DPI,
// caixa de icone, raios e espessuras. Header INTERNO de src/ui/controls —
// nao incluir fora deste diretorio.
// ============================================================================
#pragma once

#include "ui/design_tokens.h"

namespace soundint::ui::controls::metrics {

// Caixa de glifo de icone padronizada: 16px logicos x fator DPI (grid 4px).
inline float glyphBox(float scale)
{
    return 16.f * scale;
}

// Raio de canto escalado pelo fator DPI do controle.
inline float radius(float radiusPx, float scale)
{
    return radiusPx * scale;
}

// Espessura de traco em PIXEL INTEIRO (1.5px borra no anti-alias): 1px ate
// 199% de DPI e 2px a partir de 200%.
inline float stroke(float scale)
{
    return (scale >= 2.f) ? 2.f : 1.f;
}

}  // namespace soundint::ui::controls::metrics
