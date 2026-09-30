#include "ui/i18n.h"

#include <cwchar>
#include <map>
#include <mutex>
#include <string>

// STUB Wave 0 — o agente de Settings/UI (Wave 2) substitui por tabelas reais
// carregadas de assets/i18n/ (pt-BR e en), mantendo a mesma API.

namespace soundint::ui {

namespace {
std::mutex g_mutex;
std::wstring g_language = L"auto";
}  // namespace

const wchar_t* tr(const wchar_t* key)
{
    // Stub: devolve a propria chave (visivel apenas em dev).
    return key;
}

void setLanguage(const wchar_t* languageCode)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_language = languageCode ? languageCode : L"auto";
}

const wchar_t* currentLanguage()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_language.c_str();
}

}  // namespace soundint::ui
