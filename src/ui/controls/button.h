// ============================================================================
// Botao com texto e/ou glifo (tokens::kIcon*).
// Pointer: Down captura; Up dentro -> clique. Hover/pressionado aplicam
// palette.hover/pressed. Teclado: Enter/Space quando focado.
// ============================================================================
#pragma once

#include "ui/controls/interactive.h"

#include <functional>
#include <string>

namespace soundint::ui::controls {

class Button : public InteractiveControl {
public:
    using ClickFn = std::function<void()>;

    // Hierarquia visual: Primary = acao principal (accent), Danger =
    // destrutiva (vermelho), Secondary = padrao (surface + borda).
    enum class Variant { Secondary, Primary, Danger };

    void setText(std::wstring text) { text_ = std::move(text); }
    const std::wstring& text() const { return text_; }
    void setGlyph(std::wstring glyph) { glyph_ = std::move(glyph); }
    const std::wstring& glyph() const { return glyph_; }
    void setVariant(Variant variant) { variant_ = variant; }
    Variant variant() const { return variant_; }
    void setOnClick(ClickFn fn) { onClick_ = std::move(fn); }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    Size measure(float constraintWidth) override;

private:
    void click();

    std::wstring text_;
    std::wstring glyph_;
    Variant variant_ = Variant::Secondary;
    ClickFn onClick_;
};

}  // namespace soundint::ui::controls
