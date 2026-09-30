// ============================================================================
// Mixer flyout da tray (Track J) — conteudo, layout e eventos.
// Janela WindowBase topmost/toolwindow ancorada no clique da tray.
// ============================================================================
#include "ui/windows/mixer.h"

#include "core/log.h"
#include "ui/controls/controls.h"
#include "ui/i18n.h"
#include "ui/windows/window_base.h"

#include <shellapi.h>

#include <cmath>
#include <cwctype>
#include <memory>
#include <unordered_map>
#include <utility>

namespace soundint::ui::mixer {
namespace {

constexpr wchar_t kClassName[] = L"SoundIntMixerFlyout";
constexpr DWORD kSettleMs = 250;    // ignora perda de foco imediata ao mostrar
constexpr float kIconBox = 20.f;    // caixa do icone da linha
constexpr float kBtnSize = 32.f;    // botao-glyph (mute/rota/config)

// ---------------------------------------------------------------------------
// GlyphButton — botao apenas de glifo, com cor por tom (Normal/Danger/Accent).
// ---------------------------------------------------------------------------
class GlyphButton final : public controls::InteractiveControl {
public:
    enum class Tone : uint8_t { Normal, Danger, Accent };

    using ClickFn = std::function<void()>;

    void setGlyph(std::wstring glyph) { glyph_ = std::move(glyph); }
    void setTone(Tone tone) { tone_ = tone; }
    void setOnClick(ClickFn fn) { onClick_ = std::move(fn); }

    Size measure(float constraintWidth) override
    {
        return {constraintWidth, tokens::kControlHeight};
    }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override
    {
        if (!visible_) {
            return;
        }
        rt.fillRoundedRect(bounds_, tokens::kRadiusMd,
                           enabled_ ? palette.surface : palette.surfaceAlt);
        rt.strokeRoundedRect(bounds_, tokens::kRadiusMd, 1.f, palette.border);
        renderOverlay(rt, palette);

        Color fg = palette.textSecondary;
        if (enabled_) {
            switch (tone_) {
                case Tone::Danger: fg = palette.danger; break;
                case Tone::Accent: fg = palette.accent; break;
                case Tone::Normal: default: fg = palette.textPrimary; break;
            }
        }

        TextStyle ts;
        ts.fontFamily = tokens::kFontIcons;
        ts.fontSize = tokens::kFontBody;
        ts.weight = tokens::kWeightRegular;
        ts.color = fg;
        ts.align = TextStyle::Align::Center;
        ts.valign = TextStyle::VAlign::Middle;
        rt.drawGlyph(glyph_, bounds_, ts);
        renderFocus(rt, palette);
    }

    bool onPointer(const PointerEvent& e) override
    {
        if (!visible_) {
            return false;
        }
        switch (e.kind) {
            case PointerKind::Down:
                if (!enabled_ || e.button != MouseButton::Left ||
                    !bounds_.contains(e.x, e.y)) {
                    return false;
                }
                captured_ = true;
                pressed_ = true;
                hovered_ = true;
                invalidate();
                return true;

            case PointerKind::Move:
                if (captured_) {
                    pressed_ = bounds_.contains(e.x, e.y);
                    invalidate();
                    return true;
                }
                if (updateHover(e.x, e.y)) {
                    invalidate();
                }
                return hovered_;

            case PointerKind::Up: {
                if (!captured_) {
                    return false;
                }
                captured_ = false;
                pressed_ = false;
                const bool inside = bounds_.contains(e.x, e.y);
                invalidate();
                if (inside && enabled_ && onClick_) {
                    onClick_();
                }
                return true;
            }

            case PointerKind::Leave:
                hovered_ = false;
                pressed_ = false;
                invalidate();
                return false;

            default:
                return false;
        }
    }

    bool onKey(const KeyEvent& e) override
    {
        if (!visible_ || !enabled_ || !focused_ || !e.down) {
            return false;
        }
        if (e.key == Key::Enter || e.key == Key::Space) {
            if (onClick_) {
                onClick_();
            }
            invalidate();
            return true;
        }
        return false;
    }

private:
    std::wstring glyph_;
    Tone tone_ = Tone::Normal;
    ClickFn onClick_;
};

// ---------------------------------------------------------------------------
// SectionLabel — capsula de secao ou rotulo centralizado de estado vazio.
// ---------------------------------------------------------------------------
class SectionLabel final : public Control {
public:
    enum class Kind : uint8_t { Section, Empty };

    void setText(std::wstring text) { text_ = std::move(text); }
    void setKind(Kind kind) { kind_ = kind; }

    Size measure(float constraintWidth) override
    {
        const float h = (kind_ == Kind::Section) ? layout::kSectionH : layout::kEmptyH;
        return {constraintWidth, h};
    }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override
    {
        if (!visible_) {
            return;
        }
        TextStyle ts;
        ts.fontFamily = tokens::kFontUi;
        ts.align = TextStyle::Align::Left;
        ts.valign = TextStyle::VAlign::Middle;

        if (kind_ == Kind::Section) {
            rt.fillRoundedRect(bounds_, tokens::kRadiusSm, palette.surfaceAlt);
            ts.fontSize = tokens::kFontCaption;
            ts.weight = tokens::kWeightSemibold;
            ts.color = palette.textSecondary;
            const Rect box{bounds_.x + tokens::kSpace2, bounds_.y,
                           bounds_.w - tokens::kSpace2 * 2.f, bounds_.h};
            rt.drawText(text_, box, ts);
        } else {
            ts.fontSize = tokens::kFontBody;
            ts.weight = tokens::kWeightRegular;
            ts.color = palette.textSecondary;
            ts.align = TextStyle::Align::Center;
            rt.drawText(text_, bounds_, ts);
        }
    }

private:
    std::wstring text_;
    Kind kind_ = Kind::Section;
};

// ---------------------------------------------------------------------------
// Divider — linha horizontal em palette.border.
// ---------------------------------------------------------------------------
class Divider final : public Control {
public:
    Size measure(float constraintWidth) override
    {
        return {constraintWidth, layout::kDividerH};
    }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override
    {
        if (!visible_) {
            return;
        }
        const float y = bounds_.y + bounds_.h * 0.5f;
        rt.drawLine(bounds_.x, y, bounds_.x + bounds_.w, y, 1.f, palette.border);
    }
};

// ---------------------------------------------------------------------------
// OutputRow — ListRow de saida com o kIconCheck da default (accent) a direita.
// ---------------------------------------------------------------------------
class OutputRow final : public controls::ListRow {
public:
    bool isDefault = false;

