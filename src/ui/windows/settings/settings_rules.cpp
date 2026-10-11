// ============================================================================
// Pagina Regras (U3): linha [campo processo][Dropdown saida][Adicionar] no
// topo, lista com destino amigavel, status transitorio com desfazer.
//
// - Destino vem do Dropdown (host().outputs); "Padrao do sistema" (item 0)
//   limpa a regra. Nenhum deviceId cru digitado a mao.
// - Adicionar processo existente NUNCA zera o destino (upsert seguro): a
//   regra e atualizada preservando enabled/destino quando a escolha e
//   "Padrao do sistema".
// - Remocao some com a linha e oferece "Desfazer" (~5s) na linha de status.
// - NENHUM callback de clique destroi controles sincronamente (UAF): o
//   rebuild e adiado (rowsDirty_) para o proximo layout/render.
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "ui/windows/settings/settings_undo.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace soundint::ui::settings {
namespace {

// Larguras/alturas logicas (96 DPI) da linha de entrada e da lista.
constexpr float kAddW = 110.f;
constexpr float kDropdownW = 180.f;
constexpr float kStatusRowH = 28.f;
constexpr float kUndoW = 88.f;
constexpr float kToggleW = 48.f;
constexpr float kRemoveW = 32.f;
constexpr float kCaptionW = 44.f;

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
    Rect caption{};
    Rect toggle{};
    Rect remove{};
};

class RulesPage : public SettingsPage {
public:
    RulesPage()
    {
        titleKey_ = L"settings.rules.title";
        addButton_.setVariant(controls::Button::Variant::Primary);
        addButton_.setOnClick([this]() { addRule(); });
        undoButton_.setOnClick([this]() { undoClicked(); });
        outputDropdown_.setOnChanged([this](size_t index) { onOutputChanged(index); });
        addField(&field_);
        rebuildRows();
        refresh();
    }

protected:
    void retranslateImpl() override
    {
        processLabel_ = tr(L"settings.rules.processLabel");
        outputLabel_ = tr(L"settings.rules.outputLabel");
        field_.setPlaceholder(tr(L"settings.rules.placeholder"));
        addButton_.setText(tr(L"settings.rules.add"));
        undoButton_.setText(tr(L"common.undo"));
        outputDropdown_.setAccessibleName(outputLabel_);
        hint_ = tr(L"settings.rules.hint");
        refreshOutputChoices();
        rebuildRows();
    }

