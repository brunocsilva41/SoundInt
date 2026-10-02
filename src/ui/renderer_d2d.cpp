// ============================================================================
// Renderer Direct2D/DirectWrite (Track D) — implementacao.
// ============================================================================
#include "ui/renderer_d2d.h"

#include "ui/design_tokens.h"

#include <cmath>

namespace soundint::ui {

namespace {

// 1 DIP = 1 px fisico: o contrato inteiro trabalha em pixels literais.
constexpr float kTargetDpi = 96.0f;
// Limite de medida do measureText (sem restricao de largura no contrato).
constexpr float kMeasureLimit = 1000000.f;

D2D1_COLOR_F toD2d(const Color& color)
{
    return D2D1_COLOR_F{color.r, color.g, color.b, color.a};
}

D2D1_RECT_F toRect(const Rect& rect)
{
    return D2D1_RECT_F{rect.x, rect.y, rect.x + rect.w, rect.y + rect.h};
}

float clampedRadius(float radius)
{
    return radius > 0.f ? radius : 0.f;
}

DWRITE_TEXT_ALIGNMENT toAlignment(TextStyle::Align align)
{
    switch (align) {
    case TextStyle::Align::Center:
        return DWRITE_TEXT_ALIGNMENT_CENTER;
    case TextStyle::Align::Right:
        return DWRITE_TEXT_ALIGNMENT_TRAILING;
    case TextStyle::Align::Left:
    default:
        return DWRITE_TEXT_ALIGNMENT_LEADING;
    }
}

DWRITE_PARAGRAPH_ALIGNMENT toValignment(TextStyle::VAlign valign)
{
    switch (valign) {
    case TextStyle::VAlign::Top:
        return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    case TextStyle::VAlign::Bottom:
        return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
    case TextStyle::VAlign::Middle:
    default:
        return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    }
}

// Locale do usuario para IDWriteTextFormat (resolvida uma vez por processo).
const wchar_t* userLocale()
{
    static const std::wstring locale = [] {
        wchar_t buffer[LOCALE_NAME_MAX_LENGTH] = {};
        const UINT length = GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH);
        if (length > 0) {
            return std::wstring(buffer, length);
        }
        return std::wstring(L"en-us");
    }();
    return locale.c_str();
}

}  // namespace

// ---------------------------------------------------------------------------
// Singleton exigido pelo contrato.
// ---------------------------------------------------------------------------
Renderer& Renderer::instance()
{
    // Singleton "leaky": nunca destruido. A ordem de destruicao de estaticos
    // entre TUs e indeterminada e janelas (tambem estaticas) podem referenciar
    // o renderer durante o teardown do processo; o SO reclama os recursos no exit.
    static RendererD2d* renderer = new RendererD2d;
    return *renderer;
}

RendererD2d::~RendererD2d()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// D2dRenderTarget — ciclo de vida do wrapper
// ---------------------------------------------------------------------------
void D2dRenderTarget::bind(RendererD2d* owner, ID2D1HwndRenderTarget* target)
{
    if (target_ != target) {
        brush_.reset();   // pincel pertence ao target anterior
        clipDepth_ = 0;
    }
    owner_ = owner;
    target_ = target;
}

void D2dRenderTarget::unbind()
{
    owner_ = nullptr;
    target_ = nullptr;
    brush_.reset();
    clipDepth_ = 0;
}

// ---------------------------------------------------------------------------
// D2dRenderTarget — desenho
// ---------------------------------------------------------------------------
void D2dRenderTarget::clear(const Color& color)
{
    if (target_ == nullptr) {
        return;
    }
    target_->Clear(toD2d(color));
}

void D2dRenderTarget::fillRoundedRect(const Rect& rect, float radius, const Color& color)
{
    if (target_ == nullptr || owner_ == nullptr || !prepareBrush(color)) {
        return;
    }
    const float r = clampedRadius(radius);
    const D2D1_ROUNDED_RECT rounded{toRect(rect), r, r};
    ComRef<ID2D1RoundedRectangleGeometry> geometry;
    if (FAILED(owner_->d2dFactory_->CreateRoundedRectangleGeometry(rounded, geometry.put()))) {
        return;
    }
    target_->FillGeometry(geometry.get(), brush_.get());
}