    void render(IRenderTarget& rt, const tokens::Palette& palette) override
    {
        controls::ListRow::render(rt, palette);
        if (!isDefault) {
            return;
        }
        TextStyle ts;
        ts.fontFamily = tokens::kFontIcons;
        ts.fontSize = tokens::kFontBody;
        ts.weight = tokens::kWeightRegular;
        ts.color = palette.accent;
        ts.align = TextStyle::Align::Center;
        ts.valign = TextStyle::VAlign::Middle;
        const Rect box{bounds_.x + bounds_.w - tokens::kSpace2 - kIconBox, bounds_.y,
                       kIconBox, bounds_.h};
        rt.drawGlyph(tokens::kIconCheck, box, ts);
    }
};

// ---------------------------------------------------------------------------
// AppRow — linha de app: icone, nome, saida roteada, slider de volume, mute e
// dropdown de roteamento (bounds generosos quando expandido, regra do Track E).
// ---------------------------------------------------------------------------
class AppRow final : public Control {
public:
    struct Callbacks {
        std::function<void(float)> onVolume;                      // commit (Up/wheel/seta)
        std::function<void(bool)> onMute;                         // novo estado
        std::function<void(const std::wstring&)> onRoute;         // "" = padrao
        std::function<void(bool)> onExpanded;                     // dropdown aberto/fechado
    };

    void configure(SessionInfo session, std::wstring iconGlyph, std::wstring iconInitial,
                   std::wstring subtitle, std::vector<std::wstring> items,
                   std::vector<std::wstring> routeIds, size_t routeIndex,
                   bool routingEnabled, bool expanded, Callbacks callbacks)
    {
        session_ = std::move(session);
        iconGlyph_ = std::move(iconGlyph);
        iconInitial_ = std::move(iconInitial);
        subtitle_ = std::move(subtitle);
        routeIds_ = std::move(routeIds);
        routingEnabled_ = routingEnabled;
        callbacks_ = std::move(callbacks);
        committedVolume_ = session_.volume;

        route_.setItems(std::move(items));
        route_.setSelectedIndex(routeIndex);
        route_.setExpanded(routingEnabled_ && expanded);
        route_.setOnChanged([this](size_t index) {
            std::wstring id;
            if (index > 0 && index != controls::Dropdown::kNoIndex &&
                index - 1 < routeIds_.size()) {
                id = routeIds_[index - 1];
            }
            if (callbacks_.onRoute) {
                callbacks_.onRoute(id);
            }
        });

        mute_.setGlyph(session_.muted ? tokens::kIconSpeakerMute : tokens::kIconSpeaker);
        mute_.setTone(session_.muted ? GlyphButton::Tone::Danger
                                     : GlyphButton::Tone::Normal);
        mute_.setAccessibleName(tr(L"mixer.mute"));
        mute_.setOnClick([this] {
            if (callbacks_.onMute) {
                callbacks_.onMute(!session_.muted);
            }
        });

        routeBtn_.setGlyph(tokens::kIconChevronDown);
        routeBtn_.setOnClick([this] { route_.setExpanded(!route_.expanded()); });

        slider_.setValue(session_.volume);
        slider_.setAccessibleName(tr(L"mixer.volume"));
    }

    bool routeExpanded() const { return route_.expanded(); }

    void collapseRoute()
    {
        if (route_.expanded()) {
            route_.setExpanded(false);
            if (callbacks_.onExpanded) {
                callbacks_.onExpanded(false);
            }
        }
    }

    // Controle interativo sob o ponto (ordem: dropdown, mute, rota, slider).
    controls::InteractiveControl* hitChild(float x, float y)
    {
        layoutChildren();
        if (routingEnabled_ && route_.expanded()) {
            const Rect field = route_.fieldRect();
            const Rect pop = route_.popupRect();
            if (field.contains(x, y) || pop.contains(x, y)) {
                return &route_;
            }
        }
        if (mute_.bounds().contains(x, y)) {
            return &mute_;
        }
        if (routingEnabled_ && routeBtn_.bounds().contains(x, y)) {
            return &routeBtn_;
        }
        if (slider_.bounds().contains(x, y)) {
            return &slider_;
        }
        return nullptr;
    }

    void collectFocus(std::vector<controls::InteractiveControl*>& out)
    {
        out.push_back(&mute_);
        if (routingEnabled_) {
            out.push_back(&routeBtn_);
        }
        out.push_back(&slider_);
        if (routingEnabled_) {
            out.push_back(&route_);
        }
    }

