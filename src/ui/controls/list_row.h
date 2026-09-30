// ============================================================================
// Linha de lista: glifo (icone) + titulo + subtitulo opcional.
// Pointer: Down captura; Up dentro -> clique. Altura tokens::kRowHeight.
// ============================================================================
#pragma once

#include "ui/controls/interactive.h"

#include <functional>
#include <string>

namespace soundint::ui::controls {

class ListRow : public InteractiveControl {
public:
    using ClickFn = std::function<void()>;

    void setGlyph(std::wstring glyph) { glyph_ = std::move(glyph); }
    const std::wstring& glyph() const { return glyph_; }
    void setTitle(std::wstring title) { title_ = std::move(title); }
    const std::wstring& title() const { return title_; }
    void setSubtitle(std::wstring subtitle) { subtitle_ = std::move(subtitle); }
    const std::wstring& subtitle() const { return subtitle_; }
    void setOnClick(ClickFn fn) { onClick_ = std::move(fn); }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    Size measure(float constraintWidth) override;

private:
    void click();

    std::wstring glyph_;
    std::wstring title_;
    std::wstring subtitle_;
    ClickFn onClick_;
};

}  // namespace soundint::ui::controls
