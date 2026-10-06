// ============================================================================
// Dropdown (seletor de saida): itens largos, selectedIndex e onChanged.
//
// DECISAO DE LAYOUT (documentada): o popup flutuante e desenhado ABAIXO do
// campo, no mesmo render(), sob pushClip(popupRect). Para ele aparecer, a
// janela DEVE repassar bounds generosos ao expandir (altura = campo + itens,
// consultando requiredHeight()/popupBounds()); se os bounds forem apenas do
// campo, o popup e cortado pelo clip externo da janela (comportamento
// aceito e assumido aqui).
//
// Entrada: wheel/setas com fechado mudam a selecao (notificam); com aberto
// movem o realce; Enter confirma; Esc fecha. Fechado = linha + kIconChevronDown.
//
// DPI: campo, itens, fontes e paddings escalam com scale() (1.0 = atual).
// ============================================================================
#pragma once

#include "ui/controls/interactive.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace soundint::ui::controls {

class Dropdown : public InteractiveControl {
public:
    static constexpr size_t kNoIndex = static_cast<size_t>(-1);
    static constexpr float kItemHeight = tokens::kControlHeight;

    using ChangedFn = std::function<void(size_t)>;

    void setItems(std::vector<std::wstring> items);
    const std::vector<std::wstring>& items() const { return items_; }
    // Set programatico: nao dispara onChanged. Fora de faixa vira kNoIndex.
    void setSelectedIndex(size_t index);
    size_t selectedIndex() const { return selected_; }
    void setOnChanged(ChangedFn fn) { onChanged_ = std::move(fn); }

    bool expanded() const { return expanded_; }
    void setExpanded(bool expanded);

    // API aditiva para janelas: o popup esta aberto (expandido) e qual
    // retangulo ele ocupa no cliente. popupBounds() e vazio quando fechado —
    // util para hit-test externo (ex.: clique-fora) e para expandir bounds.
    bool isPopupOpen() const;
    Rect popupBounds() const;

    // Geometria pura (exposta para teste); alturas ja escaladas por scale().
    Rect fieldRect() const;        // linha do campo (topo dos bounds)
    Rect popupRect() const;        // lista flutuante (abaixo do campo)
    Rect itemRect(size_t index) const;
    size_t hitItem(float x, float y) const;
    float requiredHeight() const;  // altura dos bounds quando expandido

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    Size measure(float constraintWidth) override;

private:
    void select(size_t index, bool notify);
    bool stepSelection(int direction);   // selecao (fechado) — notifica
    void stepIndex(size_t& index, int direction);   // realce (aberto)
    void collapse();

    float itemHeight() const;   // altura de item do popup, ja escalada

    std::vector<std::wstring> items_;
    size_t selected_ = kNoIndex;
    size_t highlight_ = kNoIndex;
    size_t hoverItem_ = kNoIndex;
    bool expanded_ = false;
    ChangedFn onChanged_;
};

}  // namespace soundint::ui::controls
