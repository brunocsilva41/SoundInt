// ============================================================================
// Testes da biblioteca de controles (Track E) — somente IRenderTarget + tokens.
// Cobrem: API aditiva (popupBounds/setTrailingInset), escala DPI completa,
// anti-UAF do click, estados disabled e espessuras em pixel inteiro.
// ============================================================================
#include "doctest.h"

#include "ui/controls/controls.h"

#include <string>
#include <vector>

using namespace soundint::ui;
using namespace soundint::ui::controls;

namespace {

// Mock de IRenderTarget: registra as chamadas para assercoes de estilo/clip.
class MockRenderTarget : public IRenderTarget {
public:
    struct Call {
        std::wstring op;
        Rect rect{};
        Color color{};
        std::wstring text;
        float strokeW = 0.f;   // espessura (op "stroke")
    };

    std::vector<Call> calls;
    int clipDepth = 0;
    int maxClipDepth = 0;

    int count(const std::wstring& op) const
    {
        int n = 0;
        for (const Call& c : calls) {
            if (c.op == op) {
                ++n;
            }
        }
        return n;
    }

    // Primeira chamada da operacao (ou nullptr).
    const Call* find(const std::wstring& op) const
    {
        for (const Call& c : calls) {
            if (c.op == op) {
                return &c;
            }
        }
        return nullptr;
    }

    void clear(const Color& color) override { calls.push_back({L"clear", {}, color, {}}); }

    void fillRoundedRect(const Rect& rect, float, const Color& color) override
    {
        calls.push_back({L"fill", rect, color, {}});
    }

    void strokeRoundedRect(const Rect& rect, float, float strokeWidth,
                           const Color& color) override
    {
        calls.push_back({L"stroke", rect, color, {}, strokeWidth});
    }

    void drawLine(float, float, float, float, float, const Color& color) override
    {
        calls.push_back({L"line", {}, color, {}});
    }

    void drawText(std::wstring_view text, const Rect& box, const TextStyle& style) override
    {
        calls.push_back({L"text", box, style.color, std::wstring(text)});
    }

    Size measureText(std::wstring_view text, const TextStyle& style) override
    {
        const float w = static_cast<float>(text.size()) * style.fontSize * 0.5f;
        return {w, style.fontSize};
    }

    void drawGlyph(std::wstring_view glyph, const Rect& box, const TextStyle& style) override
    {
        calls.push_back({L"glyph", box, style.color, std::wstring(glyph)});
    }

    void pushClip(const Rect&) override
    {
        ++clipDepth;
        if (clipDepth > maxClipDepth) {
            maxClipDepth = clipDepth;
        }
        calls.push_back({L"push", {}, {}, {}});
    }

