// ============================================================================
// Testes da biblioteca de controles (Track E) — somente IRenderTarget + tokens.
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

    void clear(const Color& color) override { calls.push_back({L"clear", {}, color, {}}); }

    void fillRoundedRect(const Rect& rect, float, const Color& color) override
    {
        calls.push_back({L"fill", rect, color, {}});
    }

    void strokeRoundedRect(const Rect& rect, float, float, const Color& color) override
    {
        calls.push_back({L"stroke", rect, color, {}});
    }

    void drawLine(float, float, float, float, float, const Color& color) override
    {
        calls.push_back({L"line", {}, color, {}});
    }

    void drawText(std::wstring_view text, const Rect& box, const TextStyle&) override
    {
        calls.push_back({L"text", box, {}, std::wstring(text)});
    }

    Size measureText(std::wstring_view text, const TextStyle& style) override
    {
        const float w = static_cast<float>(text.size()) * style.fontSize * 0.5f;
        return {w, style.fontSize};
    }

    void drawGlyph(std::wstring_view glyph, const Rect& box, const TextStyle&) override
    {
        calls.push_back({L"glyph", box, {}, std::wstring(glyph)});
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
    badge.setText(L"v1");
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
