// ============================================================================
// CONTRACT — i18n simples (chave -> string). Wave 0 fornece stub que devolve
// a propria chave; o agente de Settings/UI (Wave 2) implementa as tabelas
// pt-BR/en em src/ui/i18n.cpp e assets/i18n/.
// ============================================================================
#pragma once

namespace soundint::ui {

// Retorna a string da chave no idioma ativo (Settings::language).
// Chaves no formato "modal.newDevice.title".
const wchar_t* tr(const wchar_t* key);

// Troca o idioma ativo ("auto" resolve pelo locale do sistema).
void setLanguage(const wchar_t* languageCode);
const wchar_t* currentLanguage();

}  // namespace soundint::ui