    void syncImpl() override
    {
        refreshOutputChoices();
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

    bool pointerImpl(const PointerEvent& e) override
    {
        // Empty state clicavel (CTA): foca o campo de processo.
        if (empty_ && e.kind == PointerKind::Down && e.button == MouseButton::Left &&
            emptyRect_.contains(e.x, e.y)) {
            focusProcessField();
            requestRender();
            return true;
        }
        return false;
    }

    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    void rebuildRows();
    void refreshOutputChoices();
    void onOutputChanged(size_t index);
    void addRule();
    void removeRule(const std::wstring& processName);
    void undoClicked();
    void focusProcessField();

    TextField field_;
    controls::Dropdown outputDropdown_;
    controls::Button addButton_;
    controls::Button undoButton_;
    PageStatus status_;
    OutputChoices choices_;

    std::vector<std::unique_ptr<RuleRow>> rows_;
    std::vector<RowGeom> geoms_;
    std::wstring selectedDeviceId_;

    std::wstring processLabel_;
    std::wstring outputLabel_;
    std::wstring hint_;

    Rect processLabelRect_{};
    Rect outputLabelRect_{};
    Rect statusRect_{};
    Rect emptyRect_{};
    bool empty_ = false;
    bool rowsDirty_ = false;
};

// ---------------------------------------------------------------------------
// Montagem das linhas
// ---------------------------------------------------------------------------
void RulesPage::rebuildRows()
{
    // Ordem de render: campos/acoes/desfazer, linhas da lista e, por ultimo,
    // o Dropdown (o popup flutuante precisa desenhar por cima das linhas).
    controls_.clear();
    controls_.push_back(&field_);
    controls_.push_back(&addButton_);
    controls_.push_back(&undoButton_);
    rows_.clear();

    const auto& rules = core::Store::instance().rules();
    for (const soundint::AppRule& rule : rules) {
        auto row = std::make_unique<RuleRow>();
        row->processName = rule.processName;

        std::wstring enabledName = tr(L"settings.rules.enabled");
        enabledName += L": ";
        enabledName += rule.processName;

        row->enabled = std::make_unique<controls::Toggle>();
        row->enabled->setValue(rule.enabled);
        row->enabled->setAccessibleName(enabledName);
        row->enabled->setOnChanged([name = rule.processName](bool value) {
            auto& store = core::Store::instance();
            std::optional<soundint::AppRule> found = store.findRule(name);
            if (found.has_value()) {
                found->enabled = value;   // preserva o deviceId atual
                store.upsertRule(*found);
                store.save();
            }
            requestRender();
        });

        std::wstring removeName = tr(L"settings.rules.remove");
        removeName += L": ";
        removeName += rule.processName;

        row->remove = std::make_unique<controls::Button>();
        row->remove->setGlyph(tokens::kIconClose);
        row->remove->setVariant(controls::Button::Variant::Danger);
        row->remove->setAccessibleName(removeName);
        // removeRule NAO destroi a linha aqui: so marca rowsDirty_ e o
        // rebuild acontece no proximo layout (destruir sincronamente dentro
        // de Button::click() seria UAF).
        row->remove->setOnClick([this, name = rule.processName]() {
            removeRule(name);
        });

        controls_.push_back(row->enabled.get());
        controls_.push_back(row->remove.get());
        rows_.push_back(std::move(row));
    }

    controls_.push_back(&outputDropdown_);
}

void RulesPage::refreshOutputChoices()
{
    std::vector<DeviceInfo> outputs;
    if (host().outputs) {
        outputs = host().outputs();
    }
    choices_.refresh(std::move(outputs), tr(L"settings.rules.systemDefault"));
    outputDropdown_.setItems(choices_.items());
    if (!selectedDeviceId_.empty() && choices_.indexForId(selectedDeviceId_) == 0) {
        selectedDeviceId_.clear();   // saida desconectada: volta ao padrao
    }
    outputDropdown_.setSelectedIndex(choices_.indexForId(selectedDeviceId_));
}

void RulesPage::onOutputChanged(size_t index)
{
    selectedDeviceId_ = choices_.idForIndex(index);   // 0 = padrao do sistema
}

// ---------------------------------------------------------------------------
// Acoes
// ---------------------------------------------------------------------------
void RulesPage::addRule()
{
    const std::wstring name = normalizeProcessName(field_.text());
    if (name.empty()) {
        status_.show(StatusKind::Danger, tr(L"settings.rules.invalid"));
        requestRender();
        return;
    }

    auto& store = core::Store::instance();
    std::optional<soundint::AppRule> existing = store.findRule(name);
    soundint::AppRule rule;
    if (existing.has_value()) {
        // Upsert seguro: preserva 'enabled' e NUNCA zera o destino atual
        // quando a escolha do Dropdown e "Padrao do sistema".
        rule = *existing;
        if (!selectedDeviceId_.empty()) {
            rule.deviceId = selectedDeviceId_;   // atualizacao explicita
        }
        status_.show(StatusKind::Info, tr(L"settings.rules.updated"));
    } else {
        rule.enabled = true;
        rule.deviceId = selectedDeviceId_;
        status_.show(StatusKind::Success, tr(L"settings.rules.added"));
    }
    rule.processName = name;
    store.upsertRule(rule);
    store.save();

    field_.setText(L"");
    rowsDirty_ = true;
    requestRender();
    focusProcessField();   // entrada rapida: proxima digitacao vai direto ao campo
}

void RulesPage::removeRule(const std::wstring& processName)
{
    auto& store = core::Store::instance();
    std::optional<soundint::AppRule> found = store.findRule(processName);
    if (!found.has_value()) {
        return;
    }
    const soundint::AppRule snapshot = *found;
    store.removeRule(processName);
    store.save();
    // O desfazer captura apenas o snapshot: sem ponteiro para a pagina.
    status_.show(StatusKind::Info, tr(L"settings.rules.removed"),
                 PageStatus::kUndoTimeoutMs, [snapshot]() {
                     core::Store::instance().upsertRule(snapshot);
                     core::Store::instance().save();
                 });
    rowsDirty_ = true;
    requestRender();
}

void RulesPage::undoClicked()
{
    if (!status_.hasUndo()) {
        return;
    }
    status_.runUndo();
    status_.show(StatusKind::Success, tr(L"settings.rules.restored"));
    rowsDirty_ = true;
    requestRender();
}

void RulesPage::focusProcessField()
{
    for (TextField* candidate : fields_) {
        candidate->setFocused(candidate == &field_);
    }
    // Higiene do foco de teclado: limpa flags de botoes para que Enter a
    // partir daqui submeta a regra, e nao um clique fantasma em botao velho.
    for (Control* control : controls_) {
        auto* interactive = dynamic_cast<controls::InteractiveControl*>(control);
        if (interactive != nullptr && interactive != &field_) {
            interactive->setFocused(false);
        }
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void RulesPage::layoutImpl(const Rect& area, float /*scale*/)
{
    // F3: rebuild adiado sob captura de mouse — rebuildRows destruiria a
    // linha cujo botao esta pressionado (UAF no Up). rowsDirty_ NAO e
    // limpo quando adiado: o proximo layout sem captura reconstrói (e a
    // checagem de tamanho roda de novo la).
    if (rowsDirty_ || rows_.size() != core::Store::instance().rules().size()) {
        if (!rebuildDeferred()) {
            rebuildRows();   // rowsDirty_ ou mutacao externa (mixer gravou regra)
            rowsDirty_ = false;
        }
    }
    const auto& rules = core::Store::instance().rules();

    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float fieldH = s(tokens::kControlHeight);
    const float addW = s(kAddW);
    const float dropW = s(kDropdownW);

    float y = y0;
    // Rotulos fixos acima dos campos (nao so placeholder).
    processLabelRect_ = {area.x, y, area.w - dropW - addW - s(16.f), s(16.f)};
    outputLabelRect_ = {area.x + area.w - dropW - addW - s(8.f), y, dropW, s(16.f)};
    y += s(16.f) + s(4.f);

    // Linha de entrada padronizada: [campo][Dropdown saida][Primary a direita]
    const float fieldW = std::max(s(120.f), area.w - dropW - addW - s(16.f));
    field_.setBounds({area.x, y, fieldW, fieldH});
    outputDropdown_.setBounds({area.x + fieldW + s(8.f), y, dropW, fieldH});
    if (outputDropdown_.expanded()) {
        // Popup precisa de bounds generosos (contrato do Dropdown).
        outputDropdown_.setBounds({area.x + fieldW + s(8.f), y, dropW,
                                   outputDropdown_.requiredHeight()});
    }
    addButton_.setBounds({area.x + area.w - addW, y, addW, fieldH});
    y += fieldH + s(4.f);

    // Linha de status: MESMA posicao sempre (reservada), texto transitorio
    // + "Desfazer" opcional a direita.
    statusRect_ = {area.x, y + s(4.f), area.w, s(20.f)};
    const bool hasUndo = status_.hasUndo();
    undoButton_.setVisible(hasUndo);
    if (hasUndo) {
        undoButton_.setBounds({area.x + area.w - s(kUndoW), y + s(2.f), s(kUndoW),
                               s(24.f)});
    }
    y += s(kStatusRowH);

    empty_ = rules.empty();
    geoms_.clear();
    if (empty_) {
        emptyRect_ = {area.x, y + s(8.f), area.w, s(48.f)};
        y = emptyRect_.y + emptyRect_.h;
    } else {
        const float controlH = s(32.f);
        const float removeW = s(kRemoveW);
        const float toggleW = s(kToggleW);
        const float captionW = s(kCaptionW);
        const float inset = s(12.f);
        y += s(4.f);
        const size_t count = std::min(rules.size(), rows_.size());
        for (size_t i = 0; i < count; ++i) {
            RowGeom geom;
            geom.row = {area.x, y, area.w, rowH};
            const float removeX = area.x + area.w - removeW - s(4.f);
            const float toggleX = removeX - s(12.f) - toggleW;
            const float captionX = toggleX - s(8.f) - captionW;
            const float nameW = (captionX - area.x) * 0.55f;
            geom.name = {area.x + inset, y, nameW - inset, rowH};
            geom.arrow = {area.x + nameW, y, s(20.f), rowH};
            geom.target = {area.x + nameW + s(24.f), y,
                           captionX - (area.x + nameW + s(24.f)) - s(8.f), rowH};
            geom.caption = {captionX, y, captionW, rowH};
            geom.toggle = {toggleX, y + (rowH - controlH) * 0.5f, toggleW, controlH};
            geom.remove = {removeX, y + (rowH - controlH) * 0.5f, removeW, controlH};
            // Os controles da linha SEMPRE recebem bounds (antes ficavam sem
            // setBounds: nunca eram clicaveis).
            rows_[i]->enabled->setBounds(geom.toggle);
            rows_[i]->remove->setBounds(geom.remove);
            geoms_.push_back(geom);
            y += rowH + s(4.f);
        }
    }

    contentH_ = y - y0;
}

// ---------------------------------------------------------------------------
// Pintura
// ---------------------------------------------------------------------------
void RulesPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    drawText(rt, processLabelRect_, processLabel_, palette, tokens::kFontCaption,
             tokens::kWeightRegular, palette.textSecondary);
    drawText(rt, outputLabelRect_, outputLabel_, palette, tokens::kFontCaption,
             tokens::kWeightRegular, palette.textSecondary);

    if (status_.active()) {
        const float undoW = status_.hasUndo() ? s(kUndoW) + s(8.f) : 0.f;
        const Rect textBox{statusRect_.x, statusRect_.y, statusRect_.w - undoW,
                           statusRect_.h};
        drawText(rt, textBox, status_.text(), palette, tokens::kFontBody,
                 tokens::kWeightRegular, statusColor(status_.kind(), palette));
    }

    if (empty_) {
        const Rect msgRect{emptyRect_.x, emptyRect_.y, emptyRect_.w, s(24.f)};
        const Rect hintRect{emptyRect_.x, emptyRect_.y + s(24.f), emptyRect_.w,
                            s(20.f)};
        // CTA: clique na area foca o campo de processo (ver pointerImpl).
        drawText(rt, msgRect, tr(L"settings.rules.empty"), palette,
                 tokens::kFontBody, tokens::kWeightRegular, palette.accent,
                 TextStyle::Align::Center);
        drawText(rt, hintRect, hint_, palette, tokens::kFontCaption,
                 tokens::kWeightRegular, palette.textSecondary,
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
                 tokens::kWeightRegular, palette.textSecondary,
                 TextStyle::Align::Center);
        drawText(rt, geom.target,
                 choices_.friendlyName(rule.deviceId,
                                       tr(L"settings.rules.systemDefault")),
                 palette, tokens::kFontBody, tokens::kWeightRegular,
                 palette.textSecondary);
        // Rotulo visivel do toggle da linha (antes havia so o icone mudo).
        drawText(rt, geom.caption, tr(L"settings.rules.active"), palette,
                 tokens::kFontCaption, tokens::kWeightRegular,
                 palette.textSecondary, TextStyle::Align::Right);
    }
}

}  // namespace

std::unique_ptr<SettingsPage> createRulesPage()
{
    return std::make_unique<RulesPage>();
}

}  // namespace soundint::ui::settings
