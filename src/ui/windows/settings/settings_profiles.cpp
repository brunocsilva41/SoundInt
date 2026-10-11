// ============================================================================
// Pagina Perfis (U3): criar/remover/aplicar perfis e editar rotas
// (processName -> deviceId) com Dropdown de saidas — nenhum deviceId cru
// digitado a mao. Aplicar depende do roteador (host().applyProfile).
//
// - Upsert seguro: recriar perfil com nome existente preserva as rotas e
//   avisa; nada e apagado em silencio.
// - "Criar" conduz ao proximo passo: seleciona o perfil e foca o campo de
//   processo. O editor de rotas fica SEMPRE visivel (sem reflow abrupto).
// - Remocoes (perfil e rota) oferecem "Desfazer" (~5s) na linha de status.
// - NENHUM callback de clique destroi controles sincronamente (UAF): o
//   rebuild e adiado (rowsDirty_) para o proximo layout/render.
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "ui/windows/settings/settings_undo.h"

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

// Larguras/alturas logicas (96 DPI) padronizadas com a pagina Regras.
constexpr float kAddW = 110.f;
constexpr float kApplyW = 90.f;
constexpr float kDropdownW = 180.f;
constexpr float kStatusRowH = 28.f;
constexpr float kUndoW = 88.f;
constexpr float kRemoveW = 32.f;
constexpr float kRouteRowH = 36.f;

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
    static constexpr size_t kNoProfile = static_cast<size_t>(-1);

    ProfilesPage()
    {
        titleKey_ = L"settings.profiles.title";
        createButton_.setVariant(controls::Button::Variant::Primary);
        createButton_.setOnClick([this]() { createProfile(); });
        addButton_.setVariant(controls::Button::Variant::Primary);
        addButton_.setOnClick([this]() { addOverride(); });
        undoProfileButton_.setOnClick([this]() { undoProfileClicked(); });
        undoRouteButton_.setOnClick([this]() { undoRouteClicked(); });
        routeDropdown_.setOnChanged([this](size_t index) {
            selectedRouteDeviceId_ = choices_.idForIndex(index);
        });
        addField(&nameField_);
        addField(&processField_);
        rebuildRows();
        refresh();
    }