void D2dRenderTarget::strokeRoundedRect(const Rect& rect, float radius, float strokeWidth,
                                        const Color& color)
{
    if (strokeWidth <= 0.f) {
        return;
    }
    if (target_ == nullptr || owner_ == nullptr || !prepareBrush(color)) {
        return;
    }
    const float r = clampedRadius(radius);
    const D2D1_ROUNDED_RECT rounded{toRect(rect), r, r};
    ComRef<ID2D1RoundedRectangleGeometry> geometry;
    if (FAILED(owner_->d2dFactory_->CreateRoundedRectangleGeometry(rounded, geometry.put()))) {
        return;
    }
    target_->DrawGeometry(geometry.get(), brush_.get(), strokeWidth);
}

void D2dRenderTarget::drawLine(float x1, float y1, float x2, float y2, float width,
                               const Color& color)
{
    if (width <= 0.f || target_ == nullptr || !prepareBrush(color)) {
        return;
    }
    target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush_.get(), width);
}

void D2dRenderTarget::drawText(std::wstring_view text, const Rect& box, const TextStyle& style)
{
    if (text.empty() || target_ == nullptr || owner_ == nullptr) {
        return;
    }
    IDWriteTextFormat* format = owner_->formatFor(style);
    if (format == nullptr || !prepareBrush(style.color)) {
        return;
    }
    // CLIP garante que o texto nunca vaza do box pedido.
    target_->DrawText(text.data(), static_cast<UINT32>(text.size()), format, toRect(box),
                      brush_.get(), D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
}

Size D2dRenderTarget::measureText(std::wstring_view text, const TextStyle& style)
{
    if (owner_ == nullptr || !owner_->dwriteFactory_) {
        return Size{0.f, 0.f};
    }
    IDWriteTextFormat* format = owner_->formatFor(style);
    if (format == nullptr) {
        return Size{0.f, 0.f};
    }

    const wchar_t* data = text.empty() ? L"" : text.data();
    ComRef<IDWriteTextLayout> layout;
    const HRESULT hr = owner_->dwriteFactory_->CreateTextLayout(
        data, static_cast<UINT32>(text.size()), format, kMeasureLimit, kMeasureLimit,
        layout.put());
    if (FAILED(hr) || !layout) {
        return Size{0.f, 0.f};
    }

    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout->GetMetrics(&metrics))) {
        return Size{0.f, 0.f};
    }
    return Size{std::ceil(metrics.widthIncludingTrailingWhitespace), std::ceil(metrics.height)};
}

void D2dRenderTarget::drawGlyph(std::wstring_view glyph, const Rect& box, const TextStyle& style)
{
    if (glyph.empty()) {
        return;
    }
    TextStyle iconStyle = style;
    iconStyle.fontFamily = owner_ != nullptr ? owner_->iconFamily()
                                              : std::wstring(tokens::kFontIcons);
    iconStyle.weight = tokens::kWeightRegular;   // fontes de icone so tem regular
    iconStyle.align = TextStyle::Align::Center;
    iconStyle.valign = TextStyle::VAlign::Middle;
    iconStyle.wrap = false;
    iconStyle.ellipsis = false;
    drawText(glyph, box, iconStyle);
}

void D2dRenderTarget::pushClip(const Rect& rect)
{
    if (target_ == nullptr) {
        return;
    }
    target_->PushAxisAlignedClip(toRect(rect), D2D1_ANTIALIAS_MODE_ALIASED);
    ++clipDepth_;
}

void D2dRenderTarget::popClip()
{
    if (target_ == nullptr || clipDepth_ <= 0) {
        return;
    }
    target_->PopAxisAlignedClip();
    --clipDepth_;
}

// ---------------------------------------------------------------------------
// D2dRenderTarget — apoio
// ---------------------------------------------------------------------------
bool D2dRenderTarget::prepareBrush(const Color& color)
{
    if (target_ == nullptr) {
        return false;
    }
    const D2D1_COLOR_F d2dColor = toD2d(color);
    if (!brush_) {
        return SUCCEEDED(target_->CreateSolidColorBrush(d2dColor, brush_.put()));
    }
    brush_->SetColor(d2dColor);
    return true;
}

// ---------------------------------------------------------------------------
// RendererD2d — factories
// ---------------------------------------------------------------------------
bool RendererD2d::initialize()
{
    if (initialized_) {
        return true;
    }
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, d2dFactory_.put()))) {
        return false;
    }

    const HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                           dwriteFactory_.putUnknown());
    if (FAILED(hr)) {
        d2dFactory_.reset();
        return false;
    }
    initialized_ = true;
    return true;
}

