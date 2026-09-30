// ============================================================================
// Modal "novo dispositivo de audio" (Track I).
// Glifo por tipo de dispositivo, acao primaria = tornar saida padrao e
// checkbox "ao conectar, usar automaticamente" (ArrivalRule).
// ============================================================================
#pragma once

#include "ui/controls/button.h"
#include "ui/windows/modal_common.h"

namespace soundint::ui::modal {

class ModalDeviceWindow : public ModalWindow {
public:
    ModalDeviceWindow();

    void setDevice(const DeviceInfo& device);

protected:
    Size logicalContentSize() const override;
    void layoutControls(float scale) override;
    void paint(IRenderTarget& rt, float scale, const tokens::Palette& pal) override;
    void onPrimaryAction() override;
    void onCancelAction() override;
    const wchar_t* windowClassName() const override { return L"SoundInt.ModalDevice"; }

private:
    DeviceInfo device_;
    std::wstring glyph_;

    Rect iconRect_{};
    Rect titleRect_{};
    Rect nameRect_{};

    CheckBox remember_;
    controls::Button secondary_;   // "Agora nao"
    AccentButton primary_;         // "Tornar saida padrao"
};

}  // namespace soundint::ui::modal
