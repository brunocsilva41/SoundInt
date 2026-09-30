// ============================================================================
// Testes do Track D — geometria, cores, tokens de forma e resolucao de tema.
// ============================================================================
#include "doctest.h"

#include "ui/design_tokens.h"
#include "ui/palette.h"
#include "ui/renderer.h"

namespace {

using soundint::ui::Color;
using soundint::ui::IRenderTarget;
using soundint::ui::Rect;
using soundint::ui::Renderer;
using soundint::ui::Size;
using soundint::ui::TextStyle;
namespace tokens = soundint::ui::tokens;

bool sameColor(const Color& a, const Color& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool samePalette(const tokens::Palette& a, const tokens::Palette& b)
{
    return sameColor(a.windowBg, b.windowBg) && sameColor(a.surface, b.surface) &&
           sameColor(a.surfaceAlt, b.surfaceAlt) && sameColor(a.border, b.border) &&
           sameColor(a.textPrimary, b.textPrimary) &&
           sameColor(a.textSecondary, b.textSecondary) && sameColor(a.accent, b.accent) &&
           sameColor(a.accentFg, b.accentFg) && sameColor(a.danger, b.danger) &&
           sameColor(a.success, b.success) && sameColor(a.hover, b.hover) &&
           sameColor(a.pressed, b.pressed) && sameColor(a.overlayScrim, b.overlayScrim);
}

// Janela real oculta para exercitar attach/paint/resize/detach do renderer.
LRESULT CALLBACK testWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

class TestWindow {
public:
    TestWindow()
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = testWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        if (RegisterClassExW(&wc) != 0) {
            registered_ = true;
            hwnd_ = CreateWindowExW(0, kClassName, L"", WS_POPUP, 0, 0, 240, 160, nullptr,
                                    nullptr, wc.hInstance, nullptr);
        }
    }

    ~TestWindow()
    {
        if (hwnd_ != nullptr) {
            DestroyWindow(hwnd_);
        }
        if (registered_) {
            UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
        }
    }

    HWND get() const { return hwnd_; }

private:
    static constexpr wchar_t kClassName[] = L"SoundIntUiTestWnd";

    bool registered_ = false;
    HWND hwnd_ = nullptr;
};

}  // namespace

TEST_CASE("ui: Rect::contains respeita as bordas")
{
    const Rect r{10.f, 20.f, 100.f, 50.f};

    CHECK(r.contains(10.f, 20.f));        // canto superior esquerdo: incluso
    CHECK(r.contains(109.f, 69.f));       // ultimo pixel interior
    CHECK(r.contains(50.f, 40.f));        // centro

    CHECK_FALSE(r.contains(9.9f, 40.f));  // fora a esquerda
    CHECK_FALSE(r.contains(50.f, 19.9f)); // fora acima
    CHECK_FALSE(r.contains(110.f, 40.f)); // borda direita: exclusa
    CHECK_FALSE(r.contains(50.f, 70.f));  // borda inferior: exclusa

    const Rect empty{};
    CHECK_FALSE(empty.contains(0.f, 0.f));
}

TEST_CASE("ui: Color::rgb normaliza 0-255 em 0-1")
{
    const Color white = Color::rgb(255, 255, 255);
    CHECK(white.r == doctest::Approx(1.0));
    CHECK(white.g == doctest::Approx(1.0));
    CHECK(white.b == doctest::Approx(1.0));
    CHECK(white.a == doctest::Approx(1.0));

    const Color black = Color::rgb(0, 0, 0);
    CHECK(black.r == doctest::Approx(0.0));
    CHECK(black.a == doctest::Approx(1.0));   // alpha padrao

    const Color mid = Color::rgb(128, 64, 32);
    CHECK(mid.r == doctest::Approx(128.0 / 255.0));
    CHECK(mid.g == doctest::Approx(64.0 / 255.0));
    CHECK(mid.b == doctest::Approx(32.0 / 255.0));

    const Color scrim = Color::rgb(0, 0, 0, 51);   // 20% de preto
    CHECK(scrim.a == doctest::Approx(0.2));
}

TEST_CASE("ui: tokens de forma sao positivos e ordenados")
{
    CHECK(tokens::kGrid > 0.f);
    CHECK(tokens::kSpace1 == tokens::kGrid);
    CHECK(tokens::kSpace1 < tokens::kSpace2);
    CHECK(tokens::kSpace2 < tokens::kSpace3);
    CHECK(tokens::kSpace3 < tokens::kSpace4);
    CHECK(tokens::kSpace4 < tokens::kSpace5);
    CHECK(tokens::kSpace5 < tokens::kSpace6);

    CHECK(tokens::kRadiusSm > 0.f);
    CHECK(tokens::kRadiusSm < tokens::kRadiusMd);
    CHECK(tokens::kRadiusMd < tokens::kRadiusLg);

    CHECK(tokens::kControlHeight > 0.f);
    CHECK(tokens::kRowHeight > tokens::kControlHeight);
    CHECK(tokens::kWindowMargin > 0.f);
    CHECK(tokens::kAnimMs > 0);
}

