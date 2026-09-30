// ============================================================================
// Pagina Regras (Track K): lista de regras por app + adicao/remocao.
// Sem audio no track: o destino exibe o deviceId textual (vazio = padrao).
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace soundint::ui::settings {
namespace {

struct RuleRow {
    std::wstring processName;
    std::unique_ptr<controls::Toggle> enabled;
    std::unique_ptr<controls::Button> remove;
};

struct RowGeom {
    Rect row{};
    Rect name{};
    Rect arrow{};
    Rect target{};
    Rect toggle{};
    Rect remove{};
};

class RulesPage : public SettingsPage {
public:
    RulesPage()
    {
        titleKey_ = L"settings.rules.title";
        addButton_.setOnClick([this]() { addRule(); });
        controls_.push_back(&addButton_);
        addField(&field_);
        rebuildRows();
        refresh();
    }

protected:
    void retranslateImpl() override
    {
        hint_ = tr(L"settings.rules.hint");
        field_.setPlaceholder(tr(L"settings.rules.placeholder"));
        addButton_.setText(tr(L"settings.rules.add"));
        status_.clear();
        rebuildRows();
    }

    void syncImpl() override
    {
        rebuildRows();
    }

    bool charImpl(wchar_t c) override
    {
        if (c == L'\r' && field_.focused()) {
            addRule();
            return true;
        }
        return false;
    }

    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    void rebuildRows();
    void addRule();

    TextField field_;
    controls::Button addButton_;
    std::vector<std::unique_ptr<RuleRow>> rows_;
    std::vector<RowGeom> geoms_;

    std::wstring hint_;
    std::wstring status_;
    Rect hintRect_{};
    Rect statusRect_{};
    Rect emptyRect_{};
    bool empty_ = false;
};

void RulesPage::rebuildRows()
{
    // Remove os controles antigos do vetor antes de destrui-los.
    controls_.clear();
    controls_.push_back(&addButton_);
    controls_.push_back(&field_);
    rows_.clear();

    const auto& rules = core::Store::instance().rules();
    for (const soundint::AppRule& rule : rules) {
        auto row = std::make_unique<RuleRow>();
        row->processName = rule.processName;

        row->enabled = std::make_unique<controls::Toggle>();
        row->enabled->setValue(rule.enabled);
        row->enabled->setAccessibleName(tr(L"settings.rules.enabled"));
        row->enabled->setOnChanged([this, name = rule.processName](bool value) {
            auto& store = core::Store::instance();
            std::optional<soundint::AppRule> found = store.findRule(name);
            if (found.has_value()) {
                found->enabled = value;
                store.upsertRule(*found);
                store.save();
            }
            requestRender();
        });

        row->remove = std::make_unique<controls::Button>();
        row->remove->setGlyph(tokens::kIconClose);
        row->remove->setAccessibleName(tr(L"settings.rules.remove"));
        row->remove->setOnClick([this, name = rule.processName]() {
            auto& store = core::Store::instance();
            store.removeRule(name);
            store.save();
            syncImpl();
            requestRender();
        });

        controls_.push_back(row->enabled.get());
        controls_.push_back(row->remove.get());
        rows_.push_back(std::move(row));
    }
}

void RulesPage::addRule()
{
    const std::wstring name = normalizeProcessName(field_.text());
    auto& store = core::Store::instance();
    if (name.empty()) {
        status_ = tr(L"settings.rules.invalid");
        requestRender();
        return;
    }

    soundint::AppRule rule;
    rule.enabled = true;
    rule.processName = name;   // deviceId vazio = padrao do sistema (sem audio aqui)
    rule.deviceId.clear();
    store.upsertRule(rule);
    store.save();

    field_.setText(L"");
    status_ = tr(L"settings.rules.added");
    syncImpl();
    requestRender();
}

void RulesPage::layoutImpl(const Rect& area, float /*scale*/)
{
    const auto& rules = core::Store::instance().rules();
    if (rows_.size() != rules.size()) {
        rebuildRows();   // store alterado por outro track
    }

    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float fieldH = s(tokens::kControlHeight);

    float y = y0;
    const float addW = s(110.f);
    field_.setBounds({area.x, y, area.w - addW - s(8.f), fieldH});
    addButton_.setBounds({area.x + area.w - addW, y, addW, fieldH});
    y += fieldH + s(8.f);

    hintRect_ = {area.x, y, area.w, s(20.f)};
    y += s(20.f) + s(4.f);
    statusRect_ = {area.x, y, area.w, s(20.f)};
    if (!status_.empty()) {
        y += s(20.f) + s(4.f);
    }

    empty_ = rules.empty();
    geoms_.clear();
    if (empty_) {
        emptyRect_ = {area.x, y + s(8.f), area.w, s(40.f)};
        y = emptyRect_.y + emptyRect_.h;
    } else {
        const float toggleW = s(48.f);
        const float controlH = s(32.f);
        const float inset = s(12.f);
        const float nameW = area.w * 0.42f;
        y += s(8.f);
        const size_t count = std::min(rules.size(), rows_.size());
        for (size_t i = 0; i < count; ++i) {
            RowGeom geom;
            geom.row = {area.x, y, area.w, rowH};
            geom.name = {area.x + inset, y, nameW - inset, rowH};
            geom.arrow = {area.x + nameW, y, s(20.f), rowH};
            const float toggleX = area.x + area.w - toggleW - s(44.f);
            geom.target = {area.x + nameW + s(24.f), y,
                           toggleX - (area.x + nameW + s(24.f)) - s(8.f), rowH};
            geom.toggle = {toggleX, y + (rowH - controlH) * 0.5f, toggleW, controlH};
            geom.remove = {area.x + area.w - controlH - s(4.f),
                           y + (rowH - controlH) * 0.5f, controlH, controlH};
            geoms_.push_back(geom);
            y += rowH + s(4.f);
        }
    }

    contentH_ = y - y0;
}

void RulesPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    drawText(rt, hintRect_, hint_, palette, tokens::kFontCaption,
             tokens::kWeightRegular, palette.textSecondary);
    if (!status_.empty()) {
        drawText(rt, statusRect_, status_, palette, tokens::kFontCaption,
                 tokens::kWeightRegular, palette.accent);
    }
    if (empty_) {
        drawText(rt, emptyRect_, tr(L"settings.rules.empty"), palette,
                 tokens::kFontBody, tokens::kWeightRegular, palette.textSecondary,
                 TextStyle::Align::Center);
        return;
    }

    const auto& rules = core::Store::instance().rules();
    for (size_t i = 0; i < geoms_.size() && i < rules.size(); ++i) {
        const RowGeom& geom = geoms_[i];
        const soundint::AppRule& rule = rules[i];

        rt.fillRoundedRect(geom.row, tokens::kRadiusMd * scale_, palette.surface);
        rt.strokeRoundedRect(geom.row, tokens::kRadiusMd * scale_, 1.f, palette.border);

        drawText(rt, geom.name, rule.processName, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textPrimary);
        drawText(rt, geom.arrow, L"\u2192", palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textSecondary, TextStyle::Align::Center);
        const std::wstring target =
            rule.deviceId.empty() ? std::wstring(tr(L"settings.rules.systemDefault"))
                                  : rule.deviceId;
        drawText(rt, geom.target, target, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textSecondary);
    }
}

}  // namespace

std::unique_ptr<SettingsPage> createRulesPage()
{
    return std::make_unique<RulesPage>();
}

}  // namespace soundint::ui::settings
