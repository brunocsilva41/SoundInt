// ============================================================================
// Pagina Perfis (Track K): criar/remover/aplicar perfis e editar overrides
// (processName -> deviceId) de forma textual. Aplicar depende do roteador
// (host().applyProfile), injetado na integracao.
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace soundint::ui::settings {
namespace {

std::wstring trim(const std::wstring& text)
{
    static constexpr wchar_t kWs[] = L" \t\r\n";
    const size_t begin = text.find_first_not_of(kWs);
    if (begin == std::wstring::npos) {
        return L"";
    }
    const size_t end = text.find_last_not_of(kWs);
    return text.substr(begin, end - begin + 1);
}

struct ProfileRow {
    std::wstring name;
    std::unique_ptr<controls::Button> apply;
    std::unique_ptr<controls::Button> remove;
};

struct OverrideRow {
    std::wstring processName;
    std::wstring deviceId;
    std::unique_ptr<controls::Button> remove;
};

class ProfilesPage : public SettingsPage {
public:
    ProfilesPage()
    {
        titleKey_ = L"settings.profiles.title";
        createButton_.setOnClick([this]() { createProfile(); });
        addButton_.setOnClick([this]() { addOverride(); });
        controls_.push_back(&createButton_);
        controls_.push_back(&addButton_);
        addField(&nameField_);
        addField(&processField_);
        addField(&deviceField_);
        rebuildRows();
        refresh();
    }

protected:
    void retranslateImpl() override
    {
        nameField_.setPlaceholder(tr(L"settings.profiles.placeholder"));
        createButton_.setText(tr(L"settings.profiles.create"));
        processField_.setPlaceholder(tr(L"settings.profiles.overrideProcessPlaceholder"));
        deviceField_.setPlaceholder(tr(L"settings.profiles.overrideDevicePlaceholder"));
        addButton_.setText(tr(L"settings.profiles.addOverride"));
        hint_ = tr(L"settings.profiles.selectHint");
        section_ = tr(L"settings.profiles.overrides");
        status_.clear();
        rebuildRows();
    }

    void syncImpl() override
    {
        const auto& profiles = core::Store::instance().profiles();
        const bool found = std::any_of(
            profiles.begin(), profiles.end(),
            [this](const soundint::Profile& p) { return p.name == selected_; });
        if (!found) {
            selected_.clear();
        }
        rebuildRows();
    }

    bool charImpl(wchar_t c) override
    {
        if (c != L'\r') {
            return false;
        }
        if (nameField_.focused()) {
            createProfile();
            return true;
        }
        if (processField_.focused() || deviceField_.focused()) {
            addOverride();
            return true;
        }
        return false;
    }

    bool pointerImpl(const PointerEvent& e) override
    {
        if (e.kind != PointerKind::Down || e.button != MouseButton::Left) {
            return false;
        }
        for (size_t i = 0; i < profileGeoms_.size(); ++i) {
            if (!profileGeoms_[i].contains(e.x, e.y)) {
                continue;
            }
            const auto& profiles = core::Store::instance().profiles();
            if (i < profiles.size()) {
                selected_ = profiles[i].name;
                status_.clear();
                syncImpl();
                requestRender();
            }
            return true;
        }
        return false;
    }

    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    void rebuildRows();
    void createProfile();
    void addOverride();
    void removeProfile(const std::wstring& name);
    void applyProfileByName(const std::wstring& name);
    void removeOverride(const std::wstring& processName);
    bool mutateSelected(const std::function<void(soundint::Profile&)>& mutate);

    TextField nameField_;
    TextField processField_;
    TextField deviceField_;
    controls::Button createButton_;
    controls::Button addButton_;

    std::vector<std::unique_ptr<ProfileRow>> profileRows_;
    std::vector<std::unique_ptr<OverrideRow>> overrideRows_;

    std::wstring selected_;
    std::wstring status_;
    std::wstring hint_;
    std::wstring section_;

    std::vector<Rect> profileGeoms_;
    std::vector<Rect> overrideGeoms_;
    Rect emptyRect_{};
    Rect hintRect_{};
    Rect statusRect_{};
    Rect sectionRect_{};
    bool empty_ = false;
    bool hasSelection_ = false;
};

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------
bool ProfilesPage::mutateSelected(const std::function<void(soundint::Profile&)>& mutate)
{
    auto& store = core::Store::instance();
    for (const soundint::Profile& profile : store.profiles()) {
        if (profile.name != selected_) {
            continue;
        }
        soundint::Profile copy = profile;
        mutate(copy);
        store.upsertProfile(copy);
        store.save();
        return true;
    }
    return false;
}

void ProfilesPage::createProfile()
{
    const std::wstring name = trim(nameField_.text());
    if (name.empty()) {
        status_ = tr(L"settings.profiles.invalid");
        requestRender();
        return;
    }

    soundint::Profile profile;
    profile.name = name;   // ids/overrides vazios: a Wave 3 preenche os defaults
    auto& store = core::Store::instance();
    store.upsertProfile(profile);
    store.save();

    selected_ = name;
    nameField_.setText(L"");
    status_.clear();
    syncImpl();
    requestRender();
}

void ProfilesPage::removeProfile(const std::wstring& name)
{
    auto& store = core::Store::instance();
    store.removeProfile(name);
    store.save();
    if (selected_ == name) {
        selected_.clear();
    }
    syncImpl();
    requestRender();
}

void ProfilesPage::applyProfileByName(const std::wstring& name)
{
    if (host().applyProfile) {
        host().applyProfile(name);
        status_.clear();
    } else {
        status_ = tr(L"settings.profiles.noRouter");
    }
    requestRender();
}

void ProfilesPage::addOverride()
{
    if (selected_.empty()) {
        status_ = tr(L"settings.profiles.selectHint");
        requestRender();
        return;
    }
    const std::wstring process = normalizeProcessName(processField_.text());
    if (process.empty()) {
        status_ = tr(L"settings.profiles.invalidOverride");
        requestRender();
        return;
    }
    const std::wstring device = trim(deviceField_.text());
    const bool ok = mutateSelected([&process, &device](soundint::Profile& profile) {
        for (soundint::AppRule& rule : profile.overrides) {
            if (rule.processName == process) {
                rule.deviceId = device;
                rule.enabled = true;
                return;
            }
        }
        soundint::AppRule rule;
        rule.enabled = true;
        rule.processName = process;
        rule.deviceId = device;
        profile.overrides.push_back(rule);
    });
    if (!ok) {
        status_ = tr(L"settings.profiles.selectHint");
        requestRender();
        return;
    }

    processField_.setText(L"");
    deviceField_.setText(L"");
    status_.clear();
    syncImpl();
    requestRender();
}

void ProfilesPage::removeOverride(const std::wstring& processName)
{
    mutateSelected([&processName](soundint::Profile& profile) {
        profile.overrides.erase(
            std::remove_if(profile.overrides.begin(), profile.overrides.end(),
                           [&processName](const soundint::AppRule& rule) {
                               return rule.processName == processName;
                           }),
            profile.overrides.end());
    });
    syncImpl();
    requestRender();
}

// ---------------------------------------------------------------------------
// Composicao de linhas
// ---------------------------------------------------------------------------
void ProfilesPage::rebuildRows()
{
    controls_.clear();
    controls_.push_back(&createButton_);
    controls_.push_back(&addButton_);
    controls_.push_back(&nameField_);
    controls_.push_back(&processField_);
    controls_.push_back(&deviceField_);
    profileRows_.clear();
    overrideRows_.clear();

    const auto& profiles = core::Store::instance().profiles();
    for (const soundint::Profile& profile : profiles) {
        auto row = std::make_unique<ProfileRow>();
        row->name = profile.name;

        row->apply = std::make_unique<controls::Button>();
        row->apply->setText(tr(L"common.apply"));
        row->apply->setOnClick([this, name = profile.name]() {
            applyProfileByName(name);
        });

        row->remove = std::make_unique<controls::Button>();
        row->remove->setGlyph(tokens::kIconClose);
        row->remove->setAccessibleName(tr(L"settings.profiles.remove"));
        row->remove->setOnClick([this, name = profile.name]() { removeProfile(name); });

        controls_.push_back(row->apply.get());
        controls_.push_back(row->remove.get());
        profileRows_.push_back(std::move(row));
    }

    if (!selected_.empty()) {
        for (const soundint::Profile& profile : profiles) {
            if (profile.name != selected_) {
                continue;
            }
            for (const soundint::AppRule& rule : profile.overrides) {
                auto row = std::make_unique<OverrideRow>();
                row->processName = rule.processName;
                row->deviceId = rule.deviceId;
                row->remove = std::make_unique<controls::Button>();
                row->remove->setGlyph(tokens::kIconClose);
                row->remove->setAccessibleName(tr(L"settings.profiles.removeOverride"));
                row->remove->setOnClick(
                    [this, name = rule.processName]() { removeOverride(name); });
                controls_.push_back(row->remove.get());
                overrideRows_.push_back(std::move(row));
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void ProfilesPage::layoutImpl(const Rect& area, float scale)
{
    (void)scale;
    const auto& profiles = core::Store::instance().profiles();
    if (profileRows_.size() != profiles.size()) {
        rebuildRows();   // store alterado fora da pagina
    }

    const float y0 = area.y - scrollY_;
    const float fieldH = s(tokens::kControlHeight);
    const float rowH = s(tokens::kRowHeight);
    const float controlH = s(32.f);
    const float createW = s(110.f);

    float y = y0;
    nameField_.setBounds({area.x, y, area.w - createW - s(8.f), fieldH});
    createButton_.setBounds({area.x + area.w - createW, y, createW, fieldH});
    y += fieldH + s(8.f);

    statusRect_ = {area.x, y, area.w, s(20.f)};
    if (!status_.empty()) {
        y += s(20.f) + s(4.f);
    }

    empty_ = profiles.empty();
    profileGeoms_.clear();
    if (empty_) {
        emptyRect_ = {area.x, y + s(8.f), area.w, s(32.f)};
        y = emptyRect_.y + emptyRect_.h;
    } else {
        y += s(4.f);
        const size_t count = std::min(profiles.size(), profileRows_.size());
        for (size_t i = 0; i < count; ++i) {
            profileGeoms_.push_back({area.x, y, area.w, rowH});
            y += rowH + s(4.f);
        }
    }

    hasSelection_ = !selected_.empty();
    overrideGeoms_.clear();
    if (hasSelection_) {
        sectionRect_ = {area.x, y + s(8.f), area.w, s(24.f)};
        y = sectionRect_.y + sectionRect_.h + s(4.f);

        const float addW = s(110.f);
        const float fieldW = (area.w - addW - s(16.f)) * 0.5f;
        processField_.setBounds({area.x, y, fieldW, fieldH});
        deviceField_.setBounds({area.x + fieldW + s(8.f), y, fieldW, fieldH});
        addButton_.setBounds({area.x + area.w - addW, y, addW, fieldH});
        processField_.setVisible(true);
        deviceField_.setVisible(true);
        addButton_.setVisible(true);
        y += fieldH + s(8.f);

        for (size_t i = 0; i < overrideRows_.size(); ++i) {
            overrideGeoms_.push_back({area.x, y, area.w, controlH});
            y += controlH + s(4.f);
        }
    } else {
        processField_.setVisible(false);
        deviceField_.setVisible(false);
        addButton_.setVisible(false);
        if (!empty_) {
            hintRect_ = {area.x, y + s(8.f), area.w, s(20.f)};
            y = hintRect_.y + hintRect_.h;
        }
    }

    contentH_ = y - y0;
}

// ---------------------------------------------------------------------------
// Pintura
// ---------------------------------------------------------------------------
void ProfilesPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!status_.empty()) {
        drawText(rt, statusRect_, status_, palette, tokens::kFontCaption,
                 tokens::kWeightRegular, palette.accent);
    }

    if (empty_) {
        drawText(rt, emptyRect_, tr(L"settings.profiles.empty"), palette,
                 tokens::kFontBody, tokens::kWeightRegular, palette.textSecondary,
                 TextStyle::Align::Center);
        return;
    }

    for (size_t i = 0; i < profileGeoms_.size() && i < profileRows_.size(); ++i) {
        const Rect& row = profileGeoms_[i];
        const bool isSelected = (profileRows_[i]->name == selected_);
        rt.fillRoundedRect(row, tokens::kRadiusMd * scale_,
                           isSelected ? palette.surfaceAlt : palette.surface);
        rt.strokeRoundedRect(row, tokens::kRadiusMd * scale_, 1.f,
                             isSelected ? palette.accent : palette.border);

        const float nameW = row.w - s(140.f);
        drawText(rt, {row.x + s(12.f), row.y, nameW, row.h}, profileRows_[i]->name,
                 palette, tokens::kFontBody,
                 isSelected ? tokens::kWeightSemibold : tokens::kWeightRegular,
                 isSelected ? palette.accent : palette.textPrimary);
    }

    if (hasSelection_) {
        drawText(rt, sectionRect_, section_, palette, tokens::kFontSubtitle,
                 tokens::kWeightSemibold, palette.textPrimary);
        for (size_t i = 0; i < overrideGeoms_.size() && i < overrideRows_.size(); ++i) {
            const Rect& row = overrideGeoms_[i];
            rt.fillRoundedRect(row, tokens::kRadiusSm * scale_, palette.surface);
            rt.strokeRoundedRect(row, tokens::kRadiusSm * scale_, 1.f, palette.border);

            const OverrideRow& overrideRow = *overrideRows_[i];
            const float half = (row.w - s(48.f)) * 0.5f;
            drawText(rt, {row.x + s(8.f), row.y, half, row.h}, overrideRow.processName,
                     palette, tokens::kFontBody, tokens::kWeightRegular,
                     palette.textPrimary);
            const std::wstring target =
                overrideRow.deviceId.empty()
                    ? std::wstring(tr(L"settings.rules.systemDefault"))
                    : overrideRow.deviceId;
            drawText(rt, {row.x + s(8.f) + half, row.y, half - s(8.f), row.h}, target,
                     palette, tokens::kFontBody, tokens::kWeightRegular,
                     palette.textSecondary);
        }
    } else {
        drawText(rt, hintRect_, hint_, palette, tokens::kFontCaption,
                 tokens::kWeightRegular, palette.textSecondary);
    }
}

}  // namespace

std::unique_ptr<SettingsPage> createProfilesPage()
{
    return std::make_unique<ProfilesPage>();
}

}  // namespace soundint::ui::settings