    Size measure(float constraintWidth) override
    {
        float height = layout::kAppRowH;
        if (routingEnabled_ && route_.expanded()) {
            const float items = static_cast<float>(route_.items().size());
            height += layout::kGap * 2.f + tokens::kControlHeight * (1.f + items);
        }
        return {constraintWidth, height};
    }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override
    {
        if (!visible_) {
            return;
        }
        layoutChildren();
        rt.fillRoundedRect(bounds_, tokens::kRadiusMd, palette.surface);

        const float iconY = bounds_.y + (layout::kRowOutH - kIconBox) * 0.5f;
        const Rect iconBox{bounds_.x + tokens::kSpace2, iconY, kIconBox, kIconBox};
        if (iconGlyph_.empty()) {
            rt.fillRoundedRect(iconBox, tokens::kRadiusSm, palette.surfaceAlt);
            TextStyle initial;
            initial.fontFamily = tokens::kFontUi;
            initial.fontSize = tokens::kFontCaption;
            initial.weight = tokens::kWeightSemibold;
            initial.color = palette.accent;
            initial.align = TextStyle::Align::Center;
            initial.valign = TextStyle::VAlign::Middle;
            rt.drawText(iconInitial_, iconBox, initial);
        } else {
            TextStyle glyph;
            glyph.fontFamily = tokens::kFontIcons;
            glyph.fontSize = tokens::kFontBody;
            glyph.weight = tokens::kWeightRegular;
            glyph.color = palette.textSecondary;
            glyph.align = TextStyle::Align::Center;
            glyph.valign = TextStyle::VAlign::Middle;
            rt.drawGlyph(iconGlyph_, iconBox, glyph);
        }

        const float textX = iconBox.x + iconBox.w + tokens::kSpace2;
        const float textW = mute_.bounds().x - tokens::kSpace1 - textX;
        const float halfH = layout::kRowOutH * 0.5f;

        TextStyle title;
        title.fontFamily = tokens::kFontUi;
        title.fontSize = tokens::kFontBody;
        title.weight = tokens::kWeightSemibold;
        title.color = palette.textPrimary;
        title.align = TextStyle::Align::Left;
        title.valign = TextStyle::VAlign::Middle;

        if (subtitle_.empty()) {
            const Rect box{textX, bounds_.y, textW > 0.f ? textW : 0.f,
                           layout::kRowOutH};
            rt.drawText(session_.displayName, box, title);
        } else {
            const Rect tbox{textX, bounds_.y, textW > 0.f ? textW : 0.f, halfH};
            rt.drawText(session_.displayName, tbox, title);

            TextStyle sub;
            sub.fontFamily = tokens::kFontUi;
            sub.fontSize = tokens::kFontCaption;
            sub.weight = tokens::kWeightRegular;
            sub.color = palette.textSecondary;
            sub.align = TextStyle::Align::Left;
            sub.valign = TextStyle::VAlign::Middle;
            const Rect sbox{textX, bounds_.y + halfH, textW > 0.f ? textW : 0.f, halfH};
            rt.drawText(subtitle_, sbox, sub);
        }

        mute_.render(rt, palette);
        if (routingEnabled_) {
            routeBtn_.render(rt, palette);
        }
        slider_.render(rt, palette);
        if (routingEnabled_ && route_.expanded()) {
            route_.render(rt, palette);
        }
    }

    bool onPointer(const PointerEvent& e) override
    {
        if (!visible_) {
            return false;
        }
        layoutChildren();

        const bool wasExpanded = route_.expanded();
        if (wasExpanded && e.kind == PointerKind::Down) {
            const Rect field = route_.fieldRect();
            const Rect pop = route_.popupRect();
            if (!field.contains(e.x, e.y) && !pop.contains(e.x, e.y)) {
                route_.setExpanded(false);   // clique fora: recolhe sem consumir
            }
        }

        bool consumed = false;
        consumed = mute_.onPointer(e) || consumed;
        consumed = routeBtn_.onPointer(e) || consumed;
        consumed = slider_.onPointer(e) || consumed;
        if (routingEnabled_) {
            consumed = route_.onPointer(e) || consumed;
        }

        if (e.kind == PointerKind::Up || e.kind == PointerKind::Wheel) {
            commitVolume();
        }
        if (route_.expanded() != wasExpanded && callbacks_.onExpanded) {
            callbacks_.onExpanded(route_.expanded());
        }
        return consumed;
    }

    bool onKey(const KeyEvent& e) override
    {
        if (!visible_) {
            return false;
        }
        layoutChildren();
        const bool wasExpanded = route_.expanded();

        bool consumed = false;
        consumed = mute_.onKey(e) || consumed;
        consumed = routeBtn_.onKey(e) || consumed;
        consumed = slider_.onKey(e) || consumed;
        if (routingEnabled_) {
            consumed = route_.onKey(e) || consumed;
        }

        commitVolume();
        if (route_.expanded() != wasExpanded && callbacks_.onExpanded) {
            callbacks_.onExpanded(route_.expanded());
        }
        return consumed;
    }

private:
    void layoutChildren()
    {
        const float x = bounds_.x;
        const float y = bounds_.y;
        const float w = bounds_.w;
        const float right = x + w - tokens::kSpace2;
        const float btnY = y + (layout::kRowOutH - kBtnSize) * 0.5f;

        float muteRight = right;
        if (routingEnabled_) {
            routeBtn_.setBounds({right - kBtnSize, btnY, kBtnSize, kBtnSize});
            muteRight = right - kBtnSize - tokens::kSpace1;
        }
        mute_.setBounds({muteRight - kBtnSize, btnY, kBtnSize, kBtnSize});

        slider_.setBounds({x + tokens::kSpace2, y + layout::kRowOutH + tokens::kSpace1,
                           w - tokens::kSpace2 * 2.f, tokens::kControlHeight});

        const float dropY = y + layout::kAppRowH + tokens::kSpace1;
        route_.setBounds({x + tokens::kSpace2, dropY, w - tokens::kSpace2 * 2.f,
                          tokens::kControlHeight});
    }

    // Volume: aplicado em Up/Wheel/seta (nao no Move) para nao bombear o host.
    void commitVolume()
    {
        const float value = slider_.value();
        if (value == committedVolume_) {
            return;
        }
        committedVolume_ = value;
        if (callbacks_.onVolume) {
            callbacks_.onVolume(value);
        }
    }

