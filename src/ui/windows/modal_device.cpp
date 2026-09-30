// ============================================================================
// Modal "novo dispositivo de audio" (Track I).
// ============================================================================
#include "ui/windows/modal_device.h"

#include "ui/controls/button.h"
#include "ui/i18n.h"

#include <algorithm>

namespace soundint::ui::modal {
namespace {

// Layout em px logicos (x dpiScale() na janela).
constexpr float kWidth = 360.f;
constexpr float kPad = tokens::kWindowMargin;          // 16
constexpr float kIconSize = 32.f;
constexpr float kHeaderHeight = 40.f;                  // linha do icone/titulo
constexpr float kTitleHeight = 20.f;
constexpr float kNameHeight = 18.f;
constexpr float kNameOffsetY = 22.f;                   // sob o titulo
constexpr float kRowGap = tokens::kSpace3;             // 12
constexpr float kCheckHeight = 24.f;
constexpr float kSecondaryWidth = 104.f;
constexpr float kPrimaryWidth = 176.f;
constexpr float kHeight = kPad + kHeaderHeight + kRowGap + kCheckHeight +
                          kRowGap + tokens::kControlHeight + kPad;   // 152

}  // namespace

ModalDeviceWindow::ModalDeviceWindow()
{
    addControl(remember_, false);
    addControl(secondary_, false);
    addControl(primary_, true);

    remember_.setOnChanged([this](bool) { invalidate(); });
    secondary_.setOnClick([this] { onCancelAction(); });
    primary_.setOnClick([this] { onPrimaryAction(); });
}

void ModalDeviceWindow::setDevice(const DeviceInfo& device)
{
    device_ = device;
    glyph_ = deviceGlyphFor(device.friendlyName);

    primary_.setText(tr(L"modal.newDevice.primary"));
    secondary_.setText(tr(L"modal.newDevice.secondary"));
    secondary_.setAccessibleName(tr(L"common.close"));
    remember_.setText(tr(L"modal.newDevice.remember"));
    remember_.setValue(true);   // "usar ao conectar" ligado por padrao
}

Size ModalDeviceWindow::logicalContentSize() const
{
    return {kWidth, kHeight};
}

void ModalDeviceWindow::layoutControls(float scale)
{
    const float width = kWidth * scale;
    const float pad = kPad * scale;

    iconRect_ = {pad, pad, kIconSize * scale, kIconSize * scale};
    const float textX = pad + (kIconSize + tokens::kSpace2) * scale;
    const float textW = (std::max)(0.f, width - textX - pad);
    titleRect_ = {textX, pad, textW, kTitleHeight * scale};
    nameRect_ = {textX, (kPad + kNameOffsetY) * scale, textW, kNameHeight * scale};

    const float checkY = (kPad + kHeaderHeight + kRowGap) * scale;
    remember_.setBounds({pad, checkY, width - pad * 2.f, kCheckHeight * scale});

    const float buttonY = checkY + (kCheckHeight + kRowGap) * scale;
    const float primaryX = width - pad - kPrimaryWidth * scale;
    const float secondaryX = primaryX - tokens::kSpace2 * scale - kSecondaryWidth * scale;
    primary_.setBounds(
        {primaryX, buttonY, kPrimaryWidth * scale, tokens::kControlHeight * scale});
    secondary_.setBounds(
        {secondaryX, buttonY, kSecondaryWidth * scale, tokens::kControlHeight * scale});
}

void ModalDeviceWindow::paint(IRenderTarget& rt, float scale,
                              const tokens::Palette& pal)
{
    rt.fillRoundedRect(iconRect_, tokens::kRadiusMd * scale, pal.surfaceAlt);

    TextStyle glyph;
    glyph.fontFamily = tokens::kFontIcons;
    glyph.fontSize = tokens::kFontSubtitle * scale;
    glyph.weight = tokens::kWeightRegular;
    glyph.color = pal.accent;
    glyph.align = TextStyle::Align::Center;
    glyph.valign = TextStyle::VAlign::Middle;
    rt.drawGlyph(glyph_, iconRect_, glyph);

    TextStyle title;
    title.fontFamily = tokens::kFontUi;
    title.fontSize = tokens::kFontSubtitle * scale;
    title.weight = tokens::kWeightSemibold;
    title.color = pal.textPrimary;
    title.align = TextStyle::Align::Left;
    title.valign = TextStyle::VAlign::Middle;
    rt.drawText(tr(L"modal.newDevice.title"), titleRect_, title);

    TextStyle name = title;
    name.fontSize = tokens::kFontBody * scale;
    name.weight = tokens::kWeightRegular;
    name.color = pal.textSecondary;
    rt.drawText(device_.friendlyName, nameRect_, name);
}

void ModalDeviceWindow::onPrimaryAction()
{
    if (host_ && host_->setDefaultDevice) {
        if (!host_->setDefaultDevice(device_) && host_->notify) {
            host_->notify(tr(L"modal.newDevice.title"), device_.friendlyName);
        }
    }
    if (remember_.value() && host_ && host_->upsertArrival) {
        ArrivalRule rule;
        rule.enabled = true;
        rule.setAsDefault = true;
        rule.deviceNamePattern = arrivalPatternFrom(device_);
        if (!rule.deviceNamePattern.empty()) {
            host_->upsertArrival(rule);
        }
    }
    requestClose();   // fecha apos a acao (sem checkbox extra)
}

void ModalDeviceWindow::onCancelAction()
{
    requestClose();
}

}  // namespace soundint::ui::modal