void RendererD2d::shutdown()
{
    for (auto& entry : windows_) {
        entry.second.surface.unbind();
        entry.second.target.reset();
        entry.second.painting = false;
    }
    windows_.clear();
    formats_.clear();
    dwriteFactory_.reset();
    d2dFactory_.reset();
    initialized_ = false;
}

// ---------------------------------------------------------------------------
// RendererD2d — janelas
// ---------------------------------------------------------------------------
RendererD2d::WindowState* RendererD2d::findWindow(HWND hwnd)
{
    const auto it = windows_.find(hwnd);
    return it == windows_.end() ? nullptr : &it->second;
}

bool RendererD2d::createTarget(WindowState& state)
{
    if (!d2dFactory_ || state.hwnd == nullptr) {
        return false;
    }

    RECT rect{};
    if (!GetClientRect(state.hwnd, &rect)) {
        return false;
    }
    const LONG width = rect.right - rect.left;
    const LONG height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return false;   // janela minimizada: sem target
    }

    const D2D1_SIZE_U size =
        D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height));
    // DPI fixo em 96: coordenadas do contrato = pixels fisicos literais.
    const D2D1_RENDER_TARGET_PROPERTIES properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(),
                                     kTargetDpi, kTargetDpi, D2D1_RENDER_TARGET_USAGE_NONE,
                                     D2D1_FEATURE_LEVEL_DEFAULT);
    const D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProperties =
        D2D1::HwndRenderTargetProperties(state.hwnd, size, D2D1_PRESENT_OPTIONS_NONE);

    ID2D1HwndRenderTarget* target = nullptr;
    const HRESULT hr = d2dFactory_->CreateHwndRenderTarget(properties, hwndProperties, &target);
    if (FAILED(hr) || target == nullptr) {
        return false;
    }
    state.target.attach(target);
    state.surface.bind(this, target);
    return true;
}

void RendererD2d::releaseTarget(WindowState& state)
{
    // ordem importa: o pincel segura o device vivo.
    state.surface.unbind();
    state.target.reset();
}

bool RendererD2d::attach(HWND hwnd)
{
    if (hwnd == nullptr) {
        return false;
    }
    if (!initialized_ && !initialize()) {
        return false;
    }

    WindowState* state = findWindow(hwnd);
    if (state == nullptr) {
        state = &windows_.try_emplace(hwnd).first->second;
        state->hwnd = hwnd;
    }
    if (state->target) {
        return true;   // ja anexada
    }
    state->detached = false;
    return createTarget(*state);
}

void RendererD2d::detach(HWND hwnd)
{
    WindowState* state = findWindow(hwnd);
    if (state == nullptr) {
        return;
    }
    // Se ha um paint em curso, o no permanece ate o endPaint (evita dangling;
    // a surface fica inerte e todo desenho vira no-op).
    const bool wasPainting = state->painting;
    state->detached = true;
    releaseTarget(*state);
    if (!wasPainting) {
        windows_.erase(hwnd);
    }
}