    SessionInfo session_;
    std::wstring iconGlyph_;
    std::wstring iconInitial_;
    std::wstring subtitle_;
    std::vector<std::wstring> routeIds_;
    bool routingEnabled_ = true;
    float committedVolume_ = 1.f;
    Callbacks callbacks_;

    controls::Slider slider_;
    GlyphButton mute_;
    GlyphButton routeBtn_;
    controls::Dropdown route_;
};

// ---------------------------------------------------------------------------
// Altura do painel a partir do plano (compartilhado por testes e janela).
// ---------------------------------------------------------------------------
LayoutPlan planForContent(float content, float workHeight)
{
    LayoutPlan plan;
    plan.contentHeight = content;
    plan.panelTop = layout::kPad + layout::kHeaderH + layout::kHeaderGap;

    const float full = plan.panelTop + content + layout::kPad;
    const float maxH = workHeight * layout::kMaxWorkRatio;
    float windowH = (full < maxH) ? full : maxH;
    if (windowH < layout::kMinWindowH) {
        windowH = layout::kMinWindowH;
    }
    plan.windowHeight = windowH;

    plan.viewport = windowH - plan.panelTop - layout::kPad;
    if (plan.viewport < 0.f) {
        plan.viewport = 0.f;
    }
    plan.maxScroll = (content > plan.viewport) ? (content - plan.viewport) : 0.f;
    return plan;
}

// SHGetFileInfoW (cacheado): valida a imagem do exe do app.
bool exeIconAvailable(const std::wstring& path)
{
    static std::unordered_map<std::wstring, bool> cache;
    const auto it = cache.find(path);
    if (it != cache.end()) {
        return it->second;
    }

    SHFILEINFOW info{};
    const DWORD_PTR ok = SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info),
                                        SHGFI_ICON | SHGFI_SMALLICON);
    if (ok != 0 && info.hIcon != nullptr) {
        DestroyIcon(info.hIcon);
    }
    const bool available = (ok != 0);
    cache.emplace(path, available);
    return available;
}

// Avatar do app: inicial do nome quando a imagem existe; senao kIconInfo.
void appIconFor(const SessionInfo& session, std::wstring& glyph, std::wstring& initial)
{
    glyph = tokens::kIconInfo;
    initial.clear();

    const std::wstring& name =
        session.displayName.empty() ? session.processName : session.displayName;
    if (session.processPath.empty() || !exeIconAvailable(session.processPath)) {
        return;
    }
    for (wchar_t c : name) {
        if (std::iswalnum(c)) {
            initial.assign(1, static_cast<wchar_t>(std::towupper(c)));
            glyph.clear();
            return;
        }
    }
}

// Nome amigavel da saida roteada; vazio/desconhecido -> padrao do sistema.
std::wstring deviceLabel(const std::wstring& deviceId,
                         const std::vector<DeviceInfo>& outputs)
{
    if (!deviceId.empty()) {
        for (const auto& device : outputs) {
            if (device.id == deviceId) {
                return device.friendlyName;
            }
        }
    }
    return tr(L"mixer.defaultOutputLabel");
}

}  // namespace

// ---------------------------------------------------------------------------
// Auxiliares puros
// ---------------------------------------------------------------------------
RECT computeMixerRect(POINT anchor, Size size, const RECT& work)
{
    const float w = (size.w > 0.f) ? size.w : 0.f;
    const float h = (size.h > 0.f) ? size.h : 0.f;
    float x = static_cast<float>(anchor.x) - w - layout::kAnchorDx;
    float y = static_cast<float>(anchor.y) - h - layout::kAnchorDy;

    const float workLeft = static_cast<float>(work.left);
    const float workTop = static_cast<float>(work.top);
    const float workRight = static_cast<float>(work.right);
    const float workBottom = static_cast<float>(work.bottom);

    if (w >= workRight - workLeft) {
        x = workLeft;
    } else {
        if (x < workLeft) {
            x = workLeft;
        }
        if (x + w > workRight) {
            x = workRight - w;
        }
    }

    if (h >= workBottom - workTop) {
        y = workTop;
    } else {
        if (y < workTop) {
            y = workTop;
        }
        if (y + h > workBottom) {
            y = workBottom - h;
        }
    }

    RECT out{};
    out.left = static_cast<LONG>(std::lround(x));
    out.top = static_cast<LONG>(std::lround(y));
    out.right = static_cast<LONG>(std::lround(x + w));
    out.bottom = static_cast<LONG>(std::lround(y + h));
    return out;
}

float contentHeightFor(const ContentSpec& spec)
{
    std::vector<float> heights;
    heights.reserve(8);

    if (spec.outputRows > 0) {
        heights.push_back(layout::kSectionH);
        for (size_t i = 0; i < spec.outputRows; ++i) {
            heights.push_back(layout::kRowOutH);
        }
        heights.push_back(layout::kDividerH);
    }

    heights.push_back(layout::kSectionH);
    if (spec.appRows > 0) {
        for (size_t i = 0; i < spec.appRows; ++i) {
            heights.push_back(layout::kAppRowH);
        }
    } else {
        heights.push_back(layout::kEmptyH);
    }

    if (spec.systemRow) {
        heights.push_back(layout::kAppRowH);
    }

    float total = 0.f;
    for (size_t i = 0; i < heights.size(); ++i) {
        if (i > 0) {
            total += layout::kGap;
        }
        total += heights[i];
    }

    if (spec.dropdownItems > 0) {
        const float items = static_cast<float>(spec.dropdownItems);
        total += layout::kGap * 2.f + tokens::kControlHeight * (1.f + items);
    }
    return total;
}

