// ============================================================================
// Pagina Atualizacoes (Track K): autoCheckUpdates, betaChannel e verificacao
// sob demanda via host().checkForUpdates (assincrono; resultado chega por
// kMsgUpdateResult na thread principal).
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"

namespace soundint::ui::settings {
namespace {

class UpdatesPage : public SettingsPage {
public:
    UpdatesPage()
    {
        titleKey_ = L"settings.updates.title";
        autoCheckToggle_.setOnChanged([](bool value) {
            auto& store = core::Store::instance();
            store.settings().autoCheckUpdates = value;
            store.save();
            requestRender();
        });
        betaToggle_.setOnChanged([](bool value) {
            auto& store = core::Store::instance();
            store.settings().betaChannel = value;
            store.save();
            requestRender();
        });
        checkButton_.setOnClick([this]() { checkNow(); });

        controls_.push_back(&autoCheckToggle_);
        controls_.push_back(&betaToggle_);
        controls_.push_back(&checkButton_);
        refresh();
    }

    void updateResult(bool found, const std::wstring& info) override
    {
        state_ = found ? State::Available : State::UpToDate;
        info_ = info;
        requestRender();
    }

protected:
    void retranslateImpl() override
    {
        autoCheckLabel_ = tr(L"settings.updates.autoCheck");
        betaLabel_ = tr(L"settings.updates.betaChannel");
        checkButton_.setText(tr(L"settings.updates.checkNow"));
        checkButton_.setGlyph(tokens::kIconRefresh);
        current_ = tr(L"settings.updates.current");
        // info_ nao e traduzido (vem do update module) — preserva entre idiomas.
    }

    void syncImpl() override
    {
        const soundint::Settings& settings = core::Store::instance().settings();
        autoCheckToggle_.setValue(settings.autoCheckUpdates);
        betaToggle_.setValue(settings.betaChannel);
    }

    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    enum class State { Idle, Checking, Available, UpToDate, NoService };

    void checkNow()
    {
        if (!host().checkForUpdates) {
            state_ = State::NoService;
            info_.clear();
            requestRender();
            return;
        }
        state_ = State::Checking;
        info_.clear();
        requestRender();
        host().checkForUpdates(
            [](bool found, const std::wstring& info) { postUpdateResult(found, info); });
    }

    std::wstring statusText() const
    {
        switch (state_) {
            case State::Checking:
                return tr(L"settings.updates.checking");
            case State::Available: {
                std::wstring text = tr(L"settings.updates.available");
                if (!info_.empty()) {
                    text += L' ';
                    text += info_;
                }
                return text;
            }
            case State::UpToDate:
                return tr(L"settings.updates.upToDate");
            case State::NoService:
                return tr(L"settings.updates.noService");
            case State::Idle:
            default:
                return std::wstring{};
        }
    }

    controls::Toggle autoCheckToggle_;
    controls::Toggle betaToggle_;
    controls::Button checkButton_;

    std::wstring autoCheckLabel_;
    std::wstring betaLabel_;
    std::wstring current_;
    std::wstring info_;

    State state_ = State::Idle;
    Rect autoRect_{};
    Rect betaRect_{};
    Rect statusRect_{};
    Rect currentRect_{};
};

void UpdatesPage::layoutImpl(const Rect& area, float /*scale*/)
{
    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float toggleW = s(56.f);
    const float toggleH = s(tokens::kControlHeight);
    const float labelW = area.w - toggleW - s(16.f);
    const float buttonW = s(170.f);
    const float buttonH = s(tokens::kControlHeight);

    float y = y0;
    autoRect_ = {area.x, y, labelW, rowH};
    autoCheckToggle_.setBounds({area.x + area.w - toggleW, y + (rowH - toggleH) * 0.5f,
                                toggleW, toggleH});
    y += rowH;

    betaRect_ = {area.x, y, labelW, rowH};
    betaToggle_.setBounds({area.x + area.w - toggleW, y + (rowH - toggleH) * 0.5f,
                           toggleW, toggleH});
    y += rowH + s(8.f);

    checkButton_.setBounds({area.x, y, buttonW, buttonH});
    y += buttonH + s(8.f);

    statusRect_ = {area.x, y, area.w, s(20.f)};
    y += s(20.f) + s(4.f);

    currentRect_ = {area.x, y, area.w, s(20.f)};
    y += s(20.f);

    contentH_ = y - y0;
}

void UpdatesPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    drawText(rt, autoRect_, autoCheckLabel_, palette, tokens::kFontBody,
             tokens::kWeightRegular, palette.textPrimary);
    drawText(rt, betaRect_, betaLabel_, palette, tokens::kFontBody,
             tokens::kWeightRegular, palette.textPrimary);

    const std::wstring status = statusText();
    if (!status.empty()) {
        Color color = palette.textSecondary;
        if (state_ == State::Available) {
            color = palette.success;
        } else if (state_ == State::Checking) {
            color = palette.accent;
        } else if (state_ == State::NoService) {
            color = palette.danger;
        }
        drawText(rt, statusRect_, status, palette, tokens::kFontBody,
                 tokens::kWeightRegular, color);
    }

    drawText(rt, currentRect_, current_, palette, tokens::kFontCaption,
             tokens::kWeightRegular, palette.textSecondary);
}

}  // namespace

std::unique_ptr<SettingsPage> createUpdatesPage()
{
    return std::make_unique<UpdatesPage>();
}

}  // namespace soundint::ui::settings
