// ============================================================================
// Badge: texto curto em capsula (radius tokens::kRadiusSm).
// Cor por Kind usando apenas a paleta recebida no render; desabilitado cai
// sempre no neutro (surfaceAlt + texto secundario).
// Largura natural exige um IRenderTarget (measureText); medida de layout
// devolve a altura kControlHeight*scale() e a largura do constraint.
// ============================================================================
#pragma once

#include "ui/control.h"

#include <string>

namespace soundint::ui::controls {

class Badge : public Control {
public:
    enum class Kind : uint8_t { Neutral, Accent, Success, Danger };

    void setText(std::wstring text) { text_ = std::move(text); }
    const std::wstring& text() const { return text_; }
    void setKind(Kind kind) { kind_ = kind; }
    Kind kind() const { return kind_; }

    // Tamanho natural (testavel com um IRenderTarget mock), escalado.
    Size naturalSize(IRenderTarget& rt) const;

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    Size measure(float constraintWidth) override;

private:
    std::wstring text_;
    Kind kind_ = Kind::Neutral;
};

}  // namespace soundint::ui::controls