TEST_CASE("ui: tokens de tipografia sao positivos")
{
    CHECK(tokens::kFontCaption > 0.f);
    CHECK(tokens::kFontCaption < tokens::kFontBody);
    CHECK(tokens::kFontBody < tokens::kFontSubtitle);
    CHECK(tokens::kFontSubtitle < tokens::kFontTitle);
    CHECK(tokens::kWeightRegular > 0);
    CHECK(tokens::kWeightSemibold > tokens::kWeightRegular);

    CHECK(tokens::kFontUi[0] != L'\0');
    CHECK(tokens::kFontIcons[0] != L'\0');
    CHECK(tokens::kIconSpeaker[0] != L'\0');
    CHECK(tokens::kIconSettings[0] != L'\0');
}

TEST_CASE("ui: resolvePalette 1 = claro e 2 = escuro")
{
    const tokens::Palette light = soundint::ui::resolvePalette(1);
    const tokens::Palette dark = soundint::ui::resolvePalette(2);

    CHECK(samePalette(light, tokens::lightPalette()));
    CHECK(samePalette(dark, tokens::darkPalette()));
    CHECK_FALSE(samePalette(light, dark));
}

TEST_CASE("ui: resolvePalette 0 (e valores invalidos) seguem o sistema")
{
    const bool systemDark = soundint::ui::isSystemDark();
    const tokens::Palette expected =
        systemDark ? tokens::darkPalette() : tokens::lightPalette();

    CHECK(samePalette(soundint::ui::resolvePalette(0), expected));
    CHECK(samePalette(soundint::ui::resolvePalette(99), expected));
}

TEST_CASE("ui: renderer e singleton com ciclo de vida proprio")
{
    Renderer& first = Renderer::instance();
    Renderer& second = Renderer::instance();
    CHECK(&first == &second);

    CHECK(first.initialize());
    CHECK(first.initialize());   // idempotente
    first.shutdown();
    first.shutdown();            // idempotente
}

TEST_CASE("ui: attach sem HWND e recusado")
{
    Renderer& renderer = Renderer::instance();
    CHECK_FALSE(renderer.attach(nullptr));
    CHECK_FALSE(renderer.resize(nullptr, 100, 100));
    CHECK(renderer.beginPaint(nullptr, nullptr) == nullptr);
    renderer.endPaint(nullptr);
}

TEST_CASE("ui: ciclo completo de paint em janela real")
{
    TestWindow window;
    REQUIRE(window.get() != nullptr);

    Renderer& renderer = Renderer::instance();
    REQUIRE(renderer.initialize());
    REQUIRE(renderer.attach(window.get()));

    IRenderTarget* rt = renderer.beginPaint(window.get(), nullptr);
    REQUIRE(rt != nullptr);

    const tokens::Palette palette = tokens::lightPalette();
    rt->clear(palette.windowBg);
    rt->fillRoundedRect(Rect{10.f, 10.f, 100.f, 40.f}, tokens::kRadiusMd, palette.accent);
    rt->strokeRoundedRect(Rect{10.f, 10.f, 100.f, 40.f}, tokens::kRadiusMd, 1.f, palette.border);
    rt->drawLine(0.f, 0.f, 100.f, 100.f, 2.f, palette.textPrimary);

    rt->pushClip(Rect{0.f, 0.f, 120.f, 60.f});
    TextStyle style;
    style.fontSize = tokens::kFontBody;
    rt->drawText(L"SoundInt", Rect{16.f, 16.f, 88.f, 28.f}, style);
    rt->popClip();

    rt->drawGlyph(tokens::kIconSpeaker, Rect{10.f, 60.f, 24.f, 24.f}, style);

    const Size measured = rt->measureText(L"Texto de referencia", style);
    CHECK(measured.w > 0.f);
    CHECK(measured.h > 0.f);

    // popClip a mais nao estoura (guarda de balanceamento).
    rt->popClip();

    renderer.endPaint(window.get());
    // endPaint repetido e no-op.
    renderer.endPaint(window.get());

    CHECK(renderer.resize(window.get(), 320, 200));
    renderer.detach(window.get());

    // Detach solta o target; um novo beginPaint faz auto-attach de novo.
    IRenderTarget* reopened = renderer.beginPaint(window.get(), nullptr);
    CHECK(reopened != nullptr);
    if (reopened != nullptr) {
        renderer.endPaint(window.get());
    }
    renderer.detach(window.get());
}

TEST_CASE("ui: beginPaint faz auto-attach da janela")
{
    TestWindow window;
    REQUIRE(window.get() != nullptr);

    Renderer& renderer = Renderer::instance();
    REQUIRE(renderer.initialize());

    IRenderTarget* rt = renderer.beginPaint(window.get(), nullptr);
    REQUIRE(rt != nullptr);
    rt->clear(Color::rgb(32, 32, 32));
    renderer.endPaint(window.get());

    renderer.detach(window.get());
}