LayoutPlan planLayout(const ContentSpec& spec, float workHeight)
{
    return planForContent(contentHeightFor(spec), workHeight);
}

std::vector<SessionInfo> orderSessions(const std::vector<SessionInfo>& sessions)
{
    std::vector<SessionInfo> active;
    std::vector<SessionInfo> inactive;
    std::vector<SessionInfo> system;

    for (const auto& session : sessions) {
        if (session.systemSounds) {
            system.push_back(session);
            continue;
        }
        if (session.active) {
            active.push_back(session);
        } else {
            inactive.push_back(session);
        }
    }

    std::vector<SessionInfo> ordered;
    ordered.reserve(active.size() + inactive.size() + system.size());
    ordered.insert(ordered.end(), active.begin(), active.end());
    ordered.insert(ordered.end(), inactive.begin(), inactive.end());
    ordered.insert(ordered.end(), system.begin(), system.end());
    return ordered;
}

SessionPlan planSessions(const std::vector<SessionInfo>& sessions)
{
    SessionPlan plan;
    const std::vector<SessionInfo> ordered = orderSessions(sessions);
    for (const auto& session : ordered) {
        if (session.systemSounds) {
            if (!plan.systemSounds) {
                plan.systemSounds = true;
                plan.system = session;
            }
            continue;
        }
        if (session.active) {
            plan.apps.push_back(session);
        }
    }
    return plan;
}

std::vector<std::wstring> routeItems(const std::vector<DeviceInfo>& outputs)
{
    std::vector<std::wstring> items;
    items.reserve(outputs.size() + 1);
    items.emplace_back(tr(L"mixer.defaultOutputLabel"));
    for (const auto& device : outputs) {
        items.push_back(device.friendlyName);
    }
    return items;
}

size_t routeSelectionIndex(const std::vector<DeviceInfo>& outputs,
                           const std::wstring& deviceId)
{
    if (deviceId.empty()) {
        return 0;
    }
    for (size_t i = 0; i < outputs.size(); ++i) {
        if (outputs[i].id == deviceId) {
            return i + 1;
        }
    }
    return 0;
}

std::wstring outputGlyphFor(const std::wstring& friendlyName)
{
    std::wstring lower;
    lower.reserve(friendlyName.size());
    for (wchar_t c : friendlyName) {
        lower.push_back(static_cast<wchar_t>(std::towlower(c)));
    }

    const wchar_t* needles[] = {L"head", L"fone", L"auricul", L"airpod"};
    for (const wchar_t* needle : needles) {
        if (lower.find(needle) != std::wstring::npos) {
            return tokens::kIconHeadphones;
        }
    }
    return tokens::kIconSpeaker;
}

// ---------------------------------------------------------------------------
// MixerWindow — janela do flyout
// ---------------------------------------------------------------------------
class MixerWindow final : public WindowBase {
public:
    explicit MixerWindow(MixerHost host) : host_(std::move(host))
    {
        panel_.setGap(layout::kGap);

        settingsBtn_.setGlyph(tokens::kIconSettings);
        settingsBtn_.setAccessibleName(tr(L"mixer.title"));
        settingsBtn_.setOnClick([this] {
            if (host_.openSettings) {
                host_.openSettings();
            }
            requestRefresh();
        });
    }

    bool createWindow()
    {
        if (valid()) {
            return true;
        }
        if (!create(GetModuleHandleW(nullptr), kClassName, WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                    WS_POPUP)) {
            SI_LOG_ERROR("mixer", L"Falha ao criar a janela do mixer");
            return false;
        }
        return true;
    }

    void applyHost(MixerHost host) { host_ = std::move(host); }

