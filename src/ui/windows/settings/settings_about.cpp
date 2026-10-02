// ============================================================================
// Pagina Sobre (Track K): versao, licenca, repositorio, issues, logs e
// creditos. Aberturas via ShellExecuteW (shell32).
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"

#include <shellapi.h>

namespace soundint::ui::settings {
namespace {

constexpr wchar_t kRepoUrl[] = L"https://github.com/brunocsilva41/SoundInt";
constexpr wchar_t kLicenseUrl[] =
    L"https://github.com/brunocsilva41/SoundInt/blob/main/LICENSE";
constexpr wchar_t kIssuesUrl[] =
    L"https://github.com/brunocsilva41/SoundInt/issues/new";

// Icones locais (Segoe MDL2/Fluent): licenca=documento, repo=codigo,
// problema=aviso, logs=pasta.
constexpr wchar_t kIconLicense[] = L"\uE774";   // Document
constexpr wchar_t kIconRepo[] = L"\uE943";      // Code
constexpr wchar_t kIconIssue[] = L"\uE7BA";     // Warning
constexpr wchar_t kIconLogs[] = L"\uE8B7";      // OpenFolderHorizontal

void openUrl(const wchar_t* url)
{
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

class AboutPage : public SettingsPage {
public:
    AboutPage()
    {
        titleKey_ = L"settings.about.title";

        licenseButton_.setOnClick([]() { openUrl(kLicenseUrl); });
        repoButton_.setOnClick([]() { openUrl(kRepoUrl); });
        issueButton_.setOnClick([]() { openUrl(kIssuesUrl); });
        logsButton_.setOnClick([]() {
            const std::wstring dir = core::Store::instance().logDir();
            if (dir.empty()) {
                return;
            }
            const std::wstring params = L"/p,\"" + dir + L"\"";
            ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr,
                          SW_SHOWNORMAL);
        });

        licenseButton_.setGlyph(kIconLicense);
        repoButton_.setGlyph(kIconRepo);
        issueButton_.setGlyph(kIconIssue);
        logsButton_.setGlyph(kIconLogs);

        controls_.push_back(&licenseButton_);
        controls_.push_back(&repoButton_);
        controls_.push_back(&issueButton_);
        controls_.push_back(&logsButton_);
        refresh();
    }

protected:
    void retranslateImpl() override
    {
        version_ = tr(L"settings.about.version");
        licenseButton_.setText(tr(L"settings.about.license"));
        repoButton_.setText(tr(L"settings.about.repo"));
        issueButton_.setText(tr(L"settings.about.reportIssue"));
        logsButton_.setText(tr(L"settings.about.openLogs"));
        creditsTitle_ = tr(L"settings.about.credits");
        creditsText_ = tr(L"settings.about.creditsEarTrumpet");
    }

    void syncImpl() override
    {
        // Sem estado persistido: nada a ler do Store.
    }

    void layoutImpl(const Rect& area, float scale) override
    {
        (void)scale;
        const float y0 = area.y - scrollY_;
        const float buttonH = s(tokens::kControlHeight);
        const float gap = s(8.f);
        const float buttonW = (area.w - gap) * 0.5f;

        float y = y0;
        versionRect_ = {area.x, y, area.w, s(20.f)};
        y += s(20.f) + s(12.f);

        licenseButton_.setBounds({area.x, y, buttonW, buttonH});
        repoButton_.setBounds({area.x + buttonW + gap, y, buttonW, buttonH});
        y += buttonH + gap;

        issueButton_.setBounds({area.x, y, buttonW, buttonH});
        logsButton_.setBounds({area.x + buttonW + gap, y, buttonW, buttonH});
        y += buttonH + s(20.f);

        creditsTitleRect_ = {area.x, y, area.w, s(20.f)};
        y += s(20.f) + s(4.f);
        creditsTextRect_ = {area.x, y, area.w, s(20.f)};
        y += s(20.f);

        contentH_ = y - y0;
    }

    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override
    {
        drawText(rt, versionRect_, version_, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textSecondary);
        drawText(rt, creditsTitleRect_, creditsTitle_, palette, tokens::kFontBody,
                 tokens::kWeightSemibold, palette.textPrimary);
        drawText(rt, creditsTextRect_, creditsText_, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textSecondary);
    }

private:
    controls::Button licenseButton_;
    controls::Button repoButton_;
    controls::Button issueButton_;
    controls::Button logsButton_;

    std::wstring version_;
    std::wstring creditsTitle_;
    std::wstring creditsText_;

    Rect versionRect_{};
    Rect creditsTitleRect_{};
    Rect creditsTextRect_{};
};

}  // namespace

std::unique_ptr<SettingsPage> createAboutPage()
{
    return std::make_unique<AboutPage>();
}

}  // namespace soundint::ui::settings