bool RendererD2d::resize(HWND hwnd, unsigned width, unsigned height)
{
    if (hwnd == nullptr || width == 0 || height == 0) {
        return false;
    }
    WindowState* state = findWindow(hwnd);
    if (state == nullptr || state->painting) {
        return false;   // Resize nao pode ocorrer entre BeginDraw/EndDraw
    }
    if (!state->target) {
        return createTarget(*state);   // recria ja com o client rect atual
    }
    if (FAILED(state->target->Resize(D2D1::SizeU(width, height)))) {
        releaseTarget(*state);   // proximo beginPaint recria
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// RendererD2d — ciclo de paint
// ---------------------------------------------------------------------------
IRenderTarget* RendererD2d::beginPaint(HWND hwnd, const RECT* /* updateRect */)
{
    if (hwnd == nullptr) {
        return nullptr;
    }
    if (!initialized_ || findWindow(hwnd) == nullptr) {
        // Auto-attach: a janela pode pintar antes do attach explicito.
        if (!attach(hwnd)) {
            return nullptr;
        }
    }

    WindowState* state = findWindow(hwnd);
    if (state == nullptr) {
        return nullptr;
    }
    if (!state->target && !createTarget(*state)) {
        return nullptr;
    }
    if (state->painting) {
        // Quadro anterior nao foi fechado (endPaint faltando): fecha agora.
        state->target->EndDraw();
        state->surface.unbind();
        state->painting = false;
    }

    // Sincroniza o tamanho real (WM_SIZE pode ter chegado sem resize()).
    RECT rect{};
    if (GetClientRect(hwnd, &rect)) {
        const unsigned width = static_cast<unsigned>(rect.right - rect.left);
        const unsigned height = static_cast<unsigned>(rect.bottom - rect.top);
        if (width > 0 && height > 0) {
            const D2D1_SIZE_U current = state->target->GetPixelSize();
            if (current.width != width || current.height != height) {
                if (FAILED(state->target->Resize(D2D1::SizeU(width, height)))) {
                    releaseTarget(*state);
                    return nullptr;
                }
            }
        }
    }

    state->target->BeginDraw();   // void no contrato Direct2D
    state->surface.bind(this, state->target.get());
    state->painting = true;
    return &state->surface;
}

void RendererD2d::endPaint(HWND hwnd)
{
    const auto it = windows_.find(hwnd);
    if (it == windows_.end()) {
        return;
    }
    WindowState& state = it->second;

    if (state.painting && state.target) {
        const HRESULT hr = state.target->EndDraw();
        state.painting = false;
        state.surface.unbind();
        if (hr == D2DERR_RECREATE_TARGET) {
            state.target.reset();   // proximo beginPaint recria o target
        }
    } else {
        state.painting = false;
        state.surface.unbind();
    }

    if (state.detached) {
        windows_.erase(it);
    }
}

// ---------------------------------------------------------------------------
// RendererD2d — cache de formatos de texto
// ---------------------------------------------------------------------------
const std::wstring& RendererD2d::iconFamily()
{
    if (!iconFamily_.empty()) {
        return iconFamily_;
    }
    iconFamily_ = tokens::kFontIcons;   // padrao: Win11
    ComRef<IDWriteFontCollection> collection;
    if (dwriteFactory_ &&
        SUCCEEDED(dwriteFactory_->GetSystemFontCollection(collection.put(), false))) {
        const auto exists = [&collection](const wchar_t* name) {
            UINT index = 0;
            BOOL found = FALSE;
            return SUCCEEDED(collection->FindFamilyName(name, &index, &found)) && found;
        };
        if (!exists(tokens::kFontIcons)) {
            // Win10: Segoe MDL2 Assets (mesmos codepoints U+E7xx/U+E9xx).
            if (exists(L"Segoe MDL2 Assets")) {
                iconFamily_ = L"Segoe MDL2 Assets";
            }
        }
    }
    return iconFamily_;
}

IDWriteTextFormat* RendererD2d::formatFor(const TextStyle& style)
{
    if (!dwriteFactory_) {
        return nullptr;
    }

    TextFormatKey key;
    key.family = style.fontFamily.empty() ? std::wstring(tokens::kFontUi) : style.fontFamily;
    key.fontSize = style.fontSize;
    key.weight = style.weight;
    key.align = static_cast<int>(toAlignment(style.align));
    key.valign = static_cast<int>(toValignment(style.valign));
    key.wrap = style.wrap;
    key.ellipsis = style.ellipsis;

    const auto cached = formats_.find(key);
    if (cached != formats_.end() && cached->second.format) {
        return cached->second.format.get();
    }

    TextFormatEntry entry;
    IDWriteTextFormat* rawFormat = nullptr;
    // Assinatura do SDK: (familia, colecao, peso, estilo, alongamento, tamanho,
    // locale, saida) — colecao nullptr = fontes do sistema.
    HRESULT hr = dwriteFactory_->CreateTextFormat(
        key.family.c_str(), nullptr, static_cast<DWRITE_FONT_WEIGHT>(key.weight),
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, key.fontSize, userLocale(),
        &rawFormat);
    if (FAILED(hr) || rawFormat == nullptr) {
        return nullptr;
    }
    entry.format.attach(rawFormat);

    rawFormat->SetTextAlignment(static_cast<DWRITE_TEXT_ALIGNMENT>(key.align));
    rawFormat->SetParagraphAlignment(static_cast<DWRITE_PARAGRAPH_ALIGNMENT>(key.valign));
    rawFormat->SetWordWrapping(key.wrap ? DWRITE_WORD_WRAPPING_WRAP
                                        : DWRITE_WORD_WRAPPING_NO_WRAP);

    if (key.ellipsis) {
        IDWriteInlineObject* sign = nullptr;
        if (SUCCEEDED(dwriteFactory_->CreateEllipsisTrimmingSign(rawFormat, &sign)) && sign) {
            entry.ellipsisSign.attach(sign);
            const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            rawFormat->SetTrimming(&trimming, sign);
        }
    }

    return formats_.emplace(std::move(key), std::move(entry)).first->second.format.get();
}

}  // namespace soundint::ui
