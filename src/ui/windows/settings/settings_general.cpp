// ============================================================================
// Pagina Geral (Track K): popups, bandeja, inicializacao, tema e idioma.
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"

namespace soundint::ui::settings {
namespace {

constexpr size_t kToggleCount = 5;   // 4 flags comuns + startWithWindows

using BoolSetting = bool soundint::Settings::*;

constexpr BoolSetting kFlags[4] = {
    &soundint::Settings::popupOnNewDevice,
    &soundint::Settings::popupOnNewApp,
    &soundint::Settings::rememberNewAppChoice,
    &soundint::Settings::closeToTray,
};

class GeneralPage : public SettingsPage {
public:
    GeneralPage()
    {
        titleKey_ = L"settings.general.title";
        for (size_t i = 0; i < kToggleCount; ++i) {
            controls_.push_back(&toggles_[i]);
        }
        controls_.push_back(&themeDropdown_);
        controls_.push_back(&languageDropdown_);
        wire();
        refresh();
    }

protected:
    void retranslateImpl() override;
    void syncImpl() override;
    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    void wire();

    controls::Toggle toggles_[kToggleCount];
    controls::Dropdown themeDropdown_;
    controls::Dropdown languageDropdown_;
    std::wstring labels_[kToggleCount];
    std::wstring themeLabel_;
    std::wstring languageLabel_;
    Rect labelRects_[kToggleCount]{};
    Rect themeLabelRect_{};
    Rect languageLabelRect_{};
};

void GeneralPage::wire()
{
    for (size_t i = 0; i < 4; ++i) {
        const BoolSetting flag = kFlags[i];
        toggles_[i].setOnChanged([flag](bool value) {
            auto& store = core::Store::instance();
            store.settings().*flag = value;
            store.save();
            requestRender();
        });
    }

    toggles_[4].setOnChanged([](bool value) {
        auto& store = core::Store::instance();
        store.settings().startWithWindows = value;
        const bool ok = applyStartup(value);
        if (!ok && value) {
            store.settings().startWithWindows = false;
        }
        store.save();
        requestRender();
    });

    themeDropdown_.setOnChanged([](size_t index) {
        auto& store = core::Store::instance();
        store.settings().theme = (index <= 2) ? static_cast<int>(index) : 0;
        store.save();
        requestRender();
    });

    languageDropdown_.setOnChanged([](size_t index) {
        static const wchar_t* kCodes[] = {L"auto", L"pt-BR", L"en"};
        const wchar_t* code = (index < 3) ? kCodes[index] : kCodes[0];
        auto& store = core::Store::instance();
        store.settings().language = code;
        store.save();
        setLanguage(code);
        notifyLanguageChanged();
    });
}

void GeneralPage::retranslateImpl()
{
    labels_[0] = tr(L"settings.general.popupOnNewDevice");
    labels_[1] = tr(L"settings.general.popupOnNewApp");
    labels_[2] = tr(L"settings.general.rememberNewAppChoice");
    labels_[3] = tr(L"settings.general.closeToTray");
    labels_[4] = tr(L"settings.general.startWithWindows");
    themeLabel_ = tr(L"settings.general.theme");
    languageLabel_ = tr(L"settings.general.language");

    themeDropdown_.setItems({tr(L"settings.general.theme.system"),
                             tr(L"settings.general.theme.light"),
                             tr(L"settings.general.theme.dark")});
    languageDropdown_.setItems({tr(L"settings.general.language.auto"),
                                tr(L"settings.general.language.pt"),
                                tr(L"settings.general.language.en")});
}

void GeneralPage::syncImpl()
{
    const soundint::Settings& settings = core::Store::instance().settings();
    toggles_[0].setValue(settings.popupOnNewDevice);
    toggles_[1].setValue(settings.popupOnNewApp);
    toggles_[2].setValue(settings.rememberNewAppChoice);
    toggles_[3].setValue(settings.closeToTray);
    toggles_[4].setValue(settings.startWithWindows);

    const int theme = (settings.theme >= 0 && settings.theme <= 2) ? settings.theme : 0;
    themeDropdown_.setSelectedIndex(static_cast<size_t>(theme));

    size_t languageIndex = 0;
    if (settings.language == L"pt-BR") {
        languageIndex = 1;
    } else if (settings.language == L"en") {
        languageIndex = 2;
    }
    languageDropdown_.setSelectedIndex(languageIndex);
}

void GeneralPage::layoutImpl(const Rect& area, float /*scale*/)
{
    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float toggleW = s(56.f);
    const float toggleH = s(tokens::kControlHeight);
    const float labelW = area.w - toggleW - s(16.f);

    float y = y0;
    for (size_t i = 0; i < kToggleCount; ++i) {
        labelRects_[i] = {area.x, y, labelW, rowH};
        toggles_[i].setBounds({area.x + area.w - toggleW, y + (rowH - toggleH) * 0.5f,
                               toggleW, toggleH});
        y += rowH;
    }

    y += s(8.f);
    const float fieldW = s(220.f);
    const float fieldH = static_cast<float>(tokens::kControlHeight);

    themeLabelRect_ = {area.x, y, area.w - fieldW - s(16.f), rowH};
    themeDropdown_.setBounds({area.x + area.w - fieldW, y + (rowH - fieldH) * 0.5f,
                              fieldW, fieldH});
    if (themeDropdown_.expanded()) {
        themeDropdown_.setBounds({area.x + area.w - fieldW, y + (rowH - fieldH) * 0.5f,
                                  fieldW, themeDropdown_.requiredHeight()});
    }
    y += rowH;

    y += s(8.f);
    languageLabelRect_ = {area.x, y, area.w - fieldW - s(16.f), rowH};
    languageDropdown_.setBounds({area.x + area.w - fieldW, y + (rowH - fieldH) * 0.5f,
                                 fieldW, fieldH});
    if (languageDropdown_.expanded()) {
        languageDropdown_.setBounds({area.x + area.w - fieldW, y + (rowH - fieldH) * 0.5f,
                                     fieldW, languageDropdown_.requiredHeight()});
    }
    y += rowH;

    contentH_ = y - y0;
}

void GeneralPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    for (size_t i = 0; i < kToggleCount; ++i) {
        drawText(rt, labelRects_[i], labels_[i], palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textPrimary);
    }
    drawText(rt, themeLabelRect_, themeLabel_, palette, tokens::kFontBody,
             tokens::kWeightRegular, palette.textPrimary);
    drawText(rt, languageLabelRect_, languageLabel_, palette, tokens::kFontBody,
             tokens::kWeightRegular, palette.textPrimary);
}

}  // namespace

std::unique_ptr<SettingsPage> createGeneralPage()
{
    return std::make_unique<GeneralPage>();
}

}  // namespace soundint::ui::settings
