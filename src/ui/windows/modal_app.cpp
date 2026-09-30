// ============================================================================
// Modal "app comecou a tocar" (Track I).
// ============================================================================
#include "ui/windows/modal_app.h"

#include "core/store.h"
#include "ui/controls/button.h"
#include "ui/controls/dropdown.h"
#include "ui/i18n.h"

#include <algorithm>
#include <shellapi.h>

namespace soundint::ui::modal {
namespace {

// Layout em px logicos (x dpiScale() na janela).
constexpr float kWidth = 360.f;
constexpr float kPad = tokens::kWindowMargin;          // 16
constexpr float kIconSize = 32.f;
constexpr float kHeaderHeight = 40.f;
constexpr float kTitleHeight = 20.f;
constexpr float kNameHeight = 18.f;
constexpr float kNameOffsetY = 22.f;
constexpr float kLabelY = 64.f;                        // rotulo "Saida"
constexpr float kLabelHeight = 16.f;
constexpr float kDropY = 84.f;                         // campo do dropdown
constexpr float kRowGap = tokens::kSpace3;             // 12
constexpr float kCheckY = 128.f;
constexpr float kCheckHeight = 24.f;
constexpr float kButtonY = 164.f;
constexpr float kSecondaryWidth = 104.f;
constexpr float kPrimaryWidth = 176.f;
constexpr float kHeight = kButtonY + tokens::kControlHeight + kPad;   // 212

}  // namespace

ModalAppWindow::ModalAppWindow()
{
    addControl(dropdown_, false);
    addControl(remember_, false);
    addControl(secondary_, false);
    addControl(primary_, true);

    dropdown_.setOnChanged([this](size_t) { invalidate(); });
    remember_.setOnChanged([this](bool) { invalidate(); });
    secondary_.setOnClick([this] { onCancelAction(); });
    primary_.setOnClick([this] { onPrimaryAction(); });
}

ModalAppWindow::~ModalAppWindow()
{
    releaseIcon();
}

void ModalAppWindow::releaseIcon()
{
    if (icon_) {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }
}

void ModalAppWindow::setSession(const SessionInfo& session)
{
    session_ = session;
    releaseIcon();

    // Icone do executavel. Observacao: IRenderTarget (contrato Wave 0) nao
    // expoe desenho de HICON/bitmap — gap reportado; por ora o paint usa o
    // glifo kIconInfo como fallback e o HICON e liberado na destruicao.
    if (!session.processPath.empty()) {
        SHFILEINFOW info{};
        const DWORD_PTR ok =
            SHGetFileInfoW(session.processPath.c_str(), 0, &info,
                           static_cast<UINT>(sizeof(info)),
                           SHGFI_ICON | SHGFI_SMALLICON);
        if (ok != 0 && info.hIcon) {
            icon_ = info.hIcon;
        }
    }

    outputs_.clear();
    if (host_ && host_->outputs) {
        outputs_ = host_->outputs();
    }

    std::vector<std::wstring> items;
    items.reserve(outputs_.size() + 1);
    items.push_back(tr(L"modal.newApp.systemDefault"));
    for (const DeviceInfo& device : outputs_) {
        items.push_back(device.friendlyName);
    }
    dropdown_.setItems(std::move(items));

    std::wstring current;
    if (host_ && host_->currentAppDevice) {
        current = host_->currentAppDevice(session.pid);
    }
    dropdown_.setSelectedIndex(initialOutputIndex(outputs_, current));
    dropdown_.setExpanded(false);
    expandedSeen_ = false;

    // Estado padrao do "lembrar" vem das settings.
    remember_.setValue(core::Store::instance().settings().rememberNewAppChoice);

    primary_.setText(tr(L"common.apply"));
    secondary_.setText(tr(L"common.cancel"));
    secondary_.setAccessibleName(tr(L"common.cancel"));
    remember_.setText(tr(L"modal.newApp.remember"));
    dropdown_.setAccessibleName(tr(L"modal.newApp.outputLabel"));
    primary_.setAccessibleName(tr(L"common.apply"));
}

Size ModalAppWindow::logicalContentSize() const
{
    float height = kHeight;
    if (dropdown_.expanded()) {
        // A lista flutuante do Track E usa constantes de 32px que NAO escalam
        // com o DPI; traduz a extensao fisica da lista para px logicos.
        const float scale = (std::max)(dpiScale(), 0.01f);
        const float items = static_cast<float>(dropdown_.items().size());
        const float popupPixels = tokens::kControlHeight +
                                  items * controls::Dropdown::kItemHeight;
        const float slotPixels = tokens::kControlHeight * scale;
        const float gapPixels = tokens::kSpace2 * scale;
        height += (popupPixels - slotPixels + gapPixels) / scale;
    }
    return {kWidth, height};
}

void ModalAppWindow::layoutControls(float scale)
{
    const float width = kWidth * scale;
    const float pad = kPad * scale;

    iconRect_ = {pad, pad, kIconSize * scale, kIconSize * scale};
    const float textX = pad + (kIconSize + tokens::kSpace2) * scale;
    const float textW = (std::max)(0.f, width - textX - pad);
    titleRect_ = {textX, pad, textW, kTitleHeight * scale};
    nameRect_ = {textX, (kPad + kNameOffsetY) * scale, textW, kNameHeight * scale};

    labelRect_ = {pad, kLabelY * scale, width - pad * 2.f, kLabelHeight * scale};

    // Campo do dropdown: bounds GENEROSOS quando expandido (o popup flutuante
    // do Track E vive dentro dos bounds; ver follow-up no dropdown.h).
    const float slot = tokens::kControlHeight * scale;
    const float dropY = kDropY * scale;
    const float fieldBounds =
        (std::max)(slot, tokens::kControlHeight);   // >= 32 para medicao estavel
    dropdown_.setBounds({pad, dropY, width - pad * 2.f, fieldBounds});

    const bool expanded = dropdown_.expanded();
    float delta = 0.f;
    if (expanded) {
        const float needed = dropdown_.requiredHeight();
        dropdown_.setBounds(
            {pad, dropY, width - pad * 2.f, (std::max)(fieldBounds, needed)});
        delta = needed - slot + tokens::kSpace2 * scale;
    }

    const float checkY = kCheckY * scale + delta;
    remember_.setBounds({pad, checkY, width - pad * 2.f, kCheckHeight * scale});

    const float buttonY = kButtonY * scale + delta;
    const float primaryX = width - pad - kPrimaryWidth * scale;
    const float secondaryX = primaryX - tokens::kSpace2 * scale - kSecondaryWidth * scale;
    primary_.setBounds(
        {primaryX, buttonY, kPrimaryWidth * scale, tokens::kControlHeight * scale});
    secondary_.setBounds(
        {secondaryX, buttonY, kSecondaryWidth * scale, tokens::kControlHeight * scale});
}

void ModalAppWindow::paint(IRenderTarget& rt, float scale, const tokens::Palette& pal)
{
    rt.fillRoundedRect(iconRect_, tokens::kRadiusMd * scale, pal.surfaceAlt);

    // Fallback visual: sem API de desenho de HICON no contrato IRenderTarget.
    TextStyle glyph;
    glyph.fontFamily = tokens::kFontIcons;
    glyph.fontSize = tokens::kFontSubtitle * scale;
    glyph.weight = tokens::kWeightRegular;
    glyph.color = icon_ ? pal.accent : pal.textSecondary;
    glyph.align = TextStyle::Align::Center;
    glyph.valign = TextStyle::VAlign::Middle;
    rt.drawGlyph(tokens::kIconInfo, iconRect_, glyph);

    TextStyle title;
    title.fontFamily = tokens::kFontUi;
    title.fontSize = tokens::kFontSubtitle * scale;
    title.weight = tokens::kWeightSemibold;
    title.color = pal.textPrimary;
    title.align = TextStyle::Align::Left;
    title.valign = TextStyle::VAlign::Middle;
    rt.drawText(tr(L"modal.newApp.title"), titleRect_, title);

    TextStyle name = title;
    name.fontSize = tokens::kFontBody * scale;
    name.weight = tokens::kWeightRegular;
    name.color = pal.textSecondary;
    rt.drawText(session_.displayName.empty() ? session_.processName
                                             : session_.displayName,
                nameRect_, name);

    TextStyle label;
    label.fontFamily = tokens::kFontUi;
    label.fontSize = tokens::kFontCaption * scale;
    label.weight = tokens::kWeightSemibold;
    label.color = pal.textSecondary;
    label.align = TextStyle::Align::Left;
    label.valign = TextStyle::VAlign::Middle;
    rt.drawText(tr(L"modal.newApp.outputLabel"), labelRect_, label);
}

std::wstring ModalAppWindow::chosenDeviceId() const
{
    const size_t index = dropdown_.selectedIndex();
    if (index == controls::Dropdown::kNoIndex || index == 0) {
        return std::wstring{};   // "Padrao do sistema"
    }
    if (index - 1 >= outputs_.size()) {
        return std::wstring{};
    }
    return outputs_[index - 1].id;
}

std::wstring ModalAppWindow::chosenLabel() const
{
    const std::wstring id = chosenDeviceId();
    if (id.empty()) {
        return tr(L"modal.newApp.systemDefault");
    }
    for (const DeviceInfo& device : outputs_) {
        if (device.id == id) {
            return device.friendlyName;
        }
    }
    return tr(L"modal.newApp.systemDefault");
}

bool ModalAppWindow::preparePointerDown(controls::InteractiveControl* target)
{
    if (!dropdown_.expanded() || target == &dropdown_) {
        return false;
    }
    dropdown_.setExpanded(false);   // clique fora fecha a lista
    reframe();
    return true;                    // consome o Down (nao ativa outro controle)
}

void ModalAppWindow::syncState()
{
    if (!valid()) {
        return;
    }
    if (dropdown_.expanded() != expandedSeen_) {
        expandedSeen_ = dropdown_.expanded();
        reframe();   // recalcular altura + reposicionar (canto inferior direito)
    }
}

void ModalAppWindow::onPrimaryAction()
{
    const std::wstring target = chosenDeviceId();
    bool ok = true;
    if (host_ && host_->routeApp) {
        ok = host_->routeApp(session_.pid, target);
    }

    if (ok) {
        if (remember_.value() && host_ && host_->upsertRule) {
            AppRule rule;
            rule.enabled = true;
            rule.processName = normalizeProcessName(session_.processName);
            rule.deviceId = target;
            host_->upsertRule(rule);
        }
    } else if (host_ && host_->notify) {
        host_->notify(tr(L"modal.newApp.title"), chosenLabel());
    }
    requestClose();
}

void ModalAppWindow::onCancelAction()
{
    requestClose();
}

}  // namespace soundint::ui::modal