    void showFlyout(POINT anchor)
    {
        anchor_ = anchor;
        refresh();

        RECT work{};
        if (!workAreaAt(anchor_, work)) {
            SI_LOG_WARN("mixer", L"Area de trabalho indisponivel para o mixer");
            return;
        }
        const float width = layout::kWidth * dpiScale();
        const RECT rect = computeMixerRect(anchor_, {width, plan_.windowHeight}, work);
        showAt(rect.left, rect.top, true);   // ativa: teclado/Escape funcionam
        SetWindowPos(hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        shownTick_ = GetTickCount();
        invalidate();
    }

    // Reconstruicao adiada enquanto ha dispatch de evento ou captura de mouse.
    void requestRefresh()
    {
        if (dispatchDepth_ > 0 || capture_ != nullptr) {
            refreshPending_ = true;
            return;
        }
        refresh();
    }

    void refresh()
    {
        refreshPending_ = false;
        if (!valid()) {
            return;
        }

        outputs_.clear();
        defaultId_.clear();
        if (host_.outputs) {
            outputs_ = host_.outputs();
        }
        if (host_.defaultOutput) {
            defaultId_ = host_.defaultOutput();
        }

        std::vector<SessionInfo> sessions;
        if (host_.sessions) {
            sessions = host_.sessions();
        }
        const SessionPlan plan = planSessions(sessions);
        const std::vector<std::wstring> labels = routeItems(outputs_);

        pool_.clear();
        panel_.clear();
        focusables_.clear();
        expandedRow_ = nullptr;
        contentRun_ = 0.f;
        expandedRowOffset_ = 0.f;
        expandedRowHeight_ = 0.f;

        ContentSpec spec;
        spec.outputRows = outputs_.size();

        if (!outputs_.empty()) {
            addSection(tr(L"mixer.outputs"));
        }
        for (const auto& device : outputs_) {
            addOutputRow(device);
        }
        if (!outputs_.empty()) {
            addDivider();
        }

        addSection(tr(L"mixer.apps"));
        spec.appRows = plan.apps.size();
        if (plan.apps.empty()) {
            addEmpty(tr(L"mixer.empty"));
        } else {
            for (const auto& session : plan.apps) {
                addAppRow(session, labels, false);
            }
        }
        if (plan.systemSounds) {
            spec.systemRow = true;
            addAppRow(plan.system, labels, true);
        }
        if (expandedRow_ == nullptr) {
            expandedInstance_.clear();
        } else {
            spec.dropdownItems = labels.size();
        }

        RECT work{};
        if (!workAreaAt(anchor_, work)) {
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        }
        const float workHeight = static_cast<float>(work.bottom - work.top);
        plan_ = planLayout(spec, workHeight);

        const float width = layout::kWidth * dpiScale();
        resizeClient(static_cast<int>(std::lround(width)),
                     static_cast<int>(std::lround(plan_.windowHeight)));
        const Rect client = clientBounds();
        updatePanelBounds(client.w, client.h);
        plan_.viewport = panel_.bounds().h;

        panel_.setScrollY(scrollY_);
        if (expandedRow_ != nullptr && plan_.maxScroll > 0.f) {
            const float visible = panel_.bounds().h;
            const float rowBottom = expandedRowOffset_ + expandedRowHeight_;
            if (rowBottom > panel_.scrollY() + visible) {
                panel_.setScrollY(rowBottom - visible);
            }
        }
        scrollY_ = panel_.scrollY();
        restoreFocus();

        if (isVisible()) {
            const RECT rect = computeMixerRect(anchor_, {width, plan_.windowHeight}, work);
            SetWindowPos(hwnd(), HWND_TOPMOST, rect.left, rect.top, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE);
        }
        invalidate();
    }

protected:
    bool handleMessage(UINT msg, WPARAM w, LPARAM l, LRESULT& result) override
    {
        (void)w;
        (void)l;
        switch (msg) {
            case WM_CLOSE:
                hide();
                result = 0;
                return true;
            case WM_CAPTURECHANGED:
                capture_ = nullptr;
                applyPendingRefresh();
                result = 0;
                return true;
            default:
                break;
        }
        return false;
    }

    void onRender(IRenderTarget& rt, const Rect& client) override
    {
        const tokens::Palette pal = palette();
        rt.clear(pal.windowBg);

        TextStyle icon;
        icon.fontFamily = tokens::kFontIcons;
        icon.fontSize = tokens::kFontBody;
        icon.weight = tokens::kWeightRegular;
        icon.color = pal.accent;
        icon.align = TextStyle::Align::Center;
        icon.valign = TextStyle::VAlign::Middle;
        const Rect iconBox{layout::kPad, layout::kPad, kIconBox, layout::kHeaderH};
        rt.drawGlyph(tokens::kIconSpeaker, iconBox, icon);

        TextStyle title;
        title.fontFamily = tokens::kFontUi;
        title.fontSize = tokens::kFontSubtitle;
        title.weight = tokens::kWeightSemibold;
        title.color = pal.textPrimary;
        title.align = TextStyle::Align::Left;
        title.valign = TextStyle::VAlign::Middle;
        const float titleX = layout::kPad + kIconBox + tokens::kSpace2;
        const float titleW = client.w - titleX - layout::kPad - kBtnSize - tokens::kSpace2;
        const Rect titleBox{titleX, layout::kPad, titleW > 0.f ? titleW : 0.f,
                            layout::kHeaderH};
        rt.drawText(tr(L"mixer.title"), titleBox, title);

        settingsBtn_.render(rt, pal);
        panel_.render(rt, pal);
    }

    void onActivate(bool active) override
    {
        if (active) {
            return;
        }
        // Evita fechar por uma perda de foco transitoria logo apos mostrar.
        if (GetTickCount() - shownTick_ < kSettleMs) {
            return;
        }
        hide();
    }

    void onSize(unsigned w, unsigned h) override
    {
        updatePanelBounds(static_cast<float>(w), static_cast<float>(h));
        invalidate();
    }

    bool onPointer(const PointerEvent& e) override
    {
        DispatchGuard guard(this);

        switch (e.kind) {
            case PointerKind::Down: {
                // Dropdown aberto: clique fora dele recolhe antes do dispatch.
                if (expandedRow_ != nullptr &&
                    !expandedRow_->bounds().contains(e.x, e.y)) {
                    expandedRow_->collapseRoute();
                }

                Control* target = nullptr;
                controls::InteractiveControl* focusTarget = nullptr;
                if (settingsBtn_.bounds().contains(e.x, e.y)) {
                    target = &settingsBtn_;
                    focusTarget = &settingsBtn_;
                } else if (panel_.bounds().contains(e.x, e.y)) {
                    target = &panel_;
                    focusTarget = hitInteractive(e.x, e.y);
                }
                if (target == nullptr) {
                    setFocusTo(nullptr);
                    return false;
                }

                setFocusTo(focusTarget);
                capture_ = target;   // trava o refresh durante o dispatch
                const bool consumed = target->onPointer(e);
                if (consumed) {
                    captureMouse();
                } else {
                    capture_ = nullptr;
                }
                return consumed;
            }

            case PointerKind::Move: {
                if (capture_ != nullptr) {
                    return capture_->onPointer(e);
                }
                bool consumed = settingsBtn_.onPointer(e);
                // Despacha para todos os filhos (sem short-circuit) para que o
                // hover de uma linha saia quando o cursor entra em outra.
                panel_.layout();
                for (Control* child : panel_.children()) {
                    if (!child->visible()) {
                        continue;
                    }
                    const bool childConsumed = child->onPointer(e);
                    consumed = consumed || childConsumed;
                }
                return consumed;
            }

            case PointerKind::Up:
            case PointerKind::Leave: {
                if (capture_ == nullptr) {
                    return false;
                }
                Control* target = capture_;
                const bool consumed = target->onPointer(e);
                if (e.kind == PointerKind::Up) {
                    releaseMouse();
                }
                capture_ = nullptr;
                return consumed;
            }

            case PointerKind::Wheel: {
                if (!panel_.bounds().contains(e.x, e.y)) {
                    return false;
                }
                return panel_.onPointer(e);
            }

            default:
                return false;
        }
    }

    bool onKey(const KeyEvent& e) override
    {
        DispatchGuard guard(this);

        if (e.down && e.key == Key::Tab) {
            cycleFocus(e.shift);
            return true;
        }

        bool consumed = false;
        if (focusIndex_ >= 0 &&
            focusIndex_ < static_cast<ptrdiff_t>(focusables_.size())) {
            consumed = focusables_[static_cast<size_t>(focusIndex_)]->onKey(e);
        } else {
            consumed = panel_.onKey(e);
        }
        if (consumed) {
            return true;
        }

        if (e.down && e.key == Key::Escape) {
            if (expandedRow_ != nullptr) {
                expandedRow_->collapseRoute();
                requestRefresh();
            } else {
                hide();
            }
            return true;
        }
        return false;
    }

private:
    // RAII: adia o refresh enquanto um evento esta sendo despachado.
    struct DispatchGuard {
        explicit DispatchGuard(MixerWindow* window) : window_(window)
        {
            ++window_->dispatchDepth_;
        }
        ~DispatchGuard() { window_->endDispatch(); }

        DispatchGuard(const DispatchGuard&) = delete;
        DispatchGuard& operator=(const DispatchGuard&) = delete;

    private:
        MixerWindow* window_;
    };

    void endDispatch()
    {
        if (dispatchDepth_ > 0) {
            --dispatchDepth_;
        }
        applyPendingRefresh();
    }

    void applyPendingRefresh()
    {
        if (refreshPending_ && dispatchDepth_ == 0 && capture_ == nullptr) {
            refresh();
        }
    }

    void updatePanelBounds(float clientW, float clientH)
    {
        const float panelY = layout::kPad + layout::kHeaderH + layout::kHeaderGap;
        float panelW = clientW - layout::kPad * 2.f;
        float panelH = clientH - panelY - layout::kPad;
        if (panelW < 0.f) {
            panelW = 0.f;
        }
        if (panelH < 0.f) {
            panelH = 0.f;
        }
        panel_.setBounds({layout::kPad, panelY, panelW, panelH});
        settingsBtn_.setBounds({clientW - layout::kPad - kBtnSize, layout::kPad, kBtnSize,
                                kBtnSize});
    }

    controls::InteractiveControl* hitInteractive(float x, float y)
    {
        panel_.layout();
        const auto& children = panel_.children();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            Control* child = *it;
            if (!child->visible() || !child->bounds().contains(x, y)) {
                continue;
            }
            if (auto* row = dynamic_cast<AppRow*>(child)) {
                return row->hitChild(x, y);
            }
            if (auto* interactive = dynamic_cast<controls::InteractiveControl*>(child)) {
                return interactive;
            }
            return nullptr;
        }
        return nullptr;
    }