    void popClip() override
    {
        --clipDepth;
        calls.push_back({L"pop", {}, {}, {}});
    }
};

bool sameColor(const Color& a, const Color& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// Alias para o registro de chamadas do mock (legibilidade nos testes).
using Call = MockRenderTarget::Call;

// Registra o handler global de invalidacao e devolve ao estado neutro no
// fim do escopo (RAII — nao vaza para os outros casos de teste).
class InvalidateGuard {
public:
    explicit InvalidateGuard(std::vector<std::wstring>* log)
    {
        setInvalidateHandler(
            [](void* context) {
                auto* order = static_cast<std::vector<std::wstring>*>(context);
                order->push_back(L"invalidate");
            },
            log);
    }

    ~InvalidateGuard() { setInvalidateHandler(nullptr, nullptr); }
};

PointerEvent makePointer(PointerKind kind, float x, float y,
                         MouseButton button = MouseButton::None, float wheel = 0.f)
{
    PointerEvent e;
    e.kind = kind;
    e.x = x;
    e.y = y;
    e.button = button;
    e.wheelDelta = wheel;
    return e;
}

KeyEvent makeKey(Key key, bool down = true, bool shift = false)
{
    KeyEvent e;
    e.down = down;
    e.key = key;
    e.shift = shift;
    return e;
}

}  // namespace

// ---------------------------------------------------------------------------
// Slider
// ---------------------------------------------------------------------------
TEST_CASE("slider: clamp e extremos de valueFromX/xFromValue")
{
    const Rect b{10.f, 10.f, 216.f, 32.f};   // pad = 8 -> x util em [18, 218]

    CHECK(Slider::valueFromX(18.f, b) == doctest::Approx(0.f));
    CHECK(Slider::valueFromX(218.f, b) == doctest::Approx(1.f));
    CHECK(Slider::valueFromX(-1000.f, b) == doctest::Approx(0.f));
    CHECK(Slider::valueFromX(100000.f, b) == doctest::Approx(1.f));
    CHECK(Slider::xFromValue(0.f, b) == doctest::Approx(18.f));
    CHECK(Slider::xFromValue(1.f, b) == doctest::Approx(218.f));
    CHECK(Slider::xFromValue(-5.f, b) == doctest::Approx(18.f));
    CHECK(Slider::xFromValue(5.f, b) == doctest::Approx(218.f));

    const float x = Slider::xFromValue(0.25f, b);
    CHECK(Slider::valueFromX(x, b) == doctest::Approx(0.25f));

    // Retangulo degenerado: sem divisao por zero.
    const Rect tiny{0.f, 0.f, 10.f, 10.f};
    CHECK(Slider::valueFromX(50.f, tiny) == doctest::Approx(0.f));
    CHECK(Slider::xFromValue(1.f, tiny) == doctest::Approx(8.f));

    Slider s;
    s.setBounds(b);
    s.setValue(2.f);
    CHECK(s.value() == doctest::Approx(1.f));
    s.setValue(-1.f);
    CHECK(s.value() == doctest::Approx(0.f));
}

TEST_CASE("slider: passos de wheel e seta")
{
    const Rect b{10.f, 10.f, 216.f, 32.f};
    Slider s;
    s.setBounds(b);
    s.setValue(0.5f);

    int fired = 0;
    float last = -1.f;
    s.setOnChanged([&](float v) {
        last = v;
        ++fired;
    });
    s.setFocused(true);

    // Wheel: +0.05 por notch.
    PointerEvent wheel = makePointer(PointerKind::Wheel, 100.f, 26.f, MouseButton::None, 1.f);
    CHECK(s.onPointer(wheel));
    CHECK(s.value() == doctest::Approx(0.55f));
    wheel.wheelDelta = -1.f;
    CHECK(s.onPointer(wheel));
    CHECK(s.value() == doctest::Approx(0.5f));
    CHECK(fired == 2);
    CHECK(last == doctest::Approx(0.5f));

    // Seta: +0.01; Shift+seta: +0.10.
    CHECK(s.onKey(makeKey(Key::Right)));
    CHECK(s.value() == doctest::Approx(0.51f));
    CHECK(s.onKey(makeKey(Key::Right, true, true)));
    CHECK(s.value() == doctest::Approx(0.61f));
    CHECK(s.onKey(makeKey(Key::Left)));
    CHECK(s.value() == doctest::Approx(0.6f));
    CHECK(s.onKey(makeKey(Key::Down)));
    CHECK(s.value() == doctest::Approx(0.59f));

    // Clamp nas extremidades.
    s.setValue(1.f);
    CHECK(s.onKey(makeKey(Key::Right)));
    CHECK(s.value() == doctest::Approx(1.f));
    s.setValue(0.f);
    CHECK(s.onKey(makeKey(Key::Left)));
    CHECK(s.value() == doctest::Approx(0.f));

    // Set programatico nao notifica.
    fired = 0;
    s.setValue(0.3f);
    CHECK(fired == 0);
    CHECK(s.value() == doctest::Approx(0.3f));

    // Sem foco as setas nao fazem nada; wheel fora dos bounds tambem.
    s.setFocused(false);
    CHECK_FALSE(s.onKey(makeKey(Key::Right)));
    CHECK(s.value() == doctest::Approx(0.3f));
    PointerEvent wheelOut = makePointer(PointerKind::Wheel, 999.f, 999.f,
                                        MouseButton::None, 1.f);
    CHECK_FALSE(s.onPointer(wheelOut));
    CHECK(s.value() == doctest::Approx(0.3f));
}

TEST_CASE("slider: arrasto com captura")
{
    Slider s;
    s.setBounds({10.f, 10.f, 216.f, 32.f});

    // Down fora dos bounds nao captura.
    CHECK_FALSE(s.onPointer(makePointer(PointerKind::Down, 5.f, 5.f, MouseButton::Left)));

    CHECK(s.onPointer(makePointer(PointerKind::Down, 118.f, 26.f, MouseButton::Left)));
    CHECK(s.pressed());
    CHECK(s.value() == doctest::Approx(0.5f));

    // Move com captura atualiza o valor.
    CHECK(s.onPointer(makePointer(PointerKind::Move, 218.f, 26.f)));
    CHECK(s.value() == doctest::Approx(1.f));
    CHECK(s.onPointer(makePointer(PointerKind::Move, 0.f, 26.f)));
    CHECK(s.value() == doctest::Approx(0.f));

    CHECK(s.onPointer(makePointer(PointerKind::Up, 0.f, 26.f, MouseButton::Left)));
    CHECK_FALSE(s.pressed());
    CHECK(s.value() == doctest::Approx(0.f));
}

TEST_CASE("slider: pino escalado por scale_ mapeia o valor corretamente")
{
    // 150%: pino 16*1.5 = 24 -> pad 12; span = 200 - 24 = 176.
    const Rect b{0.f, 0.f, 200.f, 48.f};
    CHECK(Slider::valueFromX(12.f, b, 24.f) == doctest::Approx(0.f));
    CHECK(Slider::xFromValue(1.f, b, 24.f) == doctest::Approx(188.f));
    CHECK(Slider::valueFromX(100.f, b, 24.f) == doctest::Approx(88.f / 176.f));

    Slider s;
    s.setScale(1.5f);
    s.setBounds(b);
    CHECK(s.measure(200.f).h == doctest::Approx(48.f));

    // Arrasto ponteiro usa o mapeamento escalado (borda direita = 1.0).
    CHECK(s.onPointer(makePointer(PointerKind::Down, 199.f, 24.f, MouseButton::Left)));
    CHECK(s.value() == doctest::Approx(1.f));
    CHECK(s.onPointer(makePointer(PointerKind::Move, 12.f, 24.f)));
    CHECK(s.value() == doctest::Approx(0.f));
}

TEST_CASE("slider: render pinta overlay de hover/pressionado (afordancia)")
{
    MockRenderTarget rt;
    const tokens::Palette palette = tokens::lightPalette();

    Slider s;
    s.setBounds({0.f, 0.f, 200.f, 32.f});
    s.setValue(0.5f);

    // Neutro: nenhum fill com hover/pressed.
    s.render(rt, palette);
    CHECK_FALSE(rt.find(L"fill") == nullptr);
    bool tinted = false;
    for (const auto& c : rt.calls) {
        if (c.op == L"fill" && (sameColor(c.color, palette.hover) ||
                                sameColor(c.color, palette.pressed))) {
            tinted = true;
        }
    }
    CHECK_FALSE(tinted);

    // Pressionado: overlay pressed sobre os bounds.
    rt.calls.clear();
    CHECK(s.onPointer(makePointer(PointerKind::Down, 100.f, 16.f, MouseButton::Left)));
    s.render(rt, palette);
    tinted = false;
    for (const auto& c : rt.calls) {
        if (c.op == L"fill" && sameColor(c.color, palette.pressed) &&
            c.rect.w == doctest::Approx(200.f) && c.rect.h == doctest::Approx(32.f)) {
            tinted = true;
        }
    }
    CHECK(tinted);

    // Hover puro (sem pressao): overlay hover.
    rt.calls.clear();
    CHECK(s.onPointer(makePointer(PointerKind::Up, 100.f, 16.f, MouseButton::Left)));
    CHECK(s.onPointer(makePointer(PointerKind::Move, 100.f, 16.f)));
    CHECK(s.hovered());
    s.render(rt, palette);
    tinted = false;
    for (const auto& c : rt.calls) {
        if (c.op == L"fill" && sameColor(c.color, palette.hover)) {
            tinted = true;
        }
    }
    CHECK(tinted);
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
TEST_CASE("button: clique por ponteiro, teclado e estados")
{
    Button b;
    b.setBounds({0.f, 0.f, 120.f, 32.f});
    b.setText(L"Aplicar");
    int clicks = 0;
    b.setOnClick([&] { ++clicks; });

    // Hover.
    CHECK(b.onPointer(makePointer(PointerKind::Move, 60.f, 16.f)));
    CHECK(b.hovered());
    CHECK_FALSE(b.onPointer(makePointer(PointerKind::Leave, 60.f, 16.f)));
    CHECK_FALSE(b.hovered());

    // Down captura; Up dentro clica.
    CHECK(b.onPointer(makePointer(PointerKind::Down, 60.f, 16.f, MouseButton::Left)));
    CHECK(b.pressed());
    CHECK(b.onPointer(makePointer(PointerKind::Up, 60.f, 16.f, MouseButton::Left)));
    CHECK(clicks == 1);
    CHECK_FALSE(b.pressed());

    // Up fora dos bounds nao clica.
    CHECK(b.onPointer(makePointer(PointerKind::Down, 60.f, 16.f, MouseButton::Left)));
    CHECK(b.onPointer(makePointer(PointerKind::Up, 500.f, 500.f, MouseButton::Left)));
    CHECK(clicks == 1);

    // Teclado quando focado.
    b.setFocused(true);
    CHECK(b.onKey(makeKey(Key::Enter)));
    CHECK(clicks == 2);
    CHECK(b.onKey(makeKey(Key::Space)));
    CHECK(clicks == 3);

    // Sem foco nao responde.
    b.setFocused(false);
    CHECK_FALSE(b.onKey(makeKey(Key::Enter)));
    CHECK(clicks == 3);

    // Desabilitado nao responde nem ao ponteiro.
    b.setEnabled(false);
    b.setFocused(true);
    CHECK_FALSE(b.onPointer(makePointer(PointerKind::Down, 60.f, 16.f, MouseButton::Left)));
    CHECK_FALSE(b.onKey(makeKey(Key::Enter)));
    CHECK(clicks == 3);

    CHECK(b.measure(200.f).h == doctest::Approx(tokens::kControlHeight));
}

TEST_CASE("button: click invalida ANTES do callback (ordem anti-UAF)")
{
    std::vector<std::wstring> order;
    {
        InvalidateGuard guard(&order);

        Button b;
        b.setBounds({0.f, 0.f, 100.f, 32.f});
        b.setFocused(true);
        b.setOnClick([&order] { order.push_back(L"click"); });

        // Teclado: click() direto -> exatamente invalidate, click (nessa ordem).
        CHECK(b.onKey(makeKey(Key::Space)));
        REQUIRE(order.size() == 2u);
        CHECK(order[0] == L"invalidate");
        CHECK(order[1] == L"click");
    }
    // RAII: o guard devolve o handler global ao neutro ao sair do escopo.
}

TEST_CASE("button: variantes Primary/Danger e estado disabled coerentes")
{
    MockRenderTarget rt;
    const tokens::Palette palette = tokens::lightPalette();

    Button primary;
    primary.setBounds({0.f, 0.f, 100.f, 32.f});
    primary.setText(L"OK");
    primary.setVariant(Button::Variant::Primary);
    primary.render(rt, palette);
    const Call* fill = rt.find(L"fill");
    REQUIRE(fill != nullptr);
    CHECK(sameColor(fill->color, palette.accent));

    rt.calls.clear();
    Button danger;
    danger.setBounds({0.f, 0.f, 100.f, 32.f});
    danger.setText(L"Excluir");
    danger.setVariant(Button::Variant::Danger);
    danger.render(rt, palette);
    fill = rt.find(L"fill");
    REQUIRE(fill != nullptr);
    CHECK(sameColor(fill->color, palette.danger));
    const Call* text = rt.find(L"text");
    REQUIRE(text != nullptr);
    CHECK(sameColor(text->color, palette.dangerFg));

    // Desabilitado: fill surfaceAlt + texto secundario, em qualquer variante.
    rt.calls.clear();
    danger.setEnabled(false);
    danger.render(rt, palette);
    fill = rt.find(L"fill");
    REQUIRE(fill != nullptr);
    CHECK(sameColor(fill->color, palette.surfaceAlt));
    text = rt.find(L"text");
    REQUIRE(text != nullptr);
    CHECK(sameColor(text->color, palette.textSecondary));
}

// ---------------------------------------------------------------------------
// Toggle
// ---------------------------------------------------------------------------
TEST_CASE("toggle: valor, onChanged e geometria do pino")
{
    Toggle t;
    t.setBounds({0.f, 0.f, 100.f, 32.f});
    CHECK_FALSE(t.value());

    std::vector<bool> got;
    t.setOnChanged([&](bool v) { got.push_back(v); });

    const PointerEvent down = makePointer(PointerKind::Down, 50.f, 16.f, MouseButton::Left);
    const PointerEvent up = makePointer(PointerKind::Up, 50.f, 16.f, MouseButton::Left);

    CHECK(t.onPointer(down));
    CHECK(t.onPointer(up));
    CHECK(t.value());
    CHECK(got.size() == 1u);
    CHECK(got[0]);

    CHECK(t.onPointer(down));
    CHECK(t.onPointer(up));
    CHECK_FALSE(t.value());
    CHECK(got.size() == 2u);
    CHECK_FALSE(got[1]);

    t.setFocused(true);
    CHECK(t.onKey(makeKey(Key::Space)));
    CHECK(t.value());

    // Set programatico nao notifica.
    const std::size_t antes = got.size();
    t.setValue(false);
    CHECK_FALSE(t.value());
    CHECK(got.size() == antes);

    // Pino anda com o valor; trilha centrada na altura do controle.
    const Rect rb{0.f, 0.f, 100.f, 32.f};
    const Rect track = Toggle::trackRect(rb);
    CHECK(track.h == doctest::Approx(tokens::kControlHeight - tokens::kSpace1 * 2.f));
    CHECK(track.y == doctest::Approx(tokens::kSpace1));
    CHECK(Toggle::knobRect(rb, true).x > Toggle::knobRect(rb, false).x);
    const Rect knob = Toggle::knobRect(rb, true);
    CHECK(knob.x >= track.x);
    CHECK(knob.x + knob.w <= track.x + track.w);
}

TEST_CASE("toggle: trilha 48px e pino proporcionais ao scale_")
{
    const Rect b{0.f, 0.f, 100.f, 48.f};

    // Geometria pura com scale explicito: 48x24 logicos -> 72x36 a 150%.
    const Rect track = Toggle::trackRect(b, 1.5f);
    CHECK(track.w == doctest::Approx(72.f));
    CHECK(track.h == doctest::Approx(36.f));
    CHECK(track.x == doctest::Approx(14.f));   // (100-72)/2
    CHECK(track.y == doctest::Approx(6.f));    // (48-36)/2

    const Rect knobOn = Toggle::knobRect(b, true, 1.5f);
    const Rect knobOff = Toggle::knobRect(b, false, 1.5f);
    CHECK(knobOn.w == doctest::Approx(24.f));   // 16 * 1.5
    CHECK(knobOn.h == doctest::Approx(24.f));
    CHECK(knobOn.x > knobOff.x);
    CHECK(knobOn.x + knobOn.w <= track.x + track.w);

    // A instancia usa scale_ na geometria e na medida.
    Toggle t;
    t.setScale(1.5f);
    t.setBounds(b);
    t.setValue(true);
    CHECK(t.knobRect().x == doctest::Approx(knobOn.x));
    CHECK(t.measure(10.f).h == doctest::Approx(48.f));
}

// ---------------------------------------------------------------------------
// Dropdown
// ---------------------------------------------------------------------------
TEST_CASE("dropdown: selecao por bounds")
{
    Dropdown dd;
    dd.setBounds({10.f, 10.f, 200.f, 32.f});
    dd.setItems({L"Speaker", L"Headphones", L"HDMI"});
    dd.setSelectedIndex(0u);

    int fired = -1;
    dd.setOnChanged([&](std::size_t i) { fired = static_cast<int>(i); });

    // Geometria pura.
    const Rect field = dd.fieldRect();
    CHECK(field.x == doctest::Approx(10.f));
    CHECK(field.y == doctest::Approx(10.f));
    CHECK(field.w == doctest::Approx(200.f));
    CHECK(field.h == doctest::Approx(tokens::kControlHeight));
    const Rect pop = dd.popupRect();
    CHECK(pop.y == doctest::Approx(42.f));
    CHECK(pop.h == doctest::Approx(3.f * tokens::kControlHeight));
    CHECK(dd.itemRect(2).y == doctest::Approx(106.f));
    CHECK(dd.requiredHeight() == doctest::Approx(128.f));

    // Clique no campo abre sem notificar.
    CHECK(dd.onPointer(makePointer(PointerKind::Down, 100.f, 20.f, MouseButton::Left)));
    CHECK(dd.onPointer(makePointer(PointerKind::Up, 100.f, 20.f, MouseButton::Left)));
    CHECK(dd.expanded());
    CHECK(fired == -1);

    // Clique no item 2 (por bounds) seleciona, notifica e fecha.
    const Rect ir = dd.itemRect(2u);
    const float cx = ir.x + ir.w * 0.5f;
    const float cy = ir.y + ir.h * 0.5f;
    CHECK(dd.hitItem(cx, cy) == 2u);
    CHECK(dd.hitItem(100.f, 20.f) == Dropdown::kNoIndex);   // campo, fora do popup
    CHECK(dd.onPointer(makePointer(PointerKind::Down, cx, cy, MouseButton::Left)));
    CHECK(dd.onPointer(makePointer(PointerKind::Up, cx, cy, MouseButton::Left)));
    CHECK(dd.selectedIndex() == 2u);
    CHECK(fired == 2);
    CHECK_FALSE(dd.expanded());

    // Clique fora fecha sem alterar a selecao.
    CHECK(dd.onPointer(makePointer(PointerKind::Down, 100.f, 20.f, MouseButton::Left)));
    CHECK(dd.onPointer(makePointer(PointerKind::Up, 100.f, 20.f, MouseButton::Left)));
    CHECK(dd.expanded());
    CHECK(dd.onPointer(makePointer(PointerKind::Down, 500.f, 500.f, MouseButton::Left)));
    CHECK_FALSE(dd.expanded());
    CHECK(dd.selectedIndex() == 2u);
    CHECK(fired == 2);
}

TEST_CASE("dropdown: navegacao por wheel e teclado")
{
    Dropdown dd;
    dd.setBounds({10.f, 10.f, 200.f, 32.f});
    dd.setItems({L"A", L"B", L"C"});
    dd.setSelectedIndex(0u);
    dd.setFocused(true);

    int fired = -1;
    dd.setOnChanged([&](std::size_t i) { fired = static_cast<int>(i); });

    // Fechado: setas mudam a selecao e notificam.
    CHECK(dd.onKey(makeKey(Key::Down)));
    CHECK(dd.selectedIndex() == 1u);
    CHECK(fired == 1);
    CHECK(dd.onKey(makeKey(Key::Up)));
    CHECK(dd.selectedIndex() == 0u);
    CHECK(fired == 0);

    // Fechado: wheel move a selecao (para baixo = proximo item).
    PointerEvent wheel = makePointer(PointerKind::Wheel, 100.f, 20.f, MouseButton::None, -1.f);
    CHECK(dd.onPointer(wheel));
    CHECK(dd.selectedIndex() == 1u);
    CHECK(fired == 1);

    // Enter abre; setas movem o realce SEM notificar.
    CHECK(dd.onKey(makeKey(Key::Enter)));
    CHECK(dd.expanded());
    fired = -1;
    CHECK(dd.onKey(makeKey(Key::Down)));
    CHECK(dd.selectedIndex() == 1u);
    CHECK(fired == -1);

    // Enter confirma o realce.
    CHECK(dd.onKey(makeKey(Key::Enter)));
    CHECK(dd.selectedIndex() == 2u);
    CHECK(fired == 2);
    CHECK_FALSE(dd.expanded());

    // Esc fecha sem confirmar.
    CHECK(dd.onKey(makeKey(Key::Enter)));
    CHECK(dd.expanded());
    CHECK(dd.onKey(makeKey(Key::Escape)));
    CHECK_FALSE(dd.expanded());
    CHECK(dd.selectedIndex() == 2u);
    CHECK(fired == 2);

    // Sem foco nada e consumido.
    dd.setFocused(false);
    CHECK_FALSE(dd.onKey(makeKey(Key::Down)));
    CHECK(dd.selectedIndex() == 2u);
}

TEST_CASE("dropdown: isPopupOpen e popupBounds (API aditiva)")
{
    Dropdown dd;
    dd.setBounds({10.f, 10.f, 200.f, 32.f});
    dd.setItems({L"A", L"B"});

    // Fechado: sem popup, bounds vazio.
    CHECK_FALSE(dd.isPopupOpen());
    CHECK(dd.popupBounds().w == doctest::Approx(0.f));
    CHECK(dd.popupBounds().h == doctest::Approx(0.f));

    // Aberto: espelha popupRect().
    dd.setExpanded(true);
    CHECK(dd.isPopupOpen());
    const Rect pop = dd.popupRect();
    const Rect open = dd.popupBounds();
    CHECK(open.x == doctest::Approx(pop.x));
    CHECK(open.y == doctest::Approx(pop.y));
    CHECK(open.w == doctest::Approx(pop.w));
    CHECK(open.h == doctest::Approx(pop.h));

    // Recolhido de novo: volta a vazio.
    dd.setExpanded(false);
    CHECK_FALSE(dd.isPopupOpen());
    CHECK(dd.popupBounds().w == doctest::Approx(0.f));
    CHECK(dd.popupBounds().h == doctest::Approx(0.f));
}

TEST_CASE("dropdown: campo e popup em 150% sem zona morta de hit-test")
{
    Dropdown dd;
    dd.setScale(1.5f);
    dd.setBounds({0.f, 0.f, 200.f, 48.f});
    dd.setItems({L"A", L"B", L"C"});
    dd.setSelectedIndex(0u);

    // O campo ocupa os 48px do controle (antes: 32 fixos -> zona morta).
    const Rect field = dd.fieldRect();
    CHECK(field.h == doctest::Approx(48.f));

    // Down em y=40 (abaixo dos 32px antigos) e capturado pelo campo.
    CHECK(dd.onPointer(makePointer(PointerKind::Down, 100.f, 40.f, MouseButton::Left)));
    CHECK(dd.pressed());
    CHECK(dd.onPointer(makePointer(PointerKind::Up, 100.f, 40.f, MouseButton::Left)));
    CHECK(dd.isPopupOpen());

    // Popup: itens de 48px, hit correto, medidas coerentes.
    const Rect pop = dd.popupBounds();
    CHECK(pop.y == doctest::Approx(48.f));
    CHECK(pop.h == doctest::Approx(144.f));
    CHECK(dd.itemRect(0u).h == doctest::Approx(48.f));
    CHECK(dd.hitItem(100.f, 48.f + 48.f + 10.f) == 1u);
    CHECK(dd.requiredHeight() == doctest::Approx(192.f));
    CHECK(dd.measure(200.f).h == doctest::Approx(48.f));
}

TEST_CASE("dropdown: desabilitado pinta surfaceAlt e texto secundario")
{
    MockRenderTarget rt;
    const tokens::Palette palette = tokens::lightPalette();

    Dropdown dd;
    dd.setBounds({0.f, 0.f, 200.f, 32.f});
    dd.setItems({L"A", L"B"});
    dd.setSelectedIndex(0u);
    dd.setEnabled(false);
    dd.render(rt, palette);

    const Call* fill = rt.find(L"fill");
    REQUIRE(fill != nullptr);
    CHECK(sameColor(fill->color, palette.surfaceAlt));
    const Call* text = rt.find(L"text");
    REQUIRE(text != nullptr);
    CHECK(sameColor(text->color, palette.textSecondary));
}

// ---------------------------------------------------------------------------
// ListRow / Badge
// ---------------------------------------------------------------------------
TEST_CASE("listrow: clique, hover e medida")
{
    ListRow row;
    row.setBounds({0.f, 0.f, 300.f, 40.f});
    row.setTitle(L"Spotify");
    row.setSubtitle(L"Enumeracao de audio");

    int clicks = 0;
    row.setOnClick([&] { ++clicks; });

    CHECK(row.onPointer(makePointer(PointerKind::Move, 10.f, 10.f)));
    CHECK(row.hovered());
    CHECK_FALSE(row.onPointer(makePointer(PointerKind::Leave, 10.f, 10.f)));
    CHECK_FALSE(row.hovered());

    CHECK(row.onPointer(makePointer(PointerKind::Down, 150.f, 20.f, MouseButton::Left)));
    CHECK(row.pressed());
    CHECK(row.onPointer(makePointer(PointerKind::Up, 150.f, 20.f, MouseButton::Left)));
    CHECK(clicks == 1);
    CHECK_FALSE(row.pressed());

    CHECK(row.measure(300.f).h == doctest::Approx(tokens::kRowHeight));
}

TEST_CASE("listrow: trailingInset reserva espaco a direita do texto")
{
    MockRenderTarget rt;
    const tokens::Palette palette = tokens::lightPalette();

    ListRow row;
    row.setBounds({0.f, 0.f, 300.f, 40.f});
    row.setTitle(L"Spotify");
    CHECK(row.trailingInset() == doctest::Approx(0.f));

    row.render(rt, palette);
    const Call* base = rt.find(L"text");
    REQUIRE(base != nullptr);
    CHECK(base->rect.w > 0.f);
    const float baseW = base->rect.w;

    // Reserva de 28px: o texto encolhe exatamente esse tanto.
    row.setTrailingInset(28.f);
    CHECK(row.trailingInset() == doctest::Approx(28.f));
    rt.calls.clear();
    row.render(rt, palette);
    const Call* inset = rt.find(L"text");
    REQUIRE(inset != nullptr);
    CHECK(inset->rect.w == doctest::Approx(baseW - 28.f));
    CHECK(inset->rect.x == doctest::Approx(base->rect.x));   // esquerda fixa
}

TEST_CASE("listrow: paddings e caixa de icone escalam com scale_")
{
    MockRenderTarget rt;
    const tokens::Palette palette = tokens::lightPalette();

    ListRow row;
    row.setScale(1.5f);
    row.setBounds({0.f, 0.f, 300.f, 60.f});
    row.setGlyph(tokens::kIconSpeaker);
    row.setTitle(L"Spotify");
    row.setTrailingInset(30.f);
    row.render(rt, palette);

    // Glifo em caixa de 20*1.5 = 30, com padding 8*1.5 = 12.
    const Call* glyph = rt.find(L"glyph");
    REQUIRE(glyph != nullptr);
    CHECK(glyph->rect.x == doctest::Approx(12.f));
    CHECK(glyph->rect.w == doctest::Approx(30.f));

    // Texto: x = 12 + 30 + 12 = 54; direita = 300 - 12 - 30 = 258.
    const Call* text = rt.find(L"text");
    REQUIRE(text != nullptr);
    CHECK(text->rect.x == doctest::Approx(54.f));
    CHECK(text->rect.w == doctest::Approx(258.f - 54.f));

    CHECK(row.measure(300.f).h == doctest::Approx(60.f));
}

TEST_CASE("badge: medida natural")
{
    MockRenderTarget rt;
    Badge badge;
    badge.setText(L"v1.2.3");   // 6 chars * 12 * 0.5 = 36

    const Size nat = badge.naturalSize(rt);
    CHECK(nat.w == doctest::Approx(36.f + tokens::kSpace2 * 2.f));
    CHECK(nat.h == doctest::Approx(tokens::kControlHeight));
    CHECK(badge.measure(100.f).h == doctest::Approx(tokens::kControlHeight));
}

TEST_CASE("badge: medida natural e disabled seguem scale_")
{
    MockRenderTarget rt;
    const tokens::Palette palette = tokens::lightPalette();

    Badge badge;
    badge.setScale(1.5f);
    badge.setText(L"v1.2.3");   // 6 chars * (12*1.5) * 0.5 = 54

    const Size nat = badge.naturalSize(rt);
    CHECK(nat.w == doctest::Approx(54.f + tokens::kSpace2 * 1.5f * 2.f));
    CHECK(nat.h == doctest::Approx(48.f));
    CHECK(badge.measure(100.f).h == doctest::Approx(48.f));

    // Desabilitado: neutro (surfaceAlt + texto secundario) mesmo com Danger.
    badge.setKind(Badge::Kind::Danger);
    badge.setEnabled(false);
    badge.render(rt, palette);
    const Call* fill = rt.find(L"fill");
    REQUIRE(fill != nullptr);
    CHECK(sameColor(fill->color, palette.surfaceAlt));
    const Call* text = rt.find(L"text");
    REQUIRE(text != nullptr);
    CHECK(sameColor(text->color, palette.textSecondary));
}

// ---------------------------------------------------------------------------
// ListPanel
// ---------------------------------------------------------------------------
TEST_CASE("listpanel: layout em cascata, scroll e medida")
{
    ListPanel panel;
    panel.setBounds({0.f, 0.f, 200.f, 100.f});

    ListRow r1;
    ListRow r2;
    ListRow r3;
    r1.setTitle(L"Um");
    r2.setTitle(L"Dois");
    r3.setTitle(L"Tres");
    panel.addControl(&r1);
    panel.addControl(&r2);
    panel.addControl(&r3);

    // 3 linhas de 40 + 2 gaps de 4.
    CHECK(panel.contentHeight() == doctest::Approx(128.f));
    CHECK(panel.maxScrollY() == doctest::Approx(28.f));
    CHECK(panel.measure(200.f).h == doctest::Approx(128.f));

    // Cascata de setBounds.
    panel.layout();
    CHECK(r1.bounds().y == doctest::Approx(0.f));
    CHECK(r2.bounds().y == doctest::Approx(44.f));
    CHECK(r3.bounds().y == doctest::Approx(88.f));
    CHECK(r1.bounds().w == doctest::Approx(200.f));
    CHECK(r1.bounds().h == doctest::Approx(tokens::kRowHeight));

    // Scroll clamped no maximo e offset aplicado em cascata.
    panel.setScrollY(500.f);
    CHECK(panel.scrollY() == doctest::Approx(28.f));
    panel.layout();
    CHECK(r1.bounds().y == doctest::Approx(-28.f));

    // Wheel para baixo (delta -1) rola ate o maximo...
    panel.setScrollY(0.f);
    panel.layout();
    PointerEvent wheel = makePointer(PointerKind::Wheel, 100.f, 95.f, MouseButton::None, -1.f);
    CHECK(panel.onPointer(wheel));
    CHECK(panel.scrollY() == doctest::Approx(28.f));

    // ...e para cima (delta +1) volta ao topo.
    wheel.wheelDelta = 1.f;
    CHECK(panel.onPointer(wheel));
    CHECK(panel.scrollY() == doctest::Approx(0.f));

    // Clamp inferior.
    panel.setScrollY(-50.f);
    CHECK(panel.scrollY() == doctest::Approx(0.f));

    // Filhos invisíveis nao ocupam espaco.
    r2.setVisible(false);
    CHECK(panel.contentHeight() == doctest::Approx(84.f));   // 2*40 + 1*4
}

TEST_CASE("listpanel: gap e passo de scroll escalam com scale_")
{
    ListPanel panel;
    panel.setScale(1.5f);
    panel.setBounds({0.f, 0.f, 200.f, 40.f});   // viewport baixo p/ ver o passo

    ListRow r1;
    ListRow r2;
    r1.setScale(1.5f);
    r2.setScale(1.5f);
    panel.addControl(&r1);
    panel.addControl(&r2);

    // 2 linhas de 60 + 1 gap de 6 (kSpace1 * 1.5).
    CHECK(panel.contentHeight() == doctest::Approx(126.f));
    CHECK(panel.maxScrollY() == doctest::Approx(86.f));

    panel.layout();
    CHECK(r1.bounds().y == doctest::Approx(0.f));
    CHECK(r1.bounds().h == doctest::Approx(60.f));
    CHECK(r2.bounds().y == doctest::Approx(66.f));

    // Wheel: 1 notch = kScrollStep * 1.5 = 60.
    PointerEvent wheel = makePointer(PointerKind::Wheel, 100.f, 20.f, MouseButton::None, -1.f);
    CHECK(panel.onPointer(wheel));
    CHECK(panel.scrollY() == doctest::Approx(60.f));

    // gap logico via setGap tambem escala (4 logicos -> 6 fisicos).
    ListPanel custom;
    custom.setScale(1.5f);
    custom.setBounds({0.f, 0.f, 200.f, 10.f});
    custom.setGap(tokens::kSpace1);
    ListRow a;
    a.setScale(1.5f);
    ListRow b;
    b.setScale(1.5f);
    custom.addControl(&a);
    custom.addControl(&b);
    CHECK(custom.contentHeight() == doctest::Approx(126.f));
}

// ---------------------------------------------------------------------------
// Espessuras (grid de pixels inteiros)
// ---------------------------------------------------------------------------
TEST_CASE("controles: espessuras de traco em pixel inteiro (1px; 2px em 200%)")
{
    const tokens::Palette palette = tokens::lightPalette();

    for (const float scale : {1.f, 1.5f}) {
        MockRenderTarget rt;
        Button button;
        button.setBounds({0.f, 0.f, 120.f, 32.f});
        button.setScale(scale);
        button.setFocused(true);
        button.render(rt, palette);

        Slider slider;
        slider.setBounds({0.f, 40.f, 200.f, 32.f});
        slider.setScale(scale);
        slider.render(rt, palette);

        REQUIRE(rt.count(L"stroke") > 0);
        for (const Call& c : rt.calls) {
            if (c.op == L"stroke") {
                CHECK(c.strokeW == doctest::Approx(1.f));   // nunca 1.5px
            }
        }
    }

    {
        MockRenderTarget rt;
        Button button;
        button.setBounds({0.f, 0.f, 120.f, 64.f});
        button.setScale(2.f);
        button.setFocused(true);
        button.render(rt, palette);
        for (const Call& c : rt.calls) {
            if (c.op == L"stroke") {
                CHECK(c.strokeW == doctest::Approx(2.f));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Render (smoke) — clip equilibrado, sem dependencia do Track D
// ---------------------------------------------------------------------------
TEST_CASE("render: smoke no mock e clip balanceado")
{
    MockRenderTarget rt;
    const tokens::Palette palette = tokens::lightPalette();

    Button button;
    button.setBounds({0.f, 0.f, 120.f, 32.f});
    button.setText(L"OK");
    button.setGlyph(tokens::kIconCheck);
    button.setFocused(true);
    button.render(rt, palette);

    Toggle toggle;
    toggle.setBounds({0.f, 40.f, 100.f, 32.f});
    toggle.setValue(true);
    toggle.render(rt, palette);

    Slider slider;
    slider.setBounds({0.f, 80.f, 200.f, 32.f});
    slider.setValue(0.5f);
    slider.render(rt, palette);

    ListRow row;
    row.setBounds({0.f, 120.f, 300.f, 40.f});
    row.setGlyph(tokens::kIconSpeaker);
    row.setTitle(L"Spotify");
    row.setSubtitle(L"Linha1");
    row.render(rt, palette);

    Badge badge;
    badge.setBounds({0.f, 164.f, 60.f, 32.f});
    badge.setText(L"v");
    badge.setKind(Badge::Kind::Accent);
    badge.render(rt, palette);

    Dropdown dd;
    dd.setBounds({0.f, 200.f, 200.f, 32.f});
    dd.setItems({L"A", L"B"});
    dd.setSelectedIndex(0u);
    dd.setExpanded(true);
    dd.render(rt, palette);

    ListPanel panel;
    panel.setBounds({0.f, 240.f, 300.f, 80.f});
    panel.addControl(&row);
    panel.render(rt, palette);

    CHECK(rt.clipDepth == 0);
    CHECK(rt.count(L"push") == rt.count(L"pop"));
    CHECK(rt.count(L"push") >= 2);   // dropdown + listpanel
    CHECK(rt.maxClipDepth >= 1);
    CHECK(rt.count(L"fill") > 0);
    CHECK(rt.count(L"stroke") > 0);
    CHECK(rt.count(L"text") > 0);
    CHECK(rt.count(L"glyph") > 0);
}