protected:
    void retranslateImpl() override
    {
        nameLabel_ = tr(L"settings.profiles.nameLabel");
        routeProcessLabel_ = tr(L"settings.profiles.overrideProcess");
        routeOutputLabel_ = tr(L"settings.profiles.overrideDevice");
        nameField_.setPlaceholder(tr(L"settings.profiles.placeholder"));
        createButton_.setText(tr(L"settings.profiles.create"));
        processField_.setPlaceholder(tr(L"settings.profiles.overrideProcessPlaceholder"));
        addButton_.setText(tr(L"settings.profiles.addOverride"));
        undoProfileButton_.setText(tr(L"common.undo"));
        undoRouteButton_.setText(tr(L"common.undo"));
        routeDropdown_.setAccessibleName(routeOutputLabel_);
        hint_ = tr(L"settings.profiles.hint");
        selectHint_ = tr(L"settings.profiles.selectHint");
        section_ = tr(L"settings.profiles.overrides");
        refreshRouteChoices();
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
        refreshRouteChoices();
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
        if (processField_.focused()) {
            addOverride();
            return true;
        }
        return false;
    }

    bool pointerImpl(const PointerEvent& e) override
    {
        // Empty state clicavel (CTA): foca o campo de nome.
        if (empty_ && e.kind == PointerKind::Down && e.button == MouseButton::Left &&
            emptyRect_.contains(e.x, e.y)) {
            focusField(nameField_);
            requestRender();
            return true;
        }

        size_t hit = kNoProfile;
        for (size_t i = 0; i < profileGeoms_.size(); ++i) {
            if (profileGeoms_[i].contains(e.x, e.y)) {
                hit = i;
                break;
            }
        }

        if (e.kind == PointerKind::Move) {
            // Hover na lista de perfis (antes a selecao nao tinha feedback).
            if (hit != hoverProfile_) {
                hoverProfile_ = hit;
                requestRender();
            }
            return hit != kNoProfile;
        }
        if (e.kind == PointerKind::Leave) {
            if (hoverProfile_ != kNoProfile) {
                hoverProfile_ = kNoProfile;
                requestRender();
            }
            return false;
        }
        if (e.kind == PointerKind::Down && e.button == MouseButton::Left &&
            hit != kNoProfile) {
            const auto& profiles = core::Store::instance().profiles();
            if (hit < profiles.size() && selected_ != profiles[hit].name) {
                selected_ = profiles[hit].name;
                rowsDirty_ = true;   // rotas da selecao: rebuild adiado
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
    void refreshRouteChoices();
    void focusField(TextField& field);
    void placeUndo(const PageStatus& status, controls::Button& button,
                   const Rect& area, float y);

    void createProfile();
    void addOverride();
    void removeProfile(const std::wstring& name);
    void removeOverride(const std::wstring& processName);
    void applyProfileByName(const std::wstring& name);
    void undoProfileClicked();
    void undoRouteClicked();
    bool mutateSelected(const std::function<void(soundint::Profile&)>& mutate);
    size_t selectedRouteCount() const;

    TextField nameField_;
    TextField processField_;
    controls::Dropdown routeDropdown_;
    controls::Button createButton_;
    controls::Button addButton_;
    controls::Button undoProfileButton_;
    controls::Button undoRouteButton_;
    PageStatus profileStatus_;
    PageStatus routeStatus_;
    OutputChoices choices_;

    std::vector<std::unique_ptr<ProfileRow>> profileRows_;
    std::vector<std::unique_ptr<OverrideRow>> overrideRows_;

    std::wstring selected_;
    std::wstring selectedRouteDeviceId_;
    size_t hoverProfile_ = kNoProfile;

    std::wstring nameLabel_;
    std::wstring routeProcessLabel_;
    std::wstring routeOutputLabel_;
    std::wstring hint_;
    std::wstring selectHint_;
    std::wstring section_;

    std::vector<Rect> profileGeoms_;
    std::vector<Rect> overrideGeoms_;
    Rect nameLabelRect_{};
    Rect profileStatusRect_{};
    Rect emptyRect_{};
    Rect sectionRect_{};
    Rect routeProcessLabelRect_{};
    Rect routeOutputLabelRect_{};
    Rect routeStatusRect_{};
    Rect selectHintRect_{};
    bool empty_ = false;
    bool hasSelection_ = false;
    bool rowsDirty_ = false;
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

size_t ProfilesPage::selectedRouteCount() const
{
    for (const soundint::Profile& profile : core::Store::instance().profiles()) {
        if (profile.name == selected_) {
            return profile.overrides.size();
        }
    }
    return 0;
}

void ProfilesPage::createProfile()
{
    const std::wstring name = trim(nameField_.text());
    if (name.empty()) {
        profileStatus_.show(StatusKind::Danger, tr(L"settings.profiles.invalid"));
        requestRender();
        return;
    }

    auto& store = core::Store::instance();
    bool existed = false;
    for (const soundint::Profile& profile : store.profiles()) {
        if (profile.name == name) {
            existed = true;   // upsert seguro: NAO apaga as rotas existentes
            break;
        }
    }
    if (!existed) {
        soundint::Profile profile;
        profile.name = name;
        store.upsertProfile(profile);
        store.save();
        profileStatus_.show(StatusKind::Success, tr(L"settings.profiles.created"));
    } else {
        profileStatus_.show(StatusKind::Info, tr(L"settings.profiles.exists"));
    }

    // Conduz ao proximo passo: seleciona o perfil e foca o campo de processo.
    selected_ = name;
    nameField_.setText(L"");
    focusField(processField_);
    rowsDirty_ = true;
    requestRender();
}

void ProfilesPage::removeProfile(const std::wstring& name)
{
    auto& store = core::Store::instance();
    soundint::Profile snapshot;
    bool found = false;
    for (const soundint::Profile& profile : store.profiles()) {
        if (profile.name == name) {
            snapshot = profile;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }
    store.removeProfile(name);
    store.save();
    profileStatus_.show(StatusKind::Info, tr(L"settings.profiles.removed"),
                        PageStatus::kUndoTimeoutMs, [snapshot]() {
                            core::Store::instance().upsertProfile(snapshot);
                            core::Store::instance().save();
                        });
    if (selected_ == name) {
        selected_.clear();
    }
    rowsDirty_ = true;
    requestRender();
}

void ProfilesPage::removeOverride(const std::wstring& processName)
{
    soundint::Profile snapshot;
    bool found = false;
    for (const soundint::Profile& profile : core::Store::instance().profiles()) {
        if (profile.name == selected_) {
            snapshot = profile;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }
    const bool ok = mutateSelected([&processName](soundint::Profile& profile) {
        profile.overrides.erase(
            std::remove_if(profile.overrides.begin(), profile.overrides.end(),
                           [&processName](const soundint::AppRule& rule) {
                               return rule.processName == processName;
                           }),
            profile.overrides.end());
    });
    if (!ok) {
        return;
    }
    routeStatus_.show(StatusKind::Info, tr(L"settings.profiles.routeRemoved"),
                      PageStatus::kUndoTimeoutMs, [snapshot]() {
                          core::Store::instance().upsertProfile(snapshot);
                          core::Store::instance().save();
                      });
    rowsDirty_ = true;
    requestRender();
}

void ProfilesPage::applyProfileByName(const std::wstring& name)
{
    if (host().applyProfile) {
        host().applyProfile(name);
        profileStatus_.show(StatusKind::Success, tr(L"settings.profiles.applied"));
    } else {
        profileStatus_.show(StatusKind::Danger, tr(L"settings.profiles.noRouter"));
    }
    requestRender();
}

void ProfilesPage::addOverride()
{
    if (selected_.empty()) {
        routeStatus_.show(StatusKind::Danger, tr(L"settings.profiles.selectHint"));
        requestRender();
        return;
    }
    const std::wstring process = normalizeProcessName(processField_.text());
    if (process.empty()) {
        routeStatus_.show(StatusKind::Danger, tr(L"settings.profiles.invalidOverride"));
        requestRender();
        return;
    }
    const std::wstring device = selectedRouteDeviceId_;   // escolha do Dropdown
    const bool ok = mutateSelected([&process, &device](soundint::Profile& profile) {
        for (soundint::AppRule& rule : profile.overrides) {
            if (rule.processName == process) {
                rule.deviceId = device;   // atualizacao explicita do destino
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
        routeStatus_.show(StatusKind::Danger, tr(L"settings.profiles.selectHint"));
        requestRender();
        return;
    }
    processField_.setText(L"");
    routeStatus_.show(StatusKind::Success, tr(L"settings.profiles.routeSaved"));
    rowsDirty_ = true;
    requestRender();
    focusField(processField_);   // entrada rapida: proxima rota sem clicar de novo
}

void ProfilesPage::undoProfileClicked()
{
    if (!profileStatus_.hasUndo()) {
        return;
    }
    profileStatus_.runUndo();
    profileStatus_.show(StatusKind::Success, tr(L"settings.profiles.restored"));
    rowsDirty_ = true;
    requestRender();
}

void ProfilesPage::undoRouteClicked()
{
    if (!routeStatus_.hasUndo()) {
        return;
    }
    routeStatus_.runUndo();
    routeStatus_.show(StatusKind::Success, tr(L"settings.profiles.routeRestored"));
    rowsDirty_ = true;
    requestRender();
}

// ---------------------------------------------------------------------------
// Auxiliares
// ---------------------------------------------------------------------------
void ProfilesPage::focusField(TextField& field)
{
    for (TextField* candidate : fields_) {
        candidate->setFocused(candidate == &field);
    }
    // Higiene do foco de teclado: sem isso, Enter continuaria disparando o
    // ultimo botao clicado (ex.: Criar) em vez de agir sobre o campo.
    for (Control* control : controls_) {
        auto* interactive = dynamic_cast<controls::InteractiveControl*>(control);
        if (interactive != nullptr && interactive != &field) {
            interactive->setFocused(false);
        }
    }
}

void ProfilesPage::placeUndo(const PageStatus& status, controls::Button& button,
                             const Rect& area, float y)
{
    const bool visible = status.hasUndo();
    button.setVisible(visible);
    if (visible) {
        button.setBounds({area.x + area.w - s(kUndoW), y + s(2.f), s(kUndoW),
                          s(24.f)});
    }
}

void ProfilesPage::refreshRouteChoices()
{
    std::vector<DeviceInfo> outputs;
    if (host().outputs) {
        outputs = host().outputs();
    }
    choices_.refresh(std::move(outputs), tr(L"settings.rules.systemDefault"));
    routeDropdown_.setItems(choices_.items());
    if (!selectedRouteDeviceId_.empty() &&
        choices_.indexForId(selectedRouteDeviceId_) == 0) {
        selectedRouteDeviceId_.clear();   // saida desconectada: volta ao padrao
    }
    routeDropdown_.setSelectedIndex(choices_.indexForId(selectedRouteDeviceId_));
}

// ---------------------------------------------------------------------------
// Montagem das linhas
// ---------------------------------------------------------------------------
void ProfilesPage::rebuildRows()
{
    // Ordem de render: campos/acoes/desfazer, perfis, rotas e, por ultimo, o
    // Dropdown (o popup flutuante precisa desenhar por cima das linhas).
    controls_.clear();
    controls_.push_back(&nameField_);
    controls_.push_back(&processField_);
    controls_.push_back(&createButton_);
    controls_.push_back(&addButton_);
    controls_.push_back(&undoProfileButton_);
    controls_.push_back(&undoRouteButton_);
    profileRows_.clear();
    overrideRows_.clear();
    hoverProfile_ = kNoProfile;   // linhas novas: hit de hover revalidado no Move

    const auto& profiles = core::Store::instance().profiles();
    for (const soundint::Profile& profile : profiles) {
        auto row = std::make_unique<ProfileRow>();
        row->name = profile.name;

        std::wstring applyName = tr(L"common.apply");
        applyName += L": ";
        applyName += profile.name;
        row->apply = std::make_unique<controls::Button>();
        row->apply->setText(tr(L"common.apply"));
        row->apply->setAccessibleName(applyName);
        row->apply->setOnClick([this, name = profile.name]() {
            applyProfileByName(name);
        });

        std::wstring removeName = tr(L"settings.profiles.remove");
        removeName += L": ";
        removeName += profile.name;
        row->remove = std::make_unique<controls::Button>();
        row->remove->setGlyph(tokens::kIconClose);
        row->remove->setVariant(controls::Button::Variant::Danger);
        row->remove->setAccessibleName(removeName);
        // removeProfile so marca rowsDirty_: o rebuild acontece no proximo
        // layout (destruir a linha dentro de Button::click() seria UAF).
        row->remove->setOnClick([this, name = profile.name]() {
            removeProfile(name);
        });

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

                std::wstring removeName = tr(L"settings.profiles.removeOverride");
                removeName += L": ";
                removeName += rule.processName;
                row->remove = std::make_unique<controls::Button>();
                row->remove->setGlyph(tokens::kIconClose);
                row->remove->setVariant(controls::Button::Variant::Danger);
                row->remove->setAccessibleName(removeName);
                row->remove->setOnClick(
                    [this, name = rule.processName]() { removeOverride(name); });

                controls_.push_back(row->remove.get());
                overrideRows_.push_back(std::move(row));
            }
            break;
        }
    }

    controls_.push_back(&routeDropdown_);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void ProfilesPage::layoutImpl(const Rect& area, float /*scale*/)
{
    // F3: rebuild adiado sob captura de mouse — rebuildRows destruiria o
    // botao capturado (UAF no Up). rowsDirty_ NAO e limpo quando adiado:
    // o proximo layout sem captura reconstrói (checagens rodam de novo).
    if (rowsDirty_ ||
        profileRows_.size() != core::Store::instance().profiles().size() ||
        overrideRows_.size() != selectedRouteCount()) {
        if (!rebuildDeferred()) {
            rebuildRows();   // mutacao externa ou selecao trocada
            rowsDirty_ = false;
        }
    }

    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float fieldH = s(tokens::kControlHeight);
    const float routeRowH = s(kRouteRowH);
    const float createW = s(kAddW);
    const float dropW = s(kDropdownW);

    float y = y0;
    // Rotulo fixo acima do campo de nome (nao so placeholder).
    nameLabelRect_ = {area.x, y, area.w - createW - s(8.f), s(16.f)};
    y += s(16.f) + s(4.f);

    nameField_.setBounds({area.x, y, area.w - createW - s(8.f), fieldH});
    createButton_.setBounds({area.x + area.w - createW, y, createW, fieldH});
    y += fieldH + s(4.f);

    // Linha de status padronizada (mesma posicao: abaixo da linha de entrada).
    profileStatusRect_ = {area.x, y + s(4.f), area.w, s(20.f)};
    placeUndo(profileStatus_, undoProfileButton_, area, y);
    y += s(kStatusRowH);

    empty_ = core::Store::instance().profiles().empty();
    profileGeoms_.clear();
    hasSelection_ = !selected_.empty();
    overrideGeoms_.clear();

    if (empty_) {
        // Sem perfis: so a linha de criacao + empty state com CTA.
        emptyRect_ = {area.x, y + s(8.f), area.w, s(48.f)};
        y = emptyRect_.y + emptyRect_.h;
        processField_.setVisible(false);
        processField_.setFocused(false);
        routeDropdown_.setVisible(false);
        addButton_.setVisible(false);
        undoRouteButton_.setVisible(false);
        contentH_ = y - y0;
        return;
    }

    processField_.setVisible(true);
    routeDropdown_.setVisible(true);
    addButton_.setVisible(true);

    y += s(4.f);
    const float removeW = s(kRemoveW);
    const float applyW = s(kApplyW);
    const float controlH = s(32.f);
    const size_t count = std::min(core::Store::instance().profiles().size(),
                                  profileRows_.size());
    for (size_t i = 0; i < count; ++i) {
        profileGeoms_.push_back({area.x, y, area.w, rowH});
        // Aplicar/Remover SEMPRE posicionados (antes ficavam sem setBounds:
        // nunca eram clicaveis).
        const float removeX = area.x + area.w - removeW - s(4.f);
        const float applyX = removeX - s(8.f) - applyW;
        profileRows_[i]->remove->setBounds({removeX, y + (rowH - controlH) * 0.5f,
                                            removeW, controlH});
        profileRows_[i]->apply->setBounds({applyX, y + (rowH - controlH) * 0.5f,
                                           applyW, controlH});
        y += rowH + s(4.f);
    }

    // Editor de rotas SEMPRE visivel quando ha perfis (sem reflow abrupto).
    sectionRect_ = {area.x, y + s(4.f), area.w, s(24.f)};
    y = sectionRect_.y + sectionRect_.h + s(4.f);

    // Rotulos fixos das rotas (chaves ativas do contrato de i18n).
    const float routeFieldW = std::max(s(120.f), area.w - dropW - s(kAddW) - s(16.f));
    routeProcessLabelRect_ = {area.x, y, routeFieldW, s(16.f)};
    routeOutputLabelRect_ = {area.x + routeFieldW + s(8.f), y, dropW, s(16.f)};
    y += s(16.f) + s(4.f);

    processField_.setBounds({area.x, y, routeFieldW, fieldH});
    routeDropdown_.setBounds({area.x + routeFieldW + s(8.f), y, dropW, fieldH});
    if (routeDropdown_.expanded()) {
        routeDropdown_.setBounds({area.x + routeFieldW + s(8.f), y, dropW,
                                 routeDropdown_.requiredHeight()});
    }
    addButton_.setBounds({area.x + area.w - s(kAddW), y, s(kAddW), fieldH});
    y += fieldH + s(4.f);

    // Sem selecao o editor fica desabilitado + hint (nao some da tela).
    processField_.setEnabled(hasSelection_);
    routeDropdown_.setEnabled(hasSelection_);
    addButton_.setEnabled(hasSelection_);

    routeStatusRect_ = {area.x, y + s(4.f), area.w, s(20.f)};
    placeUndo(routeStatus_, undoRouteButton_, area, y);
    y += s(kStatusRowH);

    if (!hasSelection_) {
        selectHintRect_ = {area.x, y + s(4.f), area.w, s(20.f)};
        y += s(24.f);
    }

    for (size_t i = 0; i < overrideRows_.size(); ++i) {
        overrideGeoms_.push_back({area.x, y, area.w, routeRowH});
        const float removeX = area.x + area.w - removeW - s(4.f);
        overrideRows_[i]->remove->setBounds(
            {removeX, y + (routeRowH - s(28.f)) * 0.5f, removeW, s(28.f)});
        y += routeRowH + s(4.f);
    }

    contentH_ = y - y0;
}

// ---------------------------------------------------------------------------
// Pintura
// ---------------------------------------------------------------------------
void ProfilesPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    drawText(rt, nameLabelRect_, nameLabel_, palette, tokens::kFontCaption,
             tokens::kWeightRegular, palette.textSecondary);

    if (profileStatus_.active()) {
        const float undoW =
            profileStatus_.hasUndo() ? s(kUndoW) + s(8.f) : 0.f;
        const Rect textBox{profileStatusRect_.x, profileStatusRect_.y,
                           profileStatusRect_.w - undoW, profileStatusRect_.h};
        drawText(rt, textBox, profileStatus_.text(), palette, tokens::kFontBody,
                 tokens::kWeightRegular, statusColor(profileStatus_.kind(), palette));
    }

    if (empty_) {
        const Rect msgRect{emptyRect_.x, emptyRect_.y, emptyRect_.w, s(24.f)};
        const Rect hintRect{emptyRect_.x, emptyRect_.y + s(24.f), emptyRect_.w,
                            s(20.f)};
        // CTA: clique na area foca o campo de nome (ver pointerImpl).
        drawText(rt, msgRect, tr(L"settings.profiles.empty"), palette,
                 tokens::kFontBody, tokens::kWeightRegular, palette.accent,
                 TextStyle::Align::Center);
        drawText(rt, hintRect, hint_, palette, tokens::kFontCaption,
                 tokens::kWeightRegular, palette.textSecondary,
                 TextStyle::Align::Center);
        return;
    }

    for (size_t i = 0; i < profileGeoms_.size() && i < profileRows_.size(); ++i) {
        const Rect& row = profileGeoms_[i];
        const bool isSelected = (profileRows_[i]->name == selected_);
        const bool isHovered = (i == hoverProfile_);
        const Color fill = isSelected ? palette.surfaceAlt
                                      : (isHovered ? palette.hover : palette.surface);
        rt.fillRoundedRect(row, tokens::kRadiusMd * scale_, fill);
        rt.strokeRoundedRect(row, tokens::kRadiusMd * scale_, 1.f,
                             isSelected ? palette.accent : palette.border);

        const float nameW = row.w - s(kApplyW) - s(kRemoveW) - s(24.f);
        drawText(rt, {row.x + s(12.f), row.y, nameW, row.h}, profileRows_[i]->name,
                 palette, tokens::kFontBody,
                 isSelected ? tokens::kWeightSemibold : tokens::kWeightRegular,
                 isSelected ? palette.accent : palette.textPrimary);
    }

    // Titulo da secao com o perfil selecionado.
    std::wstring section = section_;
    if (hasSelection_) {
        section += L" \u2014 ";
        section += selected_;
    }
    drawText(rt, sectionRect_, section, palette, tokens::kFontSubtitle,
             tokens::kWeightSemibold, palette.textPrimary);

    drawText(rt, routeProcessLabelRect_, routeProcessLabel_, palette,
             tokens::kFontCaption, tokens::kWeightRegular, palette.textSecondary);
    drawText(rt, routeOutputLabelRect_, routeOutputLabel_, palette,
             tokens::kFontCaption, tokens::kWeightRegular, palette.textSecondary);

    if (routeStatus_.active()) {
        const float undoW = routeStatus_.hasUndo() ? s(kUndoW) + s(8.f) : 0.f;
        const Rect textBox{routeStatusRect_.x, routeStatusRect_.y,
                           routeStatusRect_.w - undoW, routeStatusRect_.h};
        drawText(rt, textBox, routeStatus_.text(), palette, tokens::kFontBody,
                 tokens::kWeightRegular, statusColor(routeStatus_.kind(), palette));
    }

    if (!hasSelection_) {
        drawText(rt, selectHintRect_, selectHint_, palette, tokens::kFontCaption,
                 tokens::kWeightRegular, palette.textSecondary);
    }

    for (size_t i = 0; i < overrideGeoms_.size() && i < overrideRows_.size(); ++i) {
        const Rect& row = overrideGeoms_[i];
        const OverrideRow& overrideRow = *overrideRows_[i];
        rt.fillRoundedRect(row, tokens::kRadiusSm * scale_, palette.surface);
        rt.strokeRoundedRect(row, tokens::kRadiusSm * scale_, 1.f, palette.border);

        // Processo -> destino com nome amigavel (antes: deviceId cru).
        const float removeW = s(kRemoveW) + s(4.f);
        const float textW = (row.w - removeW) * 0.5f;
        drawText(rt, {row.x + s(8.f), row.y, textW - s(8.f), row.h},
                 overrideRow.processName, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textPrimary);
        drawText(rt, {row.x + s(8.f) + textW, row.y, s(20.f), row.h}, L"\u2192",
                 palette, tokens::kFontBody, tokens::kWeightRegular,
                 palette.textSecondary, TextStyle::Align::Center);
        const float targetX = row.x + s(8.f) + textW + s(20.f);
        const float targetW = row.w - removeW - (targetX - row.x) - s(8.f);
        drawText(rt, {targetX, row.y, targetW, row.h},
                 choices_.friendlyName(overrideRow.deviceId,
                                       tr(L"settings.rules.systemDefault")),
                 palette, tokens::kFontBody, tokens::kWeightRegular,
                 palette.textSecondary);
    }
}

}  // namespace

std::unique_ptr<SettingsPage> createProfilesPage()
{
    return std::make_unique<ProfilesPage>();
}

}  // namespace soundint::ui::settings