    void setFocusTo(controls::InteractiveControl* target)
    {
        for (controls::InteractiveControl* control : focusables_) {
            control->setFocused(control == target);
        }
        focusIndex_ = -1;
        for (size_t i = 0; i < focusables_.size(); ++i) {
            if (focusables_[i] == target) {
                focusIndex_ = static_cast<ptrdiff_t>(i);
                break;
            }
        }
        invalidate();
    }

    void cycleFocus(bool backwards)
    {
        if (focusables_.empty()) {
            return;
        }
        const auto count = static_cast<ptrdiff_t>(focusables_.size());
        ptrdiff_t next = focusIndex_ + (backwards ? -1 : 1);
        if (next < 0) {
            next = count - 1;
        }
        if (next >= count) {
            next = 0;
        }
        setFocusTo(focusables_[static_cast<size_t>(next)]);
    }

    void restoreFocus()
    {
        if (focusables_.empty()) {
            focusIndex_ = -1;
            return;
        }
        const auto count = static_cast<ptrdiff_t>(focusables_.size());
        if (focusIndex_ < 0 || focusIndex_ >= count) {
            focusIndex_ = -1;
        }
        for (size_t i = 0; i < focusables_.size(); ++i) {
            focusables_[i]->setFocused(static_cast<ptrdiff_t>(i) == focusIndex_);
        }
    }

    void addControl(std::unique_ptr<Control> control)
    {
        Control* raw = control.get();
        pool_.push_back(std::move(control));
        panel_.addControl(raw);

        if (auto* interactive = dynamic_cast<controls::InteractiveControl*>(raw)) {
            focusables_.push_back(interactive);
        }
        if (auto* row = dynamic_cast<AppRow*>(raw)) {
            row->collectFocus(focusables_);
        }

        const float height = raw->measure(layout::kWidth).h;
        contentRun_ += (panel_.children().size() > 1 ? layout::kGap : 0.f) + height;
    }

    float nextChildY() const
    {
        const bool first = panel_.children().empty();
        return contentRun_ + (first ? 0.f : layout::kGap);
    }

    void addSection(const wchar_t* text)
    {
        auto label = std::make_unique<SectionLabel>();
        label->setKind(SectionLabel::Kind::Section);
        label->setText(text);
        addControl(std::move(label));
    }

    void addEmpty(const wchar_t* text)
    {
        auto label = std::make_unique<SectionLabel>();
        label->setKind(SectionLabel::Kind::Empty);
        label->setText(text);
        addControl(std::move(label));
    }

    void addDivider() { addControl(std::make_unique<Divider>()); }

    void addOutputRow(const DeviceInfo& device)
    {
        auto row = std::make_unique<OutputRow>();
        row->setTitle(device.friendlyName);
        row->setGlyph(outputGlyphFor(device.friendlyName));
        row->isDefault = (device.id == defaultId_);
        row->setAccessibleName(device.friendlyName);

        const std::wstring deviceId = device.id;
        row->setOnClick([this, deviceId] {
            if (host_.setDefault) {
                host_.setDefault(deviceId);
            }
            requestRefresh();
        });
        addControl(std::move(row));
    }

    void addAppRow(const SessionInfo& session, const std::vector<std::wstring>& labels,
                   bool systemRow)
    {
        SessionInfo info = session;
        std::wstring glyph;
        std::wstring initial;
        if (systemRow) {
            info.displayName = tr(L"mixer.systemSounds");
            glyph = tokens::kIconSpeaker;
        } else {
            appIconFor(info, glyph, initial);
        }

        const bool routingEnabled = !systemRow;
        std::wstring subtitle;
        if (routingEnabled) {
            subtitle = deviceLabel(info.deviceId, outputs_);
        }

        std::vector<std::wstring> routeIds;
        routeIds.reserve(outputs_.size());
        for (const auto& device : outputs_) {
            routeIds.push_back(device.id);
        }

        const std::wstring instanceId = info.instanceId;
        const uint32_t pid = info.pid;
        const bool expanded = routingEnabled && !expandedInstance_.empty() &&
                              expandedInstance_ == info.instanceId;

        auto row = std::make_unique<AppRow>();
        AppRow* raw = row.get();

        AppRow::Callbacks callbacks;
        callbacks.onVolume = [this, instanceId](float volume) {
            if (host_.setVolume) {
                host_.setVolume(instanceId, volume);
            }
            requestRefresh();
        };
        callbacks.onMute = [this, instanceId](bool muted) {
            if (host_.setMute) {
                host_.setMute(instanceId, muted);
            }
            requestRefresh();
        };
        callbacks.onRoute = [this, pid](const std::wstring& deviceId) {
            if (host_.routeApp) {
                host_.routeApp(pid, deviceId);
            }
            requestRefresh();
        };
        callbacks.onExpanded = [this, instanceId, raw](bool open) {
            if (open) {
                expandedInstance_ = instanceId;
                expandedRow_ = raw;
            } else if (expandedInstance_ == instanceId) {
                expandedInstance_.clear();
                expandedRow_ = nullptr;
            }
            requestRefresh();
        };

        if (expanded) {
            expandedRowOffset_ = nextChildY();
        }
        row->configure(std::move(info), std::move(glyph), std::move(initial),
                       std::move(subtitle), labels, std::move(routeIds),
                       routeSelectionIndex(outputs_, session.deviceId), routingEnabled,
                       expanded, std::move(callbacks));
        if (expanded) {
            expandedRowHeight_ = row->measure(layout::kWidth).h;
            expandedRow_ = raw;
        }
        addControl(std::move(row));
    }

    MixerHost host_;
    POINT anchor_{};
    DWORD shownTick_ = 0;

    controls::ListPanel panel_;
    GlyphButton settingsBtn_;
    std::vector<std::unique_ptr<Control>> pool_;
    std::vector<controls::InteractiveControl*> focusables_;
    ptrdiff_t focusIndex_ = -1;

    Control* capture_ = nullptr;
    int dispatchDepth_ = 0;
    bool refreshPending_ = false;

    std::vector<DeviceInfo> outputs_;
    std::wstring defaultId_;

    AppRow* expandedRow_ = nullptr;
    std::wstring expandedInstance_;
    float expandedRowOffset_ = 0.f;
    float expandedRowHeight_ = 0.f;

    float contentRun_ = 0.f;
    float scrollY_ = 0.f;
    LayoutPlan plan_{};
};

// ---------------------------------------------------------------------------
// Estado do modulo (singleton) e API publica
// ---------------------------------------------------------------------------
namespace {

struct Module {
    EventBus* bus = nullptr;
    EventBus::SubscriptionId subscription = 0;
    MixerHost host;
    bool initialized = false;
    std::unique_ptr<MixerWindow> window;
};

Module& module()
{
    static Module state;
    return state;
}

void onAppEvent(const AppEvent&)
{
    // DeviceEvent (qualquer) -> saidas; SessionEvent -> sessoes + invalidate.
    Module& state = module();
    if (state.window) {
        state.window->requestRefresh();
    }
}

}  // namespace

void init(EventBus& bus, MixerHost host)
{
    Module& state = module();
    if (state.bus != &bus || state.subscription == 0) {
        if (state.bus != nullptr && state.subscription != 0) {
            state.bus->unsubscribe(state.subscription);
        }
        state.subscription = bus.subscribe(&onAppEvent);
        state.bus = &bus;
    }
    state.host = std::move(host);
    state.initialized = true;
    if (state.window) {
        state.window->applyHost(state.host);
    }
}

void toggleAt(POINT anchor)
{
    Module& state = module();
    if (!state.initialized) {
        return;
    }
    if (!state.window) {
        auto window = std::make_unique<MixerWindow>(state.host);
        if (!window->createWindow()) {
            return;
        }
        state.window = std::move(window);
    }
    if (state.window->isVisible()) {
        state.window->hide();
        return;
    }
    state.window->showFlyout(anchor);
}

void hide()
{
    Module& state = module();
    if (state.window) {
        state.window->hide();
    }
}

bool isVisible()
{
    Module& state = module();
    return state.window && state.window->isVisible();
}

void shutdown()
{
    Module& state = module();
    if (state.bus != nullptr && state.subscription != 0) {
        state.bus->unsubscribe(state.subscription);
    }
    state.subscription = 0;
    state.bus = nullptr;
    state.initialized = false;
    state.host = MixerHost{};
    if (state.window) {
        state.window->destroy();
        state.window.reset();
    }
}

}  // namespace soundint::ui::mixer
